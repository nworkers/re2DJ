#include "re2dj/launcher/launcher_catalog.h"

#include <algorithm>
#include <system_error>

#include "re2dj/hdd/chd_image_locator.h"

namespace re2dj::launcher
{

LauncherEntry ProbeLauncherProfile(const target::TargetProfile& profile,
                                   const std::filesystem::path& base_directory)
{
    LauncherEntry entry;
    entry.id = profile.id;
    entry.display_name = profile.display_name;
    const target::TargetRunDefaults& defaults = profile.run_defaults;
    entry.default_audio_gain_db = defaults.audio_gain_db.value_or(0.0F);
    if (defaults.hdd_input_kind == target::HddInputKind::kMameChd)
    {
        const std::filesystem::path directory = base_directory / defaults.default_hdd_image_relative_path;
        std::filesystem::path image;
        std::string error;
        switch (hdd::LocateChdImage(directory, &image, &error))
        {
        case hdd::ChdImageLookup::kFound:
            entry.availability = ProfileAvailability::kReady;
            entry.location = image;
            return entry;
        case hdd::ChdImageLookup::kSeveral:
            entry.availability = ProfileAvailability::kSeveralChds;
            entry.reason = "more than one .chd in " + directory.string();
            break;
        case hdd::ChdImageLookup::kNotChd:
        case hdd::ChdImageLookup::kMissing:
        case hdd::ChdImageLookup::kNone:
            entry.availability = ProfileAvailability::kNoChd;
            entry.reason = "no .chd in " + directory.string();
            break;
        }
        entry.location = directory;
        return entry;
    }

    const std::filesystem::path directory = base_directory / defaults.default_hdd_directory_relative_path;
    entry.location = directory;
    std::error_code code;
    if (!defaults.default_hdd_directory_relative_path.empty() && std::filesystem::is_directory(directory, code))
    {
        entry.availability = ProfileAvailability::kReady;
        return entry;
    }
    entry.availability = ProfileAvailability::kNoDirectory;
    entry.reason = defaults.default_hdd_directory_relative_path.empty()
                       ? std::string("the profile has no default HDD directory")
                       : "no directory " + directory.string();
    return entry;
}

std::vector<LauncherEntry> BuildLauncherCatalog(const std::filesystem::path& base_directory)
{
    std::vector<LauncherEntry> catalog;
    for (const target::BuiltInTargetProfile& built_in : target::GetBuiltInTargetProfiles())
    {
        const bool listed = std::any_of(catalog.begin(), catalog.end(), [&](const LauncherEntry& entry) {
            return entry.id == built_in.profile.id;
        });
        if (!listed)
        {
            catalog.push_back(ProbeLauncherProfile(built_in.profile, base_directory));
        }
    }
    return catalog;
}

const char* ProfileAvailabilityName(ProfileAvailability availability)
{
    switch (availability)
    {
    case ProfileAvailability::kReady:
        return "Ready";
    case ProfileAvailability::kNoChd:
        return "No CHD";
    case ProfileAvailability::kSeveralChds:
        return "Several CHDs";
    case ProfileAvailability::kNoDirectory:
        return "No directory";
    }
    return "Unknown";
}

}  // namespace re2dj::launcher
