#include "re2dj/config/hardlock_secret_config.h"

#include <array>
#include <cctype>
#include <fstream>
#include <limits>
#include <optional>
#include <system_error>

namespace re2dj::config
{
namespace
{

std::string_view Trim(std::string_view value)
{
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0)
    {
        value.remove_prefix(1);
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0)
    {
        value.remove_suffix(1);
    }
    return value;
}

std::optional<std::size_t> KeyIndex(std::string_view key)
{
    constexpr std::array<std::string_view, 4> kKeys = {
        "modad", "seed1", "seed2", "seed3"};
    for (std::size_t index = 0; index < kKeys.size(); ++index)
    {
        if (key == kKeys[index])
        {
            return index;
        }
    }
    return std::nullopt;
}

bool ParseWord(std::string_view text, std::uint16_t* value)
{
    if (value == nullptr || text.empty())
    {
        return false;
    }
    unsigned base = 10;
    if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
    {
        base = 16;
        text.remove_prefix(2);
    }
    if (text.empty())
    {
        return false;
    }
    std::uint32_t parsed = 0;
    for (const char character : text)
    {
        unsigned digit = 0;
        if (character >= '0' && character <= '9')
        {
            digit = static_cast<unsigned>(character - '0');
        }
        else if (base == 16 && character >= 'a' && character <= 'f')
        {
            digit = static_cast<unsigned>(character - 'a') + 10;
        }
        else if (base == 16 && character >= 'A' && character <= 'F')
        {
            digit = static_cast<unsigned>(character - 'A') + 10;
        }
        else
        {
            return false;
        }
        if (digit >= base || parsed >
                                 (std::numeric_limits<std::uint16_t>::max() - digit) / base)
        {
            return false;
        }
        parsed = parsed * base + digit;
    }
    *value = static_cast<std::uint16_t>(parsed);
    return true;
}

}  // namespace

std::filesystem::path DefaultHardlockSecretConfigPath()
{
    std::error_code code;
    const std::filesystem::path root = std::filesystem::current_path(code);
    return code ? std::filesystem::path("cfg") / "hardlock.ini"
                : root / "cfg" / "hardlock.ini";
}

bool IsHardlockSecretPathAllowed(const std::filesystem::path& path)
{
    std::error_code code;
    std::filesystem::path current = std::filesystem::absolute(path, code);
    if (code)
    {
        return false;
    }
    const std::filesystem::path canonical = std::filesystem::weakly_canonical(current, code);
    if (code)
    {
        return false;
    }
    current = canonical;
    if (!std::filesystem::is_directory(current, code))
    {
        current = current.parent_path();
    }
    std::filesystem::path candidate = current;
    while (!candidate.empty())
    {
        code.clear();
        if (std::filesystem::exists(candidate / ".git", code) && !code)
        {
            const std::filesystem::path relative = current.lexically_relative(candidate);
            return !relative.empty() && *relative.begin() == std::filesystem::path("cfg");
        }
        const std::filesystem::path parent = candidate.parent_path();
        if (parent == candidate)
        {
            break;
        }
        candidate = parent;
    }
    return true;
}

bool LoadHardlockSecretConfig(const std::filesystem::path& path,
                              std::string_view profile_id,
                              HardlockSecretMaterial* material,
                              std::string* error)
{
    if (material == nullptr || error == nullptr || path.empty() || profile_id.empty())
    {
        if (error != nullptr)
        {
            *error = "invalid Hardlock configuration request";
        }
        return false;
    }
    if (!IsHardlockSecretPathAllowed(path))
    {
        *error = "Hardlock configuration inside a Git work tree must be under cfg";
        return false;
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        *error = "cannot open Hardlock configuration";
        return false;
    }

    std::array<std::uint16_t, 4> values = {};
    std::array<bool, 4> present = {};
    bool selected_section_seen = false;
    bool selected_section = false;
    std::string line;
    while (std::getline(stream, line))
    {
        std::string_view text = Trim(line);
        if (text.empty() || text.front() == '#' || text.front() == ';')
        {
            continue;
        }
        if (text.front() == '[' && text.back() == ']')
        {
            const std::string_view section = Trim(text.substr(1, text.size() - 2));
            selected_section = section == profile_id;
            selected_section_seen = selected_section_seen || selected_section;
            continue;
        }
        if (!selected_section)
        {
            continue;
        }
        const std::size_t separator = text.find('=');
        if (separator == std::string_view::npos)
        {
            *error = "malformed Hardlock configuration entry";
            return false;
        }
        const std::string_view key = Trim(text.substr(0, separator));
        const std::string_view raw_value = Trim(text.substr(separator + 1));
        const std::optional<std::size_t> index = KeyIndex(key);
        if (!index.has_value())
        {
            *error = "unknown Hardlock configuration key";
            return false;
        }
        if (present[*index])
        {
            *error = "duplicate Hardlock configuration key";
            return false;
        }
        if (!ParseWord(raw_value, &values[*index]))
        {
            *error = "Hardlock configuration value is not a 16-bit integer";
            return false;
        }
        present[*index] = true;
    }
    if (!stream.eof())
    {
        *error = "cannot read Hardlock configuration";
        return false;
    }
    if (!selected_section_seen)
    {
        *error = "Hardlock configuration does not contain the selected profile";
        return false;
    }
    for (const bool field_present : present)
    {
        if (!field_present)
        {
            *error = "Hardlock configuration is missing a required key";
            return false;
        }
    }

    material->module_address = values[0];
    material->seeds = {values[1], values[2], values[3]};
    error->clear();
    return true;
}

}  // namespace re2dj::config
