#include "re2dj/hle/private_profile.h"

#include <algorithm>
#include <cctype>

namespace re2dj::hle
{
namespace
{

std::string_view Trim(std::string_view text)
{
    const auto blank = [](char value) { return value == ' ' || value == '\t' || value == '\r'; };
    while (!text.empty() && blank(text.front()))
    {
        text.remove_prefix(1);
    }
    while (!text.empty() && blank(text.back()))
    {
        text.remove_suffix(1);
    }
    return text;
}

bool EqualsWithoutCase(std::string_view left, std::string_view right)
{
    if (left.size() != right.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index)
    {
        if (std::toupper(static_cast<unsigned char>(left[index])) !=
            std::toupper(static_cast<unsigned char>(right[index])))
        {
            return false;
        }
    }
    return true;
}

}  // namespace

std::optional<std::string> FindPrivateProfileValue(std::string_view text,
                                                   std::string_view section,
                                                   std::string_view key)
{
    section = Trim(section);
    key = Trim(key);
    bool in_section = false;
    bool section_seen = false;
    std::size_t start = 0;
    while (start < text.size())
    {
        std::size_t end = text.find('\n', start);
        if (end == std::string_view::npos)
        {
            end = text.size();
        }
        const std::string_view line = Trim(text.substr(start, end - start));
        start = end + 1;
        if (line.empty() || line.front() == ';')
        {
            continue;
        }
        if (line.front() == '[')
        {
            if (in_section)
            {
                // Only a section's first occurrence is searched.
                return std::nullopt;
            }
            const std::size_t close = line.find(']');
            const std::string_view name = Trim(line.substr(1, close == std::string_view::npos ? line.size() - 1 : close - 1));
            in_section = !section_seen && EqualsWithoutCase(name, section);
            section_seen = section_seen || in_section;
            continue;
        }
        if (!in_section)
        {
            continue;
        }
        const std::size_t equals = line.find('=');
        if (equals == std::string_view::npos)
        {
            continue;
        }
        if (EqualsWithoutCase(Trim(line.substr(0, equals)), key))
        {
            return std::string(Trim(line.substr(equals + 1)));
        }
    }
    return std::nullopt;
}

std::uint32_t ParsePrivateProfileInt(std::string_view value, std::uint32_t default_value)
{
    if (value.empty())
    {
        return default_value;
    }
    if (value.front() == '"' || value.front() == '\'')
    {
        value.remove_prefix(1);
    }
    bool negative = false;
    if (!value.empty() && (value.front() == '-' || value.front() == '+'))
    {
        negative = value.front() == '-';
        value.remove_prefix(1);
    }
    std::uint32_t base = 10;
    if (value.size() >= 2 && value[0] == '0' && value[1] == 'x')
    {
        base = 16;
        value.remove_prefix(2);
    }
    std::uint32_t number = 0;
    for (const char character : value)
    {
        std::uint32_t digit = 0;
        if (character >= '0' && character <= '9')
        {
            digit = static_cast<std::uint32_t>(character - '0');
        }
        else if (base == 16 && character >= 'a' && character <= 'f')
        {
            digit = static_cast<std::uint32_t>(character - 'a' + 10);
        }
        else if (base == 16 && character >= 'A' && character <= 'F')
        {
            digit = static_cast<std::uint32_t>(character - 'A' + 10);
        }
        else
        {
            break;
        }
        number = number * base + digit;
    }
    return negative ? 0U - number : number;
}

std::optional<std::vector<std::string>> ListPrivateProfileKeys(std::string_view text, std::string_view section)
{
    section = Trim(section);
    std::vector<std::string> keys;
    bool in_section = false;
    bool section_seen = false;
    std::size_t start = 0;
    while (start < text.size())
    {
        std::size_t end = text.find('\n', start);
        if (end == std::string_view::npos)
        {
            end = text.size();
        }
        const std::string_view line = Trim(text.substr(start, end - start));
        start = end + 1;
        if (line.empty() || line.front() == ';')
        {
            continue;
        }
        if (line.front() == '[')
        {
            if (in_section)
            {
                break;
            }
            const std::size_t close = line.find(']');
            const std::string_view name = Trim(line.substr(1, close == std::string_view::npos ? line.size() - 1 : close - 1));
            in_section = !section_seen && EqualsWithoutCase(name, section);
            section_seen = section_seen || in_section;
            continue;
        }
        const std::size_t equals = line.find('=');
        if (in_section && equals != std::string_view::npos)
        {
            keys.emplace_back(Trim(line.substr(0, equals)));
        }
    }
    if (!section_seen)
    {
        return std::nullopt;
    }
    return keys;
}

std::vector<std::string> ListPrivateProfileSections(std::string_view text)
{
    std::vector<std::string> sections;
    std::size_t start = 0;
    while (start < text.size())
    {
        std::size_t end = text.find('\n', start);
        if (end == std::string_view::npos)
        {
            end = text.size();
        }
        const std::string_view line = Trim(text.substr(start, end - start));
        start = end + 1;
        if (!line.empty() && line.front() == '[')
        {
            const std::size_t close = line.find(']');
            sections.emplace_back(Trim(line.substr(1, close == std::string_view::npos ? line.size() - 1 : close - 1)));
        }
    }
    return sections;
}

std::string PrivateProfileStringValue(std::string_view value)
{
    if (value.size() >= 2 && (value.front() == '"' || value.front() == '\'') && value.back() == value.front())
    {
        value = value.substr(1, value.size() - 2);
    }
    return std::string(value);
}

std::string PrivateProfileStringDefault(std::string_view default_value)
{
    while (!default_value.empty() && default_value.back() == ' ')
    {
        default_value.remove_suffix(1);
    }
    return std::string(default_value);
}

PrivateProfileCopy CopyPrivateProfileString(std::string_view text, std::uint32_t size)
{
    PrivateProfileCopy copy;
    if (size == 0)
    {
        copy.truncated = true;
        return copy;
    }
    const std::size_t room = size - 1;
    const std::size_t length = std::min(text.size(), room);
    copy.bytes.assign(text.begin(), text.begin() + static_cast<std::ptrdiff_t>(length));
    copy.bytes.push_back(0);
    copy.length = static_cast<std::uint32_t>(length);
    copy.truncated = text.size() >= room;
    return copy;
}

PrivateProfileCopy CopyPrivateProfileList(const std::vector<std::string>& names, std::uint32_t size)
{
    PrivateProfileCopy copy;
    std::vector<std::uint8_t> all;
    for (const std::string& name : names)
    {
        all.insert(all.end(), name.begin(), name.end());
        all.push_back(0);
    }
    if (size < 2)
    {
        copy.truncated = true;
        copy.bytes.assign(size, 0);
        return copy;
    }
    if (all.size() + 1 <= size)
    {
        copy.bytes = all;
        copy.bytes.push_back(0);
        copy.length = static_cast<std::uint32_t>(all.size());
        return copy;
    }
    copy.bytes.assign(all.begin(), all.begin() + (size - 2));
    copy.bytes.push_back(0);
    copy.bytes.push_back(0);
    copy.length = size - 2;
    copy.truncated = true;
    return copy;
}

std::optional<std::uint32_t> PrivateProfileIntOverride(std::string_view section,
                                                       std::string_view key,
                                                       std::uint32_t demo_volume)
{
    if (EqualsWithoutCase(section, "GAMEASSIGNMENTS") && EqualsWithoutCase(key, "DemoVolume") &&
        demo_volume <= kMaximumDemoVolume)
    {
        return demo_volume;
    }
    return std::nullopt;
}

}  // namespace re2dj::hle
