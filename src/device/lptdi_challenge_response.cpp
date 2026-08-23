#include "re2dj/device/lptdi_challenge_response.h"

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

void StoreLittleEndian(std::uint32_t value,
                       std::uint8_t* destination)
{
    destination[0] = static_cast<std::uint8_t>(value);
    destination[1] = static_cast<std::uint8_t>(value >> 8);
    destination[2] = static_cast<std::uint8_t>(value >> 16);
    destination[3] = static_cast<std::uint8_t>(value >> 24);
}

}  // namespace

std::uint32_t AdvanceLptdiChallenge(std::uint32_t value)
{
    const std::uint32_t low = value & 0xffffu;
    const std::uint32_t high = value >> 16;
    std::uint32_t mixed = (low * 0x015au) & 0xffffu;
    if (high != 0)
    {
        mixed += (high * 0x4e35u) & 0xffffu;
    }
    return ((mixed & 0xffffu) << 16) + low * 0x4e35u + 1u;
}

LptdiTargetState ComputeLptdiChallengeMask(std::uint32_t seed)
{
    LptdiTargetState mask = {};
    const std::uint32_t first = AdvanceLptdiChallenge(seed);
    const std::uint32_t second = AdvanceLptdiChallenge(first);
    StoreLittleEndian(first, mask.data());
    StoreLittleEndian(second, mask.data() + 4);
    return mask;
}

LptdiTargetState EncodeLptdiTargetState(std::uint32_t seed,
                                        const LptdiTargetState& target_state)
{
    LptdiTargetState response = ComputeLptdiChallengeMask(seed);
    for (std::size_t index = 0; index < response.size(); ++index)
    {
        response[index] ^= target_state[index];
    }
    return response;
}

bool ParseLptdiTargetState(std::string_view text,
                           LptdiTargetState* target_state,
                           std::string* error)
{
    if (target_state == nullptr || error == nullptr)
    {
        return false;
    }
    if (text.size() != kLptdiTargetStateSize * 2)
    {
        *error = "LPTDI target state must contain exactly 16 hex digits";
        return false;
    }
    LptdiTargetState parsed = {};
    for (std::size_t index = 0; index < parsed.size(); ++index)
    {
        const int high = HexDigit(text[index * 2]);
        const int low = HexDigit(text[index * 2 + 1]);
        if (high < 0 || low < 0)
        {
            *error = "LPTDI target state contains a non-hex digit";
            return false;
        }
        parsed[index] = static_cast<std::uint8_t>((high << 4) | low);
    }
    *target_state = parsed;
    error->clear();
    return true;
}

}  // namespace re2dj::device
