#include "re2dj/hle/hardlock/transform_responses.h"

#include <algorithm>
#include <utility>

namespace re2dj::hle::hardlock
{
namespace
{

constexpr std::size_t kHexDigitsPerBlock = kHardlockTransformBlockSize * 2;

int HexDigit(char value)
{
    if (value >= '0' && value <= '9')
    {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f')
    {
        return value - 'a' + 10;
    }
    if (value >= 'A' && value <= 'F')
    {
        return value - 'A' + 10;
    }
    return -1;
}

bool IsSpace(char value)
{
    return value == ' ' || value == '\t' || value == '\r';
}

bool ParseBlock(std::string_view token, HardlockTransformBlock* block)
{
    if (token.size() != kHexDigitsPerBlock)
    {
        return false;
    }
    for (std::size_t index = 0; index < block->size(); ++index)
    {
        const int high = HexDigit(token[index * 2]);
        const int low = HexDigit(token[index * 2 + 1]);
        if (high < 0 || low < 0)
        {
            return false;
        }
        (*block)[index] = static_cast<std::uint8_t>((high << 4) | low);
    }
    return true;
}

// Returns the next whitespace-delimited token and advances the cursor past it.
std::string_view NextToken(std::string_view line, std::size_t* cursor)
{
    while (*cursor < line.size() && IsSpace(line[*cursor]))
    {
        ++*cursor;
    }
    const std::size_t begin = *cursor;
    while (*cursor < line.size() && !IsSpace(line[*cursor]))
    {
        ++*cursor;
    }
    return line.substr(begin, *cursor - begin);
}

std::string DescribeLine(std::size_t line_number, std::string_view reason)
{
    return "Hardlock transform response line " + std::to_string(line_number) + ": " +
           std::string(reason);
}

bool ParseBlockRow(std::string_view input_token,
                   std::string_view output_token,
                   std::size_t line_number,
                   HardlockTransformResponseMap* map,
                   std::string* error)
{
    HardlockTransformResponseEntry entry;
    if (!ParseBlock(input_token, &entry.input))
    {
        *error = DescribeLine(line_number, "input must be 16 hex digits");
        return false;
    }
    if (!ParseBlock(output_token, &entry.output))
    {
        *error = DescribeLine(line_number, "output must be 16 hex digits");
        return false;
    }
    if (FindHardlockTransformResponse(map->blocks, entry.input) != nullptr)
    {
        *error = DescribeLine(line_number, "duplicate input block");
        return false;
    }
    map->blocks.push_back(entry);
    return true;
}

bool ParseRequestRow(std::string_view input_token,
                     std::string_view output_token,
                     std::size_t line_number,
                     HardlockTransformResponseMap* map,
                     std::string* error)
{
    HardlockPayloadResponseEntry entry;
    std::string pattern_error;
    if (!ParseHardlockPayloadPattern(
            input_token, &entry.input, &entry.input_mask, &pattern_error))
    {
        *error = DescribeLine(line_number, "input: " + pattern_error);
        return false;
    }
    if (!ParseHardlockPayloadPattern(
            output_token, &entry.output, &entry.output_mask, &pattern_error))
    {
        *error = DescribeLine(line_number, "output: " + pattern_error);
        return false;
    }
    entry.block_count = entry.input.size() / kHardlockTransformBlockSize;
    for (const HardlockPayloadResponseEntry& existing : map->payloads)
    {
        if (HardlockPayloadResponsesOverlap(existing, entry))
        {
            *error = DescribeLine(line_number,
                                  "request row can match the same payload as an earlier row");
            return false;
        }
    }
    map->payloads.push_back(std::move(entry));
    return true;
}

}  // namespace

bool ParseHardlockTransformResponseMap(std::string_view text,
                                       HardlockTransformResponseMap* map,
                                       std::string* error)
{
    if (map == nullptr || error == nullptr)
    {
        return false;
    }
    HardlockTransformResponseMap parsed;
    std::size_t line_number = 0;
    std::size_t position = 0;
    while (position <= text.size())
    {
        const std::size_t end = std::min(text.find('\n', position), text.size());
        const std::string_view line = text.substr(position, end - position);
        position = end + 1;
        ++line_number;

        std::size_t cursor = 0;
        const std::string_view input_token = NextToken(line, &cursor);
        if (input_token.empty() || input_token.front() == '#')
        {
            continue;
        }
        const std::string_view output_token = NextToken(line, &cursor);
        const std::string_view trailing = NextToken(line, &cursor);
        if (!trailing.empty() && trailing.front() != '#')
        {
            *error = DescribeLine(line_number, "unexpected extra token");
            return false;
        }

        // Existing maps consist entirely of sixteen-digit rows, so their
        // reading is unchanged.
        const bool block_row =
            input_token.size() == kHexDigitsPerBlock && output_token.size() == kHexDigitsPerBlock;
        if (block_row)
        {
            if (!ParseBlockRow(input_token, output_token, line_number, &parsed, error))
            {
                return false;
            }
            continue;
        }
        if (input_token.size() != output_token.size())
        {
            *error = DescribeLine(line_number, "input and output must be the same length");
            return false;
        }
        if (!ParseRequestRow(input_token, output_token, line_number, &parsed, error))
        {
            return false;
        }
    }
    if (parsed.empty())
    {
        *error = "Hardlock transform response map contains no entries";
        return false;
    }
    *map = std::move(parsed);
    error->clear();
    return true;
}

const HardlockTransformBlock* FindHardlockTransformResponse(
    const std::vector<HardlockTransformResponseEntry>& entries,
    const HardlockTransformBlock& input)
{
    for (const HardlockTransformResponseEntry& entry : entries)
    {
        if (entry.input == input)
        {
            return &entry.output;
        }
    }
    return nullptr;
}

void PackHardlockTransformResponseMap(const HardlockTransformResponseMap& map,
                                      std::vector<std::uint8_t>* block_rows,
                                      std::vector<std::uint8_t>* payload_records)
{
    if (block_rows == nullptr || payload_records == nullptr)
    {
        return;
    }
    block_rows->clear();
    block_rows->reserve(map.blocks.size() * kHardlockTransformBlockSize * 2);
    for (const HardlockTransformResponseEntry& entry : map.blocks)
    {
        block_rows->insert(block_rows->end(), entry.input.begin(), entry.input.end());
        block_rows->insert(block_rows->end(), entry.output.begin(), entry.output.end());
    }
    payload_records->assign(map.payloads.size() * kHardlockPayloadRecordSize, 0);
    for (std::size_t index = 0; index < map.payloads.size(); ++index)
    {
        PackHardlockPayloadResponse(
            map.payloads[index],
            std::span<std::uint8_t>(*payload_records)
                .subspan(index * kHardlockPayloadRecordSize, kHardlockPayloadRecordSize));
    }
}

}  // namespace re2dj::hle::hardlock
