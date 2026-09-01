#ifndef RE2DJ_TARGET_TARGET_PROFILE_H_
#define RE2DJ_TARGET_TARGET_PROFILE_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "re2dj/hdd/hdd_root.h"
#include "re2dj/hdd/hdd_scan.h"

namespace re2dj::target
{

enum class ExecutableFormatHint
{
    kWin32Pe32,
};

enum class HddInputKind
{
    kDirectory,
    kMameChd,
};

// LPTDI and legacy I/O policy for one executable profile. An empty or false
// setting means that the profile does not claim that capability.
struct TargetLptdiPolicy
{
    // Enables the confirmed raw IN/OUT adapter for this profile.
    bool legacy_io_ports = false;
    // Case-insensitive prefix for the synthetic Win32 device path.
    std::string device_mock_path_prefix;
    // Allows the diagnostic synthetic LPTDI device boundary for this profile.
    bool device_mock_enabled = false;
    // Profile-specific response state for the synthetic device.
    std::string device_mock_target_state_hex;
};

// Baseline settings for the supported product execution path. An empty or
// false setting means that the profile does not claim that capability.
struct TargetRunDefaults
{
    // Repository-relative convenience path used by the profile shortcut.
    std::string default_hdd_directory_relative_path;
    // Repository-relative directory containing the CHD selected by a profile.
    std::string default_hdd_image_relative_path;
    HddInputKind hdd_input_kind = HddInputKind::kDirectory;
    // Optional values are omitted when the original build has no corresponding
    // configuration import that the runtime can override safely.
    std::optional<float> audio_gain_db;
    std::optional<unsigned> demo_volume;
    bool fullscreen = false;
    bool hle_command_line = false;
    bool hle_windows_directory = false;
    bool hle_vfs = false;
    // Allows confirmed dynamic file API resolution through the VFS wrapper.
    bool hle_dynamic_vfs = false;
    bool hle_d3d3 = false;
    bool hle_directsound = false;
    TargetLptdiPolicy lptdi;
    bool run_detached = false;
};

// How a built-in profile recognises the dump it belongs to.
//
// File size and content hashes were rejected as the matching key: both vary per
// revision and per dump, so either would reject a legitimate dump. A name plus
// the entries that must sit beside it stays stable across revisions.
struct TargetFingerprint
{
    // File name only, matched case-insensitively against the scan.
    std::string_view executable_name;
    // Entries that must resolve in the executable's own directory. These make
    // two profiles distinguishable even when their executables differ only in
    // case, which case-insensitive resolution would otherwise hide.
    std::vector<std::string_view> required_siblings;
};

// Everything that differs between EZ2DJ versions, kept out of the loader and
// the HLE layer so both stay version-neutral.
struct TargetProfile
{
    // Short identifier chosen on the command line.
    std::string id;
    std::string display_name;
    // '/'-separated, relative to the HDD root. Filled in when a fingerprint
    // matches, so it reflects where the executable actually sits.
    std::string executable_relative_path;
    // Host-side working directory, '/'-separated and relative to the HDD root.
    // Empty means the root itself.
    std::string working_directory_relative_path;

    // The drive letter the guest believes it runs from, or '\0' when the dump
    // carries no evidence of one. Never guessed.
    char guest_drive_letter = '\0';
    // The Win32 directory the guest believes it runs in, for example
    // "\\ez2dj". Empty when the dump carries no evidence of one.
    std::string guest_directory;

    // Names the set of HLE services this version needs. Empty until real HLE
    // profiles exist.
    std::string hle_profile_id;
    TargetRunDefaults run_defaults;
    ExecutableFormatHint format_hint = ExecutableFormatHint::kWin32Pe32;

    // True when the profile came from a scan rather than the built-in table.
    bool detected = false;
    // True when this executable is useful for development but is not what the
    // cabinet actually ran. Recorded on the profile so behavior observed
    // through it is never cited as original behavior.
    bool bring_up_target = false;
    // Human-readable qualification: why this profile exists, or what is not
    // known about it.
    std::string note;
};

// Fingerprints for versions confirmed against a real dump, in the order they
// should be offered. See docs/design/20260822-005-built-in-target-profiles.md.
struct BuiltInTargetProfile
{
    TargetProfile profile;
    TargetFingerprint fingerprint;
};

const std::vector<BuiltInTargetProfile>& GetBuiltInTargetProfiles();

const BuiltInTargetProfile* FindBuiltInTargetProfileById(std::string_view id);

// Built-in profiles whose fingerprint matches this dump, with their paths
// filled in from the match.
std::vector<TargetProfile> MatchBuiltInTargetProfiles(const hdd::HddRoot& root,
                                                      const hdd::HddScanResult& scan);

// One profile per plausible game executable found by a scan, ordered the same
// way the scan ordered its candidates. Executables listed in `claimed_paths`
// are skipped, so a built-in profile is never duplicated by detection.
std::vector<TargetProfile> DetectTargetProfiles(
    const hdd::HddScanResult& scan,
    const std::vector<std::string>& claimed_paths = {});

// Matching built-in profiles first, then detected ones for whatever is left.
// The first entry is what the host selects when the user names no target.
std::vector<TargetProfile> BuildTargetProfiles(const hdd::HddRoot& root,
                                               const hdd::HddScanResult& scan);

const TargetProfile* FindTargetProfileById(const std::vector<TargetProfile>& profiles,
                                           std::string_view id);

// Lowercased file stem, with anything outside [a-z0-9_] replaced by '_'.
std::string MakeProfileId(std::string_view executable_relative_path);

std::string_view ExecutableFormatHintName(ExecutableFormatHint format_hint);

}  // namespace re2dj::target

#endif  // RE2DJ_TARGET_TARGET_PROFILE_H_
