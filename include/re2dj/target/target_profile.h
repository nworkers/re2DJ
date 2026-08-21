#ifndef RE2DJ_TARGET_TARGET_PROFILE_H_
#define RE2DJ_TARGET_TARGET_PROFILE_H_

#include <string>
#include <string_view>
#include <vector>

#include "re2dj/hdd/hdd_scan.h"

namespace re2dj::target
{

enum class ExecutableFormatHint
{
    kWin32Pe32,
};

// Everything that differs between EZ2DJ versions, kept out of the loader and
// the HLE layer so both stay version-neutral.
struct TargetProfile
{
    // Short identifier chosen on the command line.
    std::string id;
    std::string display_name;
    // '/'-separated, relative to the HDD root.
    std::string executable_relative_path;
    // Guest current directory, '/'-separated and relative to the HDD root.
    // Empty means the root itself.
    std::string working_directory_relative_path;
    // Names the set of HLE services this version needs. Empty until real HLE
    // profiles exist.
    std::string hle_profile_id;
    ExecutableFormatHint format_hint = ExecutableFormatHint::kWin32Pe32;
    // True when the profile came from a scan rather than the built-in table.
    bool detected = false;
};

// Profiles confirmed against a real dump. Empty for now: AGENTS.md forbids
// inventing per-version paths before one has been inspected, so detection
// carries the load until entries can be added with evidence.
const std::vector<TargetProfile>& GetBuiltInTargetProfiles();

// Builds one profile per plausible game executable found by a scan, ordered the
// same way the scan ordered its candidates.
std::vector<TargetProfile> DetectTargetProfiles(const hdd::HddScanResult& scan);

// Built-in profiles first, then detected ones whose id does not collide.
std::vector<TargetProfile> BuildTargetProfiles(const hdd::HddScanResult& scan);

const TargetProfile* FindTargetProfileById(const std::vector<TargetProfile>& profiles,
                                           std::string_view id);

// Lowercased file stem, with anything outside [a-z0-9_] replaced by '_'.
std::string MakeProfileId(std::string_view executable_relative_path);

std::string_view ExecutableFormatHintName(ExecutableFormatHint format_hint);

}  // namespace re2dj::target

#endif  // RE2DJ_TARGET_TARGET_PROFILE_H_
