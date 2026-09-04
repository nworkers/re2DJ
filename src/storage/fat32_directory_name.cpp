#include "re2dj/storage/fat32_directory_name.h"

#include <algorithm>
#include <array>
#include <utility>

namespace re2dj::storage
{
namespace
{

std::uint16_t ReadU16(const std::uint8_t* bytes, std::size_t offset)
{
    return static_cast<std::uint16_t>(bytes[offset]) |
           (static_cast<std::uint16_t>(bytes[offset + 1]) << 8);
}

std::string Utf16ToUtf8(const std::vector<std::uint16_t>& code_units)
{
    std::string result;
    for (std::size_t index = 0; index < code_units.size(); ++index)
    {
        std::uint32_t code_point = code_units[index];
        // A name shorter than the slots can hold ends with a null; the space
        // after it is filled with 0xffff.
        if (code_point == 0x0000 || code_point == 0xffff)
        {
            break;
        }
        if (code_point >= 0xd800 && code_point <= 0xdbff && index + 1 < code_units.size())
        {
            const std::uint32_t low = code_units[index + 1];
            if (low >= 0xdc00 && low <= 0xdfff)
            {
                code_point = 0x10000 + ((code_point - 0xd800) << 10) + (low - 0xdc00);
                ++index;
            }
        }
        if (code_point < 0x80)
        {
            result.push_back(static_cast<char>(code_point));
        }
        else if (code_point < 0x800)
        {
            result.push_back(static_cast<char>(0xc0 | (code_point >> 6)));
            result.push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
        }
        else if (code_point < 0x10000)
        {
            result.push_back(static_cast<char>(0xe0 | (code_point >> 12)));
            result.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
        }
        else
        {
            result.push_back(static_cast<char>(0xf0 | (code_point >> 18)));
            result.push_back(static_cast<char>(0x80 | ((code_point >> 12) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
        }
    }
    return result;
}

// The three character fields of a long-name slot, as {offset, character count}.
// They hold 5, 6, and 2 characters, thirteen in all, and the last one ends at
// the slot's final byte: reading further crosses into the next entry.
void AppendLongNameSlot(const std::uint8_t* entry, std::vector<std::uint16_t>* part)
{
    constexpr std::array<std::pair<std::size_t, std::size_t>, 3> ranges = {
        {{1, 5}, {14, 6}, {28, 2}}};
    for (const auto [offset, count] : ranges)
    {
        for (std::size_t index = 0; index < count; ++index)
        {
            part->push_back(ReadU16(entry, offset + index * 2));
        }
    }
}

}  // namespace

std::string DecodeFatShortName(const std::uint8_t* entry)
{
    std::string base(reinterpret_cast<const char*>(entry), 8);
    std::string extension(reinterpret_cast<const char*>(entry + 8), 3);
    while (!base.empty() && base.back() == ' ')
    {
        base.pop_back();
    }
    while (!extension.empty() && extension.back() == ' ')
    {
        extension.pop_back();
    }
    if (!extension.empty())
    {
        base.push_back('.');
        base.append(extension);
    }
    return base;
}

std::uint8_t FatShortNameChecksum(const std::uint8_t* short_entry)
{
    std::uint8_t checksum = 0;
    for (std::size_t index = 0; index < 11; ++index)
    {
        checksum = static_cast<std::uint8_t>(((checksum & 1) << 7) +
                                             (checksum >> 1) + short_entry[index]);
    }
    return checksum;
}

void FatLongNameAssembler::Clear()
{
    parts_.clear();
    checksum_ = 0;
    has_last_ = false;
}

void FatLongNameAssembler::Add(const std::uint8_t* entry)
{
    const std::uint8_t order = entry[0];
    const std::uint8_t sequence = order & 0x1f;
    if (sequence == 0 || (order & 0x80) != 0 || (parts_.empty() && (order & 0x40) == 0))
    {
        Clear();
    }
    if (parts_.empty())
    {
        checksum_ = entry[13];
        has_last_ = (order & 0x40) != 0;
    }
    else if (entry[13] != checksum_)
    {
        Clear();
        return;
    }
    std::vector<std::uint16_t> part;
    AppendLongNameSlot(entry, &part);
    parts_.emplace_back(sequence, std::move(part));
}

bool FatLongNameAssembler::Decode(const std::uint8_t* short_entry, std::string* name) const
{
    if (!has_last_ || parts_.empty() || name == nullptr ||
        FatShortNameChecksum(short_entry) != checksum_)
    {
        return false;
    }
    std::vector<std::uint16_t> units;
    for (std::uint8_t sequence = 1; sequence <= parts_.size(); ++sequence)
    {
        const auto found = std::find_if(parts_.begin(),
                                        parts_.end(),
                                        [sequence](const auto& part)
                                        { return part.first == sequence; });
        if (found == parts_.end())
        {
            return false;
        }
        units.insert(units.end(), found->second.begin(), found->second.end());
    }
    *name = Utf16ToUtf8(units);
    return !name->empty();
}

}  // namespace re2dj::storage

