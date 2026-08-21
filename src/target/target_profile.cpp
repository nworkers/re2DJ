#include "re2dj/target/target_profile.h"

#include <unordered_map>
#include <utility>

#include "re2dj/storage/guest_path.h"

namespace re2dj::target
{

namespace
{

std::string_view FileStem(std::string_view relative_path)
{
    const std::size_t slash = relative_path.find_last_of('/');
    std::string_view name =
        slash == std::string_view::npos ? relative_path : relative_path.substr(slash + 1);
    const std::size_t dot = name.find_last_of('.');
    if (dot != std::string_view::npos && dot != 0)
    {
        name = name.substr(0, dot);
    }
    return name;
}

std::string_view ParentDirectory(std::string_view relative_path)
{
    const std::size_t slash = relative_path.find_last_of('/');
    if (slash == std::string_view::npos)
    {
        return {};
    }
    return relative_path.substr(0, slash);
}

}  // namespace

const std::vector<TargetProfile>& GetBuiltInTargetProfiles()
{
    static const std::vector<TargetProfile> profiles;
    return profiles;
}

std::string MakeProfileId(std::string_view executable_relative_path)
{
    const std::string_view stem = FileStem(executable_relative_path);
    std::string id;
    id.reserve(stem.size());
    for (const char value : stem)
    {
        if (value >= 'A' && value <= 'Z')
        {
            id.push_back(static_cast<char>(value - 'A' + 'a'));
        }
        else if ((value >= 'a' && value <= 'z') || (value >= '0' && value <= '9') ||
                 value == '_')
        {
            id.push_back(value);
        }
        else
        {
            id.push_back('_');
        }
    }
    if (id.empty())
    {
        id = "target";
    }
    return id;
}

std::vector<TargetProfile> DetectTargetProfiles(const hdd::HddScanResult& scan)
{
    std::vector<TargetProfile> profiles;
    std::unordered_map<std::string, int> id_uses;

    for (const hdd::ExecutableEntry& entry : scan.executables)
    {
        if (!entry.pe_readable || !exe::IsGuestExecutable(entry.pe_info))
        {
            continue;
        }

        TargetProfile profile;
        profile.id = MakeProfileId(entry.relative_path);
        const int use_count = ++id_uses[profile.id];
        if (use_count > 1)
        {
            // Two copies of the same executable name in different directories
            // are common in a dump, so the duplicate keeps a numeric suffix
            // rather than shadowing the first.
            profile.id += "_" + std::to_string(use_count);
        }

        profile.display_name = entry.relative_path;
        profile.executable_relative_path = entry.relative_path;
        profile.working_directory_relative_path =
            std::string(ParentDirectory(entry.relative_path));
        profile.format_hint = ExecutableFormatHint::kWin32Pe32;
        profile.detected = true;
        profiles.push_back(std::move(profile));
    }

    return profiles;
}

std::vector<TargetProfile> BuildTargetProfiles(const hdd::HddScanResult& scan)
{
    std::vector<TargetProfile> profiles = GetBuiltInTargetProfiles();
    for (TargetProfile& detected : DetectTargetProfiles(scan))
    {
        if (FindTargetProfileById(profiles, detected.id) != nullptr)
        {
            continue;
        }
        profiles.push_back(std::move(detected));
    }
    return profiles;
}

const TargetProfile* FindTargetProfileById(const std::vector<TargetProfile>& profiles,
                                           std::string_view id)
{
    for (const TargetProfile& profile : profiles)
    {
        if (storage::EqualsIgnoreAsciiCase(profile.id, id))
        {
            return &profile;
        }
    }
    return nullptr;
}

std::string_view ExecutableFormatHintName(ExecutableFormatHint format_hint)
{
    switch (format_hint)
    {
    case ExecutableFormatHint::kWin32Pe32:
    default:
        return "win32-pe32";
    }
}

}  // namespace re2dj::target
