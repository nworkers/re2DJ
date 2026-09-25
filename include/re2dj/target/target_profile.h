#ifndef RE2DJ_TARGET_TARGET_PROFILE_H_
#define RE2DJ_TARGET_TARGET_PROFILE_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "re2dj/graphics/present_sync.h"
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

// Operand width of one product's raw port instructions. This is a property of
// the cabinet's I/O board, so it belongs to the profile: EZ2DJ boards are read
// and written a byte at a time, while the EZ2Dancer board is word-wide and its
// instructions therefore carry a 0x66 operand-size prefix.
enum class LegacyIoWidth
{
    kByte,
    kWord,
};

// LPTDI and legacy I/O policy for one executable profile. An empty or false
// setting means that the profile does not claim that capability.
struct TargetLptdiPolicy
{
    // Allows explicit use of the profile's confirmed raw I/O contract.
    bool legacy_io_ports = false;
    // Adds raw I/O HLE to the product facade's normal arguments.
    bool legacy_io_ports_default = false;
    // Allows I/O trapping at supported board ports when this executable's exact
    // helper RVA has not yet been confirmed.
    bool legacy_io_port_range_fallback = false;
    LegacyIoWidth legacy_io_width = LegacyIoWidth::kByte;
    // Main-image RVA of the confirmed input helper. Zero means unknown. The
    // address is that of the instruction's first byte, which for a word-wide
    // helper is its 0x66 prefix rather than the opcode.
    std::uint32_t legacy_io_in_rva = 0;
    // Main-image RVA of the confirmed output helper. Zero means unknown.
    std::uint32_t legacy_io_out_rva = 0;
    // Case-insensitive prefix for the synthetic Win32 device path.
    std::string device_mock_path_prefix;
    // Allows the diagnostic synthetic LPTDI device boundary for this profile.
    bool device_mock_enabled = false;
    // Profile-specific response state for the synthetic device.
    std::string device_mock_target_state_hex;
    // Reads Hardlock material from the conventional cfg paths when it is there:
    // the profile's response map and the optional device replay values. Nothing
    // in this repository produces those values, and their absence is not an
    // error, so this only decides whether the files are consulted.
    bool hardlock_cfg_material_default = false;
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
    // Reports the current session ID as 0, the console session of the Windows
    // XP era the cabinet ran. Since Vista session 0 is reserved for services,
    // so a modern host returns 1 or higher. The cabinet's original ran as the
    // console shell, so this is an operating-system boundary rather than a
    // diagnostic. Only a successful WTS_CURRENT_SESSION WTSSessionId (class 4)
    // result is rewritten; other queries and failures are preserved.
    bool hle_wts_console_session = false;
    TargetLptdiPolicy lptdi;
    // Starts a known bootstrap executable and follows its version-specific
    // game child before applying the HLE boundary.
    bool follow_child_process = false;
    bool run_detached = false;
    // When a present returns. No profile overrides this yet: the default is
    // the behavior every profile had before the policy became explicit, and a
    // product-specific value needs its own runtime evidence first.
    graphics::PresentSync present_sync = graphics::PresentSync::kVerticalSync;
    // Accounts the guest's blocking calls so frame time that is neither
    // computation nor presentation can be attributed. Off by default: it
    // patches guest import slots that the product path leaves alone.
    bool guest_wait_trace = false;
    // Saves the main image the protection decrypted in place, at the restored
    // entry and again once the guest has been running. Off by default: the
    // image runs to tens of megabytes and only an analysis run wants it.
    bool image_dump = false;
    // Milliseconds between resuming the guest and the second dump. Zero leaves
    // the launcher's own default in place.
    unsigned image_dump_delay_ms = 0;
};

// How a built-in profile recognises the dump it belongs to.
//
// File size and content hashes were rejected as the matching key: both vary per
// revision and per dump, so either would reject a legitimate dump. A name plus
// the entries that must sit beside it stays stable across revisions. Some
// executable-only profiles also carry optional PE header constraints when a
// case-insensitive name would otherwise collide with another built-in profile.
struct TargetFingerprint
{
    // File name only, matched case-insensitively against the scan.
    std::string_view executable_name;
    // Optional PE32 header identity, matched against the named executable.
    std::optional<std::uint32_t> entry_point_rva;
    std::optional<std::uint32_t> size_of_image;
    // Entries that must resolve in the executable's own directory. These make
    // two profiles distinguishable even when their executables differ only in
    // case, which case-insensitive resolution would otherwise hide.
    std::vector<std::string_view> required_siblings;
};

// Everything that differs between EZ2DJ versions, kept out of the loader and
// the HLE layer so both stay version-neutral.
// Controls over the guest's own state that the on-screen display may offer.
//
// Each one names a variable inside the original executable, so it is valid for
// exactly one build. The build is identified by its PE timestamp, and a control
// is armed only when the running executable carries that timestamp: an address
// confirmed in one build means nothing in another, and writing it there could
// corrupt unrelated state.
struct GameControls
{
    // RVA of a 32-bit flag that makes the game hit notes by itself when
    // non-zero. Zero means this profile offers no autoplay control.
    std::uint32_t autoplay_flag_rva = 0;
    // PE TimeDateStamp of the build the RVA was confirmed in.
    std::uint32_t build_timestamp = 0;
};

// The autoplay flag RVA to arm for an executable with `executable_timestamp`,
// or zero when the profile declares none or the build is not the one it was
// confirmed in.
std::uint32_t ArmedAutoplayFlagRva(const GameControls& controls,
                                   std::uint32_t executable_timestamp);

struct TargetProfile
{
    // Short identifier chosen on the command line.
    std::string id;
    std::string display_name;
    // '/'-separated, relative to the HDD root. Directory profiles fill this
    // when a fingerprint matches; CHD profiles set their confirmed internal
    // executable path directly because they are selected by image shortcut.
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
    GameControls game_controls;
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

// The Win32 directory the guest runs from: the profile's drive and directory,
// or "D:\\ez2dj" when the dump carries no evidence of one, the root the
// Windows launcher has always given the VFS.
std::string GuestRootPath(const TargetProfile& profile);

// The guest's full path of the profile executable under GuestRootPath, for
// example "D:\\ez2dj\\EZ2DJ.EXE".
std::string GuestExecutablePath(const TargetProfile& profile);

}  // namespace re2dj::target

#endif  // RE2DJ_TARGET_TARGET_PROFILE_H_
