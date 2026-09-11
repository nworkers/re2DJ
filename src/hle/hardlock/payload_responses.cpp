#include "re2dj/hle/hardlock/payload_responses.h"

#include <algorithm>
#include <iterator>
#include <utility>

namespace re2dj::hle::hardlock
{
namespace
{

constexpr std::size_t kHexDigitsPerBlock = kHardlockTransformBlockSize * 2;
constexpr std::size_t kRecordHeaderSize = 4;

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

bool AnySpecified(const std::vector<std::uint8_t>& mask)
{
    return std::any_of(
        mask.begin(), mask.end(), [](std::uint8_t value) { return value != 0; });
}

bool BlockCountInRange(std::size_t block_count)
{
    return block_count >= kHardlockPayloadMinBlocks &&
           block_count <= kHardlockPayloadMaxBlocks;
}

// Every field must be exactly the row's size, except the optional add pair
// which is either empty or the row's size. The parser and the unpacker only
// build rows that satisfy this; the check keeps a hand-built row from reading
// out of bounds.
bool WellFormed(const HardlockPayloadResponseEntry& entry)
{
    if (!BlockCountInRange(entry.block_count))
    {
        return false;
    }
    const std::size_t size = entry.block_count * kHardlockTransformBlockSize;
    if (entry.input.size() != size || entry.input_mask.size() != size ||
        entry.output.size() != size || entry.output_mask.size() != size)
    {
        return false;
    }
    const bool add_empty = entry.output_add.empty() && entry.output_add_mask.empty();
    const bool add_sized =
        entry.output_add.size() == size && entry.output_add_mask.size() == size;
    return add_empty || add_sized;
}

void WriteU32(std::span<std::uint8_t> bytes, std::uint32_t value)
{
    for (std::size_t index = 0; index < kRecordHeaderSize; ++index)
    {
        bytes[index] = static_cast<std::uint8_t>((value >> (index * 8)) & 0xff);
    }
}

std::uint32_t ReadU32(std::span<const std::uint8_t> bytes)
{
    std::uint32_t value = 0;
    for (std::size_t index = 0; index < kRecordHeaderSize; ++index)
    {
        value |= static_cast<std::uint32_t>(bytes[index]) << (index * 8);
    }
    return value;
}

// Record fields in order: input, input mask, output, output mask, add,
// add mask.
std::size_t FieldOffset(std::size_t field)
{
    return kRecordHeaderSize + field * kHardlockPayloadMaxBytes;
}

}  // namespace

bool ValidateHardlockPayloadResponse(const HardlockPayloadResponseEntry& entry,
                                     std::string* error)
{
    if (error == nullptr)
    {
        return false;
    }
    if (!WellFormed(entry))
    {
        *error = "request row fields do not match its block count";
        return false;
    }
    if (entry.output_add_mask.empty())
    {
        error->clear();
        return true;
    }
    const std::size_t size = entry.output.size();
    // A byte is written, added, or kept — never two at once.
    for (std::size_t index = 0; index < size; ++index)
    {
        if (entry.output_mask[index] != 0 && entry.output_add_mask[index] != 0)
        {
            *error = "request row byte is both written and added";
            return false;
        }
    }
    // Every set add byte must sit in a fully-specified four-byte group, because
    // the add is a 32-bit little-endian word.
    for (std::size_t group = 0; group + kHardlockPayloadAddGroup <= size;
         group += kHardlockPayloadAddGroup)
    {
        std::size_t set = 0;
        for (std::size_t byte = 0; byte < kHardlockPayloadAddGroup; ++byte)
        {
            if (entry.output_add_mask[group + byte] != 0)
            {
                ++set;
            }
        }
        if (set != 0 && set != kHardlockPayloadAddGroup)
        {
            *error = "request row add group is only partly specified";
            return false;
        }
    }
    error->clear();
    return true;
}

bool ParseHardlockPayloadPattern(std::string_view token,
                                 std::vector<std::uint8_t>* bytes,
                                 std::vector<std::uint8_t>* mask,
                                 std::string* error)
{
    if (bytes == nullptr || mask == nullptr || error == nullptr)
    {
        return false;
    }
    if (token.size() % kHexDigitsPerBlock != 0)
    {
        *error = "request row must cover whole eight-byte blocks";
        return false;
    }
    if (!BlockCountInRange(token.size() / kHexDigitsPerBlock))
    {
        *error = "request row must cover 2 to 16 blocks";
        return false;
    }
    std::vector<std::uint8_t> parsed_bytes(token.size() / 2, 0);
    std::vector<std::uint8_t> parsed_mask(token.size() / 2, 0);
    for (std::size_t index = 0; index < parsed_bytes.size(); ++index)
    {
        const char high = token[index * 2];
        const char low = token[index * 2 + 1];
        if (high == '?' && low == '?')
        {
            continue;
        }
        const int high_value = HexDigit(high);
        const int low_value = HexDigit(low);
        if (high_value < 0 || low_value < 0)
        {
            *error = "request row byte must be two hex digits or ??";
            return false;
        }
        parsed_bytes[index] = static_cast<std::uint8_t>((high_value << 4) | low_value);
        parsed_mask[index] = 1;
    }
    // A row that specifies nothing would match, or write, every request of its
    // size, which is never what an external answer means.
    if (!AnySpecified(parsed_mask))
    {
        *error = "request row must specify at least one byte";
        return false;
    }
    *bytes = std::move(parsed_bytes);
    *mask = std::move(parsed_mask);
    error->clear();
    return true;
}

bool HardlockPayloadResponsesOverlap(const HardlockPayloadResponseEntry& first,
                                     const HardlockPayloadResponseEntry& second)
{
    if (!WellFormed(first) || !WellFormed(second) ||
        first.block_count != second.block_count)
    {
        return false;
    }
    for (std::size_t index = 0; index < first.input.size(); ++index)
    {
        if (first.input_mask[index] != 0 && second.input_mask[index] != 0 &&
            first.input[index] != second.input[index])
        {
            return false;
        }
    }
    return true;
}

const HardlockPayloadResponseEntry* FindHardlockPayloadResponse(
    const std::vector<HardlockPayloadResponseEntry>& entries,
    std::span<const std::uint8_t> payload)
{
    for (const HardlockPayloadResponseEntry& entry : entries)
    {
        if (!WellFormed(entry) || entry.input.size() != payload.size())
        {
            continue;
        }
        bool matches = true;
        for (std::size_t index = 0; index < payload.size(); ++index)
        {
            if (entry.input_mask[index] != 0 && entry.input[index] != payload[index])
            {
                matches = false;
                break;
            }
        }
        if (matches)
        {
            return &entry;
        }
    }
    return nullptr;
}

void ApplyHardlockPayloadResponse(const HardlockPayloadResponseEntry& entry,
                                  std::span<std::uint8_t> payload)
{
    if (!WellFormed(entry) || entry.output.size() != payload.size())
    {
        return;
    }
    for (std::size_t index = 0; index < payload.size(); ++index)
    {
        if (entry.output_mask[index] != 0)
        {
            payload[index] = entry.output[index];
        }
    }
    // Add groups are 32-bit little-endian words added with carry, matching the
    // API_CODE writeback that increments block 3's two DWORDs. A byte is never
    // both written and added, so order does not matter.
    if (entry.output_add_mask.size() != payload.size())
    {
        return;
    }
    for (std::size_t group = 0; group + kHardlockPayloadAddGroup <= payload.size();
         group += kHardlockPayloadAddGroup)
    {
        bool group_set = true;
        for (std::size_t byte = 0; byte < kHardlockPayloadAddGroup; ++byte)
        {
            if (entry.output_add_mask[group + byte] == 0)
            {
                group_set = false;
                break;
            }
        }
        if (!group_set)
        {
            continue;
        }
        std::uint32_t base = 0;
        std::uint32_t addend = 0;
        for (std::size_t byte = 0; byte < kHardlockPayloadAddGroup; ++byte)
        {
            base |= static_cast<std::uint32_t>(payload[group + byte]) << (byte * 8);
            addend |= static_cast<std::uint32_t>(entry.output_add[group + byte]) << (byte * 8);
        }
        const std::uint32_t sum = base + addend;
        for (std::size_t byte = 0; byte < kHardlockPayloadAddGroup; ++byte)
        {
            payload[group + byte] = static_cast<std::uint8_t>((sum >> (byte * 8)) & 0xff);
        }
    }
}

void PackHardlockPayloadResponse(const HardlockPayloadResponseEntry& entry,
                                 std::span<std::uint8_t> record)
{
    if (record.size() < kHardlockPayloadRecordSize)
    {
        return;
    }
    std::fill_n(record.begin(), kHardlockPayloadRecordSize, std::uint8_t{0});
    if (!WellFormed(entry))
    {
        return;
    }
    WriteU32(record, static_cast<std::uint32_t>(entry.block_count));
    // Fields 4 and 5 (add, add mask) stay zero when the row has no add token,
    // which unpacks back to an empty add pair.
    const std::vector<std::uint8_t>* const fields[] = {
        &entry.input, &entry.input_mask, &entry.output,
        &entry.output_mask, &entry.output_add, &entry.output_add_mask};
    for (std::size_t field = 0; field < std::size(fields); ++field)
    {
        std::copy(fields[field]->begin(),
                  fields[field]->end(),
                  record.begin() + static_cast<std::ptrdiff_t>(FieldOffset(field)));
    }
}

bool UnpackHardlockPayloadResponse(std::span<const std::uint8_t> record,
                                   HardlockPayloadResponseEntry* entry)
{
    if (entry == nullptr || record.size() < kHardlockPayloadRecordSize)
    {
        return false;
    }
    const std::uint32_t block_count = ReadU32(record);
    if (!BlockCountInRange(block_count))
    {
        return false;
    }
    const std::size_t size = block_count * kHardlockTransformBlockSize;
    const auto field = [&](std::size_t index) {
        const auto begin = record.begin() + static_cast<std::ptrdiff_t>(FieldOffset(index));
        return std::vector<std::uint8_t>(begin, begin + static_cast<std::ptrdiff_t>(size));
    };
    HardlockPayloadResponseEntry parsed;
    parsed.block_count = block_count;
    parsed.input = field(0);
    parsed.input_mask = field(1);
    parsed.output = field(2);
    parsed.output_mask = field(3);
    std::vector<std::uint8_t> add = field(4);
    std::vector<std::uint8_t> add_mask = field(5);
    // Keep the add pair only when it carries something, so a row without an add
    // token unpacks to empty add fields.
    if (AnySpecified(add_mask))
    {
        parsed.output_add = std::move(add);
        parsed.output_add_mask = std::move(add_mask);
    }
    // A row must specify at least one input byte and at least one output byte,
    // whether written or added.
    if (!AnySpecified(parsed.input_mask) ||
        (!AnySpecified(parsed.output_mask) && !AnySpecified(parsed.output_add_mask)))
    {
        return false;
    }
    std::string error;
    if (!ValidateHardlockPayloadResponse(parsed, &error))
    {
        return false;
    }
    *entry = std::move(parsed);
    return true;
}

}  // namespace re2dj::hle::hardlock
