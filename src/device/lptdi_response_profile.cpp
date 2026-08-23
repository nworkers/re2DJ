#include "re2dj/device/lptdi_response_profile.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string_view>
#include <utility>

namespace re2dj::device
{

namespace
{

std::string_view Trim(std::string_view text)
{
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0)
    {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0)
    {
        text.remove_suffix(1);
    }
    return text;
}

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

bool ParseControlCode(std::string_view text, std::uint32_t* value)
{
    text = Trim(text);
    if (text.size() < 3 || text[0] != '0' || (text[1] != 'x' && text[1] != 'X'))
    {
        return false;
    }
    text.remove_prefix(2);
    if (text.empty() || text.size() > 8)
    {
        return false;
    }
    std::uint32_t parsed = 0;
    for (const char character : text)
    {
        const int digit = HexDigit(character);
        if (digit < 0)
        {
            return false;
        }
        parsed = (parsed << 4) | static_cast<std::uint32_t>(digit);
    }
    *value = parsed;
    return true;
}

bool ParseBytes(std::string_view text, std::vector<std::uint8_t>* bytes)
{
    text = Trim(text);
    if (text.empty() || text.size() % 2 != 0)
    {
        return false;
    }
    bytes->clear();
    bytes->reserve(text.size() / 2);
    for (std::size_t index = 0; index < text.size(); index += 2)
    {
        const int high = HexDigit(text[index]);
        const int low = HexDigit(text[index + 1]);
        if (high < 0 || low < 0)
        {
            bytes->clear();
            return false;
        }
        bytes->push_back(static_cast<std::uint8_t>((high << 4) | low));
    }
    return true;
}

std::size_t ExpectedSize(std::uint32_t control_code)
{
    if (control_code == kLptdiIoctlCode410)
    {
        return kLptdiIoctlResponseSize410;
    }
    if (control_code == kLptdiIoctlCode414)
    {
        return kLptdiIoctlResponseSize414;
    }
    return 0;
}

void SetLineError(std::size_t line_number, const char* reason, std::string* error)
{
    *error = "LPTDI response profile line " + std::to_string(line_number) + ": " + reason;
}

}  // namespace

bool ReadLptdiResponseProfile(const std::filesystem::path& path,
                              LptdiResponseProfile* profile,
                              std::string* error)
{
    if (profile == nullptr || error == nullptr)
    {
        return false;
    }
    profile->entries.clear();
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        *error = "cannot open LPTDI response profile";
        return false;
    }

    bool have_header = false;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(stream, line))
    {
        ++line_number;
        std::string_view text = Trim(line);
        if (!have_header && text.size() >= 3 &&
            static_cast<unsigned char>(text[0]) == 0xef &&
            static_cast<unsigned char>(text[1]) == 0xbb &&
            static_cast<unsigned char>(text[2]) == 0xbf)
        {
            text.remove_prefix(3);
            text = Trim(text);
        }
        if (text.empty() || text.front() == '#')
        {
            continue;
        }
        if (!have_header)
        {
            if (text != "re2dj-lptdi-response-v1")
            {
                SetLineError(line_number, "missing or invalid header", error);
                return false;
            }
            have_header = true;
            continue;
        }

        const std::size_t separator = text.find('=');
        if (separator == std::string_view::npos || text.find('=', separator + 1) != std::string_view::npos)
        {
            SetLineError(line_number, "expected IOCTL_CODE=HEX_BYTES", error);
            return false;
        }
        LptdiResponseEntry entry;
        if (!ParseControlCode(text.substr(0, separator), &entry.control_code))
        {
            SetLineError(line_number, "invalid IOCTL code", error);
            return false;
        }
        const std::size_t expected_size = ExpectedSize(entry.control_code);
        if (expected_size == 0)
        {
            SetLineError(line_number, "unknown IOCTL code", error);
            return false;
        }
        if (FindLptdiResponse(*profile, entry.control_code) != nullptr)
        {
            SetLineError(line_number, "duplicate IOCTL code", error);
            return false;
        }
        if (!ParseBytes(text.substr(separator + 1), &entry.bytes))
        {
            SetLineError(line_number, "invalid response hex", error);
            return false;
        }
        if (entry.bytes.size() != expected_size)
        {
            SetLineError(line_number, "response size does not match IOCTL contract", error);
            return false;
        }
        profile->entries.push_back(std::move(entry));
    }
    if (!have_header)
    {
        *error = "LPTDI response profile is missing its header";
        return false;
    }
    if (profile->entries.empty())
    {
        *error = "LPTDI response profile has no entries";
        return false;
    }
    return true;
}

const LptdiResponseEntry* FindLptdiResponse(const LptdiResponseProfile& profile,
                                            std::uint32_t control_code)
{
    const auto found = std::find_if(
        profile.entries.begin(),
        profile.entries.end(),
        [control_code](const LptdiResponseEntry& entry) {
            return entry.control_code == control_code;
        });
    return found == profile.entries.end() ? nullptr : &*found;
}

}  // namespace re2dj::device
