#include "re2dj/target/target_profile.h"

#include <algorithm>
#include <unordered_map>
#include <utility>

#include "re2dj/storage/guest_path.h"

namespace re2dj::target
{

namespace
{

std::string_view FileName(std::string_view relative_path)
{
    const std::size_t slash = relative_path.find_last_of('/');
    return slash == std::string_view::npos ? relative_path : relative_path.substr(slash + 1);
}

std::string_view FileStem(std::string_view relative_path)
{
    std::string_view name = FileName(relative_path);
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

std::string JoinRelative(std::string_view directory, std::string_view name)
{
    if (directory.empty())
    {
        return std::string(name);
    }
    std::string joined(directory);
    joined.push_back('/');
    joined.append(name);
    return joined;
}

bool FingerprintMatches(const hdd::HddRoot& root,
                        const TargetFingerprint& fingerprint,
                        std::string_view executable_relative_path)
{
    const std::string_view directory = ParentDirectory(executable_relative_path);
    for (const std::string_view sibling : fingerprint.required_siblings)
    {
        std::filesystem::path resolved;
        if (!root.Resolve(JoinRelative(directory, sibling), &resolved))
        {
            return false;
        }
    }
    return true;
}

}  // namespace

const std::vector<BuiltInTargetProfile>& GetBuiltInTargetProfiles()
{
    // Every entry here is backed by an inspected dump. Nothing is added on the
    // strength of a release list or a wiki page, because a guessed path would
    // later be cited as fact. See docs/analysis/ez2dj-hdd-layout.md.
    static const std::vector<BuiltInTargetProfile> profiles = [] {
        std::vector<BuiltInTargetProfile> table;

        {
            BuiltInTargetProfile entry;
            entry.profile.id = "ez2dj1stse";
            entry.profile.display_name = "EZ2DJ The 1st Tracks Special Edition";
            entry.profile.hle_profile_id = "ez2dj1stse";
            entry.profile.run_defaults.default_hdd_directory_relative_path =
                "roms/ez2dj1stse";
            entry.profile.run_defaults.audio_gain_db = 0.0f;
            entry.profile.run_defaults.demo_volume = 3;
            entry.profile.run_defaults.hle_command_line = true;
            entry.profile.run_defaults.hle_windows_directory = true;
            entry.profile.run_defaults.hle_vfs = true;
            entry.profile.run_defaults.hle_d3d3 = true;
            entry.profile.run_defaults.hle_directsound = true;
            entry.profile.run_defaults.lptdi.legacy_io_ports = true;
            entry.profile.run_defaults.lptdi.device_mock_path_prefix = "\\\\.\\LPTDI";
            entry.profile.run_defaults.lptdi.device_mock_enabled = true;
            entry.profile.run_defaults.run_detached = true;
            entry.profile.run_defaults.lptdi.device_mock_target_state_hex =
                "0900000000000000";
            entry.profile.working_directory_relative_path = {};
            // System.ini in this dump reads "shell=d:\ez2dj\ez2dj.exe", which is
            // what Windows 98 launches in place of Explorer. That single line
            // confirms the executable, the drive letter, and the directory.
            entry.profile.guest_drive_letter = 'D';
            entry.profile.guest_directory = "\\ez2dj";
            entry.profile.note =
                "Launched by the cabinet through the System.ini shell entry. The "
                "executable is protected: its entry point sits in .gtide, so "
                "running it needs a backend that tolerates self-modifying code.";
            entry.fingerprint.executable_name = "ez2dj.exe";
            entry.fingerprint.required_siblings = {
                "ez2dj1.exe", "ez2dj.ini", "System.ini", "Songs", "System"};
            table.push_back(std::move(entry));
        }

        {
            BuiltInTargetProfile entry;
            entry.profile.id = "ez2dj1stse_unpacked";
            entry.profile.display_name =
                "EZ2DJ The 1st Tracks Special Edition (unprotected build)";
            entry.profile.run_defaults.default_hdd_directory_relative_path =
                "roms/ez2dj1stse";
            entry.profile.guest_drive_letter = 'D';
            entry.profile.guest_directory = "\\ez2dj";
            entry.profile.bring_up_target = true;
            entry.profile.note =
                "Not what the cabinet ran. This 1999-12-24 build is unprotected "
                "and shares its first five sections with ez2dj.exe, which makes "
                "it the loader bring-up target. Behavior observed through it is "
                "not automatically original behavior.";
            entry.fingerprint.executable_name = "ez2dj1.exe";
            entry.fingerprint.required_siblings = {
                "ez2dj.exe", "ez2dj.ini", "Songs", "System"};
            table.push_back(std::move(entry));
        }

        {
            BuiltInTargetProfile entry;
            entry.profile.id = "ez2dj3rd";
            entry.profile.display_name = "EZ2DJ 3rd Trax";
            entry.profile.hle_profile_id = "ez2dj3rd";
            entry.profile.run_defaults.default_hdd_directory_relative_path =
                "roms/ez2dj3rd";
            entry.profile.run_defaults.audio_gain_db = 0.0f;
            entry.profile.run_defaults.demo_volume.reset();
            entry.profile.run_defaults.hle_vfs = true;
            entry.profile.run_defaults.hle_directsound = true;
            entry.profile.run_defaults.lptdi.device_mock_path_prefix = "\\\\.\\FEnteDev";
            entry.profile.run_defaults.lptdi.device_mock_enabled = true;
            // This zero-state probe is separate from 1st SE and is not a
            // confirmed physical Hardlock seed.
            entry.profile.run_defaults.lptdi.device_mock_target_state_hex =
                "0000000000000000";
            // The protection opens its device through a GetProcAddress-resolved
            // CreateFileA, so without dynamic resolution the device boundary
            // never sees that open and the protection stops at its own dialog.
            entry.profile.run_defaults.hle_dynamic_vfs = true;
            entry.profile.run_defaults.lptdi.hardlock_cfg_material_default = true;
            // Same boundary as 4th: the protection initialization reads the
            // session's connect state, and the cabinet ran this executable as
            // the console's shell.
            entry.profile.run_defaults.hle_wts_active_console = true;
            entry.profile.run_defaults.run_detached = true;
            // This dump carries no System.ini, so the drive letter and guest
            // directory stay empty rather than being copied from 1st SE.
            entry.profile.note =
                "The executable is protected: its entry point sits in .protect. "
                "The dump has no System.ini, so the guest drive letter and "
                "directory are not known.";
            entry.fingerprint.executable_name = "EZ2DJ.EXE";
            entry.fingerprint.required_siblings = {
                "EZ2DJ.INI", "FONTKR.DAT", "BG", "Sound", "system"};
            table.push_back(std::move(entry));
        }

        {
            BuiltInTargetProfile entry;
            entry.profile.id = "ez2dj4th";
            entry.profile.display_name = "EZ2DJ 4th (MAME CHD HDD)";
            entry.profile.hle_profile_id = "ez2dj4th";
            entry.profile.run_defaults.hdd_input_kind = HddInputKind::kMameChd;
            entry.profile.run_defaults.hle_vfs = true;
            entry.profile.run_defaults.hle_dynamic_vfs = true;
            entry.profile.run_defaults.lptdi.device_mock_enabled = true;
            entry.profile.run_defaults.lptdi.device_mock_path_prefix =
                "\\\\.\\FEnteDev";
            entry.profile.run_defaults.lptdi.hardlock_cfg_material_default = true;
            // The protection stops after its first device request unless the
            // session reports as an active console, and the cabinet ran this
            // executable as that console's shell.
            entry.profile.run_defaults.hle_wts_active_console = true;
            // Without this the launcher treats the first VFS file open as the
            // handoff and terminates the original, which is diagnostic rather
            // than product behavior.
            entry.profile.run_defaults.run_detached = true;
            entry.profile.run_defaults.default_hdd_image_relative_path =
                "roms/ez2dj4th";
            entry.profile.note =
                "The real 4thTrax CHD contains a FAT32-LBA volume. The confirmed "
                "game executable is EZ2DJ/EZ2DJ.EXE; CHD-backed reads stay "
                "read-only and are served through the runtime VFS.";
            entry.fingerprint.executable_name = "EZ2DJ.EXE";
            entry.fingerprint.required_siblings = {
                "EZ2DJ.INI", "FONTKR.DAT", "FONTEN.DAT", "BG", "SOUND", "SYSTEM"};
            table.push_back(std::move(entry));
        }

        return table;
    }();
    return profiles;
}

const BuiltInTargetProfile* FindBuiltInTargetProfileById(std::string_view id)
{
    for (const BuiltInTargetProfile& profile : GetBuiltInTargetProfiles())
    {
        if (storage::EqualsIgnoreAsciiCase(profile.profile.id, id))
        {
            return &profile;
        }
    }
    return nullptr;
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

std::vector<TargetProfile> MatchBuiltInTargetProfiles(const hdd::HddRoot& root,
                                                      const hdd::HddScanResult& scan)
{
    std::vector<TargetProfile> matched;
    if (!root.is_open())
    {
        return matched;
    }

    for (const BuiltInTargetProfile& candidate : GetBuiltInTargetProfiles())
    {
        if (candidate.profile.run_defaults.hdd_input_kind != HddInputKind::kDirectory)
        {
            // Image-backed profiles are selected through their image shortcut,
            // not by scanning an extracted directory tree.
            continue;
        }
        for (const hdd::ExecutableEntry& entry : scan.executables)
        {
            if (!entry.pe_readable || !exe::IsGuestExecutable(entry.pe_info))
            {
                continue;
            }
            if (!storage::EqualsIgnoreAsciiCase(FileName(entry.relative_path),
                                                candidate.fingerprint.executable_name))
            {
                continue;
            }
            if (!FingerprintMatches(root, candidate.fingerprint, entry.relative_path))
            {
                continue;
            }

            TargetProfile profile = candidate.profile;
            profile.executable_relative_path = entry.relative_path;
            // The guest runs with the executable's own directory current, which
            // is what the System.ini shell entry describes for 1st SE.
            profile.working_directory_relative_path =
                std::string(ParentDirectory(entry.relative_path));
            profile.detected = false;
            matched.push_back(std::move(profile));
            break;
        }
    }

    return matched;
}

std::vector<TargetProfile> DetectTargetProfiles(
    const hdd::HddScanResult& scan,
    const std::vector<std::string>& claimed_paths)
{
    std::vector<TargetProfile> profiles;
    std::unordered_map<std::string, int> id_uses;

    for (const hdd::ExecutableEntry& entry : scan.executables)
    {
        if (!entry.pe_readable || !exe::IsGuestExecutable(entry.pe_info))
        {
            continue;
        }
        const bool claimed = std::find(claimed_paths.begin(),
                                       claimed_paths.end(),
                                       entry.relative_path) != claimed_paths.end();
        if (claimed)
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
        profile.note = "Detected by scan. No built-in profile matched this dump.";
        profiles.push_back(std::move(profile));
    }

    return profiles;
}

std::vector<TargetProfile> BuildTargetProfiles(const hdd::HddRoot& root,
                                               const hdd::HddScanResult& scan)
{
    std::vector<TargetProfile> profiles = MatchBuiltInTargetProfiles(root, scan);

    std::vector<std::string> claimed_paths;
    claimed_paths.reserve(profiles.size());
    for (const TargetProfile& profile : profiles)
    {
        claimed_paths.push_back(profile.executable_relative_path);
    }

    for (TargetProfile& detected : DetectTargetProfiles(scan, claimed_paths))
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
