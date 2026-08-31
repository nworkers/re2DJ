#include "re2dj/device/hardlock_450_response.h"

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

}  // namespace

bool ParseHardlock450Response(const std::string& hex,
                              Hardlock450Response* response,
                              std::string* error)
{
    if (response == nullptr || error == nullptr)
    {
        return false;
    }
    if (hex.size() != response->size() * 2)
    {
        *error = "Hardlock 0x450 response must contain exactly 12 hex digits";
        return false;
    }
    Hardlock450Response parsed = {};
    for (std::size_t index = 0; index < parsed.size(); ++index)
    {
        const int high = HexDigit(hex[index * 2]);
        const int low = HexDigit(hex[index * 2 + 1]);
        if (high < 0 || low < 0)
        {
            *error = "Hardlock 0x450 response contains a non-hex digit";
            return false;
        }
        parsed[index] = static_cast<std::uint8_t>((high << 4) | low);
    }
    *response = parsed;
    return true;
}

}  // namespace re2dj::device
