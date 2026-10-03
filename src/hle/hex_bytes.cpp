#include "re2dj/hle/hex_bytes.h"

namespace re2dj::hle
{

std::string EncodeHexBytes(const std::vector<std::uint8_t>& bytes)
{
    constexpr char kDigits[] = "0123456789abcdef";
    std::string text;
    text.reserve(bytes.size() * 2);
    for (const std::uint8_t byte : bytes)
    {
        text.push_back(kDigits[byte >> 4]);
        text.push_back(kDigits[byte & 0x0f]);
    }
    return text;
}

bool DecodeHexBytes(std::string_view text, std::vector<std::uint8_t>* bytes)
{
    const auto digit = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    if (bytes == nullptr || text.size() % 2 != 0)
    {
        return false;
    }
    bytes->clear();
    bytes->reserve(text.size() / 2);
    for (std::size_t index = 0; index < text.size(); index += 2)
    {
        const int high = digit(text[index]);
        const int low = digit(text[index + 1]);
        if (high < 0 || low < 0)
        {
            bytes->clear();
            return false;
        }
        bytes->push_back(static_cast<std::uint8_t>((high << 4) | low));
    }
    return true;
}

}  // namespace re2dj::hle
