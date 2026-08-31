#include "re2dj/device/hardlock_api_descriptor.h"

#include <algorithm>

namespace re2dj::device
{
namespace
{

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

std::uint16_t ReadU16(std::span<const std::uint8_t> bytes, std::size_t offset)
{
    return static_cast<std::uint16_t>(bytes[offset]) |
           static_cast<std::uint16_t>(bytes[offset + 1] << 8);
}

std::uint32_t ReadU32(std::span<const std::uint8_t> bytes, std::size_t offset)
{
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

}  // namespace

bool ParseHardlockApiDescriptorHeader(
    std::span<const std::uint8_t> bytes,
    HardlockApiDescriptorHeader* header)
{
    if (header == nullptr || bytes.size() < kHardlockApiFixedHeaderSize)
    {
        return false;
    }

    HardlockApiDescriptorHeader parsed;
    std::copy_n(bytes.begin(), parsed.api_version.size(), parsed.api_version.begin());
    parsed.module_id = ReadU16(bytes, 0x06);
    parsed.module_address = ReadU16(bytes, 0x08);
    parsed.data_address = ReadU32(bytes, 0x12);
    parsed.block_count = ReadU16(bytes, 0x16);
    parsed.function = ReadU16(bytes, 0x18);
    parsed.status = ReadU16(bytes, 0x1a);
    parsed.remote = ReadU16(bytes, 0x1c);
    parsed.port = ReadU16(bytes, 0x1e);
    parsed.speed = ReadU16(bytes, 0x20);
    parsed.network_users = ReadU16(bytes, 0x22);
    std::copy_n(bytes.begin() + 0x24,
                parsed.id_reference.size(),
                parsed.id_reference.begin());
    std::copy_n(bytes.begin() + 0x2c,
                parsed.id_verify.size(),
                parsed.id_verify.begin());
    *header = parsed;
    return true;
}

bool ParseHardlockApiDescriptorTailWord(
    std::span<const std::uint8_t> bytes,
    std::uint16_t* tail_word)
{
    if (tail_word == nullptr || bytes.size() < kHardlockApiDescriptorSize)
    {
        return false;
    }
    *tail_word = ReadU16(bytes, kHardlockApiTailWordOffset);
    return true;
}

bool ParseHardlockApiTailWordHex(const std::string& hex,
                                 std::uint16_t* tail_word,
                                 std::string* error)
{
    if (tail_word == nullptr || error == nullptr)
    {
        return false;
    }
    if (hex.size() != 4)
    {
        *error = "Hardlock descriptor tail must contain exactly 4 hex digits";
        return false;
    }
    std::uint16_t parsed = 0;
    for (const char character : hex)
    {
        const int digit = HexDigit(character);
        if (digit < 0)
        {
            *error = "Hardlock descriptor tail contains a non-hex digit";
            return false;
        }
        parsed = static_cast<std::uint16_t>((parsed << 4) | digit);
    }
    *tail_word = parsed;
    return true;
}

}  // namespace re2dj::device
