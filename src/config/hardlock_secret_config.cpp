#include "re2dj/config/hardlock_secret_config.h"

#include <fstream>
#include <istream>
#include <system_error>

namespace re2dj::config
{
namespace
{

std::string_view Trim(std::string_view value)
{
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t' ||
                              value.front() == '\r' || value.front() == '\n'))
    {
        value.remove_prefix(1);
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' ||
                              value.back() == '\r' || value.back() == '\n'))
    {
        value.remove_suffix(1);
    }
    return value;
}

}  // namespace

std::filesystem::path DefaultHardlockSecretConfigPath()
{
    std::error_code code;
    const std::filesystem::path root = std::filesystem::current_path(code);
    return code ? std::filesystem::path("cfg") / "hardlock.ini"
                : root / "cfg" / "hardlock.ini";
}

std::filesystem::path DefaultHardlockTransformMapPath(std::string_view profile_id)
{
    if (profile_id.empty())
    {
        return {};
    }
    const std::string name = "hardlock-" + std::string(profile_id) + ".map";
    std::error_code code;
    const std::filesystem::path root = std::filesystem::current_path(code);
    return code ? std::filesystem::path("cfg") / name : root / "cfg" / name;
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
    // Inside a Git work tree the material may only live under the ignored cfg
    // directory, so a completed file cannot be committed by accident. Outside
    // one the user chooses the location.
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

bool LoadHardlockProfileMaterial(const std::filesystem::path& path,
                                 std::string_view profile_id,
                                 HardlockSecretMaterial* material,
                                 bool* found,
                                 std::string* error)
{
    if (material == nullptr || found == nullptr || error == nullptr || path.empty() ||
        profile_id.empty())
    {
        if (error != nullptr)
        {
            *error = "invalid Hardlock configuration request";
        }
        return false;
    }
    *found = false;
    std::error_code code;
    if (!std::filesystem::exists(path, code) || code)
    {
        error->clear();
        return true;
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

    bool selected_section_seen = false;
    bool selected_section = false;
    std::string line;
    while (std::getline(stream, line))
    {
        const std::string_view text = Trim(line);
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
        if (key != "response450" && key != "tail44c")
        {
            *error = "unknown Hardlock configuration key";
            return false;
        }
        std::string& target = key == "response450" ? material->handshake_response_hex
                                                   : material->descriptor_tail_hex;
        if (!target.empty())
        {
            *error = "duplicate Hardlock configuration key";
            return false;
        }
        if (raw_value.empty())
        {
            *error = "Hardlock configuration value is empty";
            return false;
        }
        // The value keeps its file spelling; the launcher option parser is the
        // single place that validates the hex width.
        target.assign(raw_value);
    }
    if (!stream.eof())
    {
        *error = "cannot read Hardlock configuration";
        return false;
    }

    *found = selected_section_seen;
    error->clear();
    return true;
}

}  // namespace re2dj::config
