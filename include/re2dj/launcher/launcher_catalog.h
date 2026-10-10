#ifndef RE2DJ_LAUNCHER_LAUNCHER_CATALOG_H_
#define RE2DJ_LAUNCHER_LAUNCHER_CATALOG_H_

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "re2dj/target/target_profile.h"

namespace re2dj::launcher
{

// Whether a built-in profile can be started from the launcher (#12), judged by
// the rules the command-line profile shortcut applies to its default paths but
// without opening a CHD or scanning a dump: the launcher lists every profile
// when it opens, and the real judgement still happens when the run starts.
enum class ProfileAvailability : std::uint8_t
{
    kReady = 0,
    // A CHD profile whose image directory holds no `.chd`, or is missing.
    kNoChd,
    // A CHD profile whose image directory holds more than one `.chd`.
    kSeveralChds,
    // A directory profile whose dump directory is missing.
    kNoDirectory,
};

struct LauncherEntry
{
    std::string id;
    std::string display_name;
    ProfileAvailability availability = ProfileAvailability::kNoDirectory;
    // One line naming what is missing; empty when ready.
    std::string reason;
    // The CHD image or dump directory found when ready, otherwise the path
    // looked at.
    std::filesystem::path location;
    // The gain a run of this profile starts with when none is chosen.
    float default_audio_gain_db = 0.0F;
};

// One entry per built-in profile, in the built-in table's order, each probed
// against paths relative to `base_directory` (the current directory, as for
// the command-line shortcut). A profile id that appears twice is listed once.
[[nodiscard]] std::vector<LauncherEntry> BuildLauncherCatalog(const std::filesystem::path& base_directory);

[[nodiscard]] LauncherEntry ProbeLauncherProfile(const target::TargetProfile& profile,
                                                 const std::filesystem::path& base_directory);

// A short status word for the list ("Ready", "No CHD", ...).
[[nodiscard]] const char* ProfileAvailabilityName(ProfileAvailability availability);

[[nodiscard]] inline bool IsLauncherEntryRunnable(const LauncherEntry& entry)
{
    return entry.availability == ProfileAvailability::kReady;
}

}  // namespace re2dj::launcher

#endif  // RE2DJ_LAUNCHER_LAUNCHER_CATALOG_H_
