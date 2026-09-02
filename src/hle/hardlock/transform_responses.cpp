#include "re2dj/hle/hardlock/transform_responses.h"

#include <algorithm>

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

std::string DescribeLine(std::size_t line_number, const char* reason)
{
    return "Hardlock transform response line " + std::to_string(line_number) + ": " +
           reason;
}

}  // namespace

bool ParseHardlockTransformResponseTable(
    std::string_view text,
    std::vector<HardlockTransformResponseEntry>* entries,
    std::string* error)
{
    if (entries == nullptr || error == nullptr)
    {
        return false;
    }
    std::vector<HardlockTransformResponseEntry> parsed;
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
        if (FindHardlockTransformResponse(parsed, entry.input) != nullptr)
        {
            *error = DescribeLine(line_number, "duplicate input block");
            return false;
        }
        parsed.push_back(entry);
    }
    if (parsed.empty())
    {
        *error = "Hardlock transform response map contains no entries";
        return false;
    }
    *entries = std::move(parsed);
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

}  // namespace re2dj::hle::hardlock
