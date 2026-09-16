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
                        const hdd::ExecutableEntry& executable)
{
    if (fingerprint.entry_point_rva.has_value() &&
        executable.pe_info.entry_point_rva != *fingerprint.entry_point_rva)
    {
        return false;
    }
    if (fingerprint.size_of_image.has_value() &&
        executable.pe_info.size_of_image != *fingerprint.size_of_image)
    {
        return false;
    }

    const std::string_view directory = ParentDirectory(executable.relative_path);
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

BuiltInTargetProfile MakeChdCompatibilityProfile(std::string_view id,
                                                  std::string_view display_name,
                                                  std::string_view image_path,
                                                  std::string_view executable_path,
                                                  std::string_view note)
{
    BuiltInTargetProfile entry;
    entry.profile.id = std::string(id);
    entry.profile.display_name = std::string(display_name);
    entry.profile.hle_profile_id = std::string(id);
    entry.profile.run_defaults.hdd_input_kind = HddInputKind::kMameChd;
    entry.profile.run_defaults.hle_vfs = true;
    entry.profile.run_defaults.hle_dynamic_vfs = true;
    entry.profile.run_defaults.hle_d3d3 = true;
    entry.profile.run_defaults.hle_directsound = true;
    entry.profile.run_defaults.lptdi.legacy_io_ports = true;
    entry.profile.run_defaults.lptdi.legacy_io_ports_default = true;
    entry.profile.run_defaults.lptdi.legacy_io_in_rva = 0x000c3817;
    entry.profile.run_defaults.lptdi.legacy_io_out_rva = 0x000c384b;
    entry.profile.run_defaults.lptdi.device_mock_enabled = true;
    entry.profile.run_defaults.lptdi.device_mock_path_prefix =
        "\\\\.\\FEnteDev";
    entry.profile.run_defaults.lptdi.hardlock_cfg_material_default = true;
    entry.profile.run_defaults.hle_wts_active_console = true;
    entry.profile.run_defaults.run_detached = true;
    entry.profile.run_defaults.default_hdd_image_relative_path =
        std::string(image_path);
    entry.profile.executable_relative_path = std::string(executable_path);
    entry.profile.note = std::string(note);
    entry.fingerprint.executable_name = "EZ2DJ.EXE";
    entry.fingerprint.required_siblings = {
        "EZ2DJ.INI", "FONTKR.DAT", "FONTEN.DAT", "BG", "SOUND", "SYSTEM"};
    return entry;
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
            entry.profile.id = "ez2dj1st";
            entry.profile.display_name = "EZ2DJ The 1st Tracks";
            entry.profile.hle_profile_id = "ez2dj1st";
            entry.profile.run_defaults.default_hdd_directory_relative_path =
                "roms/ez2dj1st";
            entry.profile.run_defaults.audio_gain_db = 0.0f;
            entry.profile.run_defaults.demo_volume = 3;
            entry.profile.run_defaults.hle_command_line = true;
            // GetWindowsDirectoryA is not in this build's import table.
            // Requesting it fails the whole handoff preparation, exactly as it
            // does on the 1st SE CHD build.
            entry.profile.run_defaults.hle_windows_directory = false;
            entry.profile.run_defaults.hle_vfs = true;
            // This executable is the same .protect family as the 1st SE CHD
            // build: the packer resolves the original imports itself at unpack
            // time, so a static IAT patch is overwritten. Without the dynamic
            // resolver its CreateFileA reaches the host, it opens the real
            // device, and it calls ExitProcess before any Hardlock request.
            entry.profile.run_defaults.hle_dynamic_vfs = true;
            // Unlike the 1st SE CHD build, this one's import table carries
            // DirectDrawCreate and GetPrivateProfileIntA, so these two
            // boundaries prepare and stay on.
            entry.profile.run_defaults.hle_d3d3 = true;
            entry.profile.run_defaults.hle_directsound = true;
            entry.profile.run_defaults.lptdi.legacy_io_ports = true;
            entry.profile.run_defaults.lptdi.legacy_io_ports_default = true;
            // Read out of this build's own decrypted .text. Past the Hardlock
            // transform loop the guest faults on an untrapped `in al, dx` at
            // 0x00035757, and the code window there holds the usual pair of
            // port helpers: `xor eax,eax; mov dx,[esp+4]; in al,dx; ret` and
            // `xor eax,eax; mov dx,[esp+4]; mov al,[esp+8]; out dx,al; ret`.
            // The runtime matches the faulting instruction address, so these
            // are the opcode bytes rather than the helper entry points. The
            // 1st SE .gtide values that stood here before belong to a
            // different build and never matched.
            entry.profile.run_defaults.lptdi.legacy_io_in_rva = 0x00035757;
            entry.profile.run_defaults.lptdi.legacy_io_out_rva = 0x0003577b;
            // Confirmed by the device trace: it tries \\.\NTICE, fails with
            // error 123, then opens \\.\FEnteDev. It never opens \\.\LPTDI, so
            // the LPTDI target-state probe has nothing to answer here and is
            // left unset.
            entry.profile.run_defaults.lptdi.device_mock_path_prefix =
                "\\\\.\\FEnteDev";
            entry.profile.run_defaults.lptdi.device_mock_enabled = true;
            entry.profile.run_defaults.lptdi.hardlock_cfg_material_default = true;
            entry.profile.run_defaults.run_detached = true;
            // The user-prepared Ez2DJ.exe is the only HDD entry used for profile
            // identification. Its parent directory becomes the VFS source root
            // after matching.
            entry.profile.note =
                "Uses the user-provided Ez2DJ.exe representative executable. It "
                "belongs to the same .protect Hardlock family as the 1st SE CHD "
                "build, 3rd and 4th: it opens \\\\.\\FEnteDev and reaches the "
                "0x9c402468 initialize, 0x9c402450 handshake and 0x9c40244c "
                "descriptor requests, and without local Hardlock material it "
                "stops at the handshake. Its descriptor reports module_address "
                "0x15e5. The legacy-I/O helper RVAs are this executable's own, "
                "confirmed by signature scan of its decrypted image dump.";
            entry.fingerprint.executable_name = "Ez2DJ.exe";
            entry.fingerprint.entry_point_rva = 0x0199b240;
            entry.fingerprint.size_of_image = 0x019b6000;
            table.push_back(std::move(entry));
        }

        {
            BuiltInTargetProfile entry;
            entry.profile.id = "ez2dj2nd";
            entry.profile.display_name = "EZ2DJ 2nd Trax";
            entry.profile.hle_profile_id = "ez2dj2nd";
            entry.profile.run_defaults.default_hdd_directory_relative_path =
                "roms/ez2dj2nd";
            entry.profile.run_defaults.audio_gain_db = 0.0f;
            // The 2nd import table has no GetPrivateProfileIntA slot, so the
            // 1st SE demo-volume injection is not applicable here.
            entry.profile.run_defaults.demo_volume.reset();
            // These defaults mirror 1st SE where runtime evidence has not yet
            // established a different 2nd executable contract.
            entry.profile.run_defaults.hle_command_line = true;
            entry.profile.run_defaults.hle_windows_directory = true;
            entry.profile.run_defaults.hle_vfs = true;
            entry.profile.run_defaults.hle_d3d3 = true;
            entry.profile.run_defaults.hle_directsound = true;
            entry.profile.run_defaults.lptdi.legacy_io_ports = true;
            entry.profile.run_defaults.lptdi.legacy_io_ports_default = true;
            // Confirmed by the first 2nd runtime privileged-instruction fault.
            entry.profile.run_defaults.lptdi.legacy_io_in_rva = 0x000782d7;
            // Confirmed by the follow-up 2nd runtime privileged-instruction
            // fault at the OUT DX,AL helper.
            entry.profile.run_defaults.lptdi.legacy_io_out_rva = 0x0007832b;
            entry.profile.run_defaults.lptdi.device_mock_path_prefix = "\\\\.\\LPTDI";
            entry.profile.run_defaults.lptdi.device_mock_enabled = true;
            entry.profile.run_defaults.run_detached = true;
            entry.profile.run_defaults.lptdi.device_mock_target_state_hex =
                "0900000000000000";
            // No System.ini was found in the 2nd dump, so the guest boot path
            // remains unset until it is confirmed from runtime evidence.
            entry.profile.note =
                "The 1st SE HLE execution defaults are reused as a compatibility "
                "baseline. The 2nd executable's Hardlock, legacy I/O, and guest "
                "boot contracts are not independently confirmed.";
            entry.fingerprint.executable_name = "EZ2DJ.exe";
            entry.fingerprint.entry_point_rva = 0x00079550;
            entry.fingerprint.size_of_image = 0x0047d000;
            entry.fingerprint.required_siblings = {
                "EZ2DJ.ini", "bg", "sound", "system"};
            table.push_back(std::move(entry));
        }

        {
            BuiltInTargetProfile entry;
            entry.profile.id = "ez2dj1stse";
            entry.profile.display_name = "EZ2DJ The 1st Tracks Special Edition";
            entry.profile.hle_profile_id = "ez2dj1stse";
            entry.profile.run_defaults.default_hdd_directory_relative_path =
                "roms/ez2dj1stse";
            entry.profile.run_defaults.hdd_input_kind = HddInputKind::kMameChd;
            entry.profile.run_defaults.default_hdd_image_relative_path =
                "roms/ez2dj1stse";
            entry.profile.run_defaults.audio_gain_db = 0.0f;
            // Every HLE default below follows the CHD build's packed import
            // directory at RVA 0x01aebbd0, which is what the launcher searches
            // for IAT slots. The original .idata survives in the file but the
            // PE header no longer points at it, so an import missing from the
            // packed table cannot be patched at all.
            //
            // GetPrivateProfileIntA is absent, so the demo-volume injection
            // cannot be prepared. The CHD's own ez2dj.ini already reads
            // DemoVolume=3.
            entry.profile.run_defaults.demo_volume.reset();
            // GetCommandLineA is present in the packed table.
            entry.profile.run_defaults.hle_command_line = true;
            // GetWindowsDirectoryA is not, and requesting it failed the whole
            // handoff preparation.
            entry.profile.run_defaults.hle_windows_directory = false;
            entry.profile.run_defaults.hle_vfs = true;
            // The protection reaches CreateFileA, DeviceIoControl, and
            // CloseHandle through GetProcAddress, so the static slots alone
            // never see its device work.
            entry.profile.run_defaults.hle_dynamic_vfs = true;
            // The packed table contributes only DirectDrawEnumerateA, but the
            // original .idata survives at RVA 0x01aba000 and imports
            // DDRAW.dll!DirectDrawCreate, which the IAT lookup now finds.
            entry.profile.run_defaults.hle_d3d3 = true;
            // DSOUND.dll ordinal 1 is present.
            entry.profile.run_defaults.hle_directsound = true;
            entry.profile.run_defaults.lptdi.legacy_io_ports = true;
            entry.profile.run_defaults.lptdi.legacy_io_ports_default = true;
            // These two RVAs are confirmed in the extracted .gtide build's
            // plaintext .text. They prepare cleanly here, but this build dies
            // in the Hardlock transform loop before reaching them, so their
            // correctness for this executable is still unconfirmed.
            entry.profile.run_defaults.lptdi.legacy_io_in_rva = 0x00038987;
            entry.profile.run_defaults.lptdi.legacy_io_out_rva = 0x000389ab;
            // Confirmed by the device trace: this build opens \\.\NTICE, fails,
            // then opens \\.\FEnteDev. It never opens the \\.\LPTDI device the
            // extracted .gtide build used, so the LPTDI target-state probe has
            // nothing to answer here and is left unset.
            entry.profile.run_defaults.lptdi.device_mock_path_prefix =
                "\\\\.\\FEnteDev";
            entry.profile.run_defaults.lptdi.device_mock_enabled = true;
            entry.profile.run_defaults.lptdi.hardlock_cfg_material_default = true;
            entry.profile.run_defaults.run_detached = true;
            // hle_wts_active_console stays off: runs with and without the
            // active-console report produced an identical IOCTL sequence, so
            // there is no evidence to turn it on for this build.
            entry.profile.working_directory_relative_path = {};
            // This CHD boots Explorer and starts the game from a StartUp
            // shortcut whose target string is "C:\ez2dj\Ez2DJ.exe"; the spare
            // SYSTEM.INI beside the executable names the same drive. The
            // extracted dump's System.ini said "d:", so the letter is a
            // property of the input, not of the release.
            entry.profile.guest_drive_letter = 'C';
            entry.profile.guest_directory = "\\ez2dj";
            entry.profile.executable_relative_path = "ez2dj/Ez2DJ.exe";
            // The CHD executable is not the .gtide build every earlier 1st SE
            // runtime fact came from. Both wrap the same original build - equal
            // PE timestamp, equal placement of the first five sections, and
            // byte-identical .idata - but this one is the .protect family used
            // by 3rd and 4th, and its execution defaults now follow its own
            // observed boundary rather than the .gtide baseline.
            entry.profile.note =
                "The CHD build wraps the original image in a .protect section "
                "with its entry point at 0x01ad1240, not the .gtide layout of "
                "the extracted dump, so running it needs a backend that "
                "tolerates self-modifying code. It belongs to the same Hardlock "
                "family as 3rd and 4th: it opens \\\\.\\FEnteDev and reaches the "
                "0x9c402468 initialize request, and without local Hardlock "
                "material it stops there. Its packed import directory omits "
                "GetWindowsDirectoryA, DirectDrawCreate, and "
                "GetPrivateProfileIntA, so those HLE boundaries stay off. The "
                "legacy-I/O helper RVAs equal the .gtide build's, and a signature "
                "scan of this executable's decrypted image dump confirms them.";
            entry.fingerprint.executable_name = "ez2dj.exe";
            entry.fingerprint.required_siblings = {
                "ez2dj1.exe", "ez2dj.ini", "System.ini", "Songs", "System"};
            table.push_back(std::move(entry));
        }

        {
            BuiltInTargetProfile entry;
            entry.profile.id = "ez2dj3rd";
            entry.profile.display_name = "EZ2DJ 3rd Trax (MAME CHD HDD)";
            entry.profile.hle_profile_id = "ez2dj3rd";
            entry.profile.run_defaults.default_hdd_directory_relative_path =
                "roms/ez2dj3rd";
            entry.profile.run_defaults.hdd_input_kind = HddInputKind::kMameChd;
            entry.profile.run_defaults.default_hdd_image_relative_path =
                "roms/ez2dj3rd";
            entry.profile.run_defaults.audio_gain_db = 0.0f;
            entry.profile.run_defaults.demo_volume.reset();
            entry.profile.run_defaults.hle_vfs = true;
            entry.profile.run_defaults.hle_d3d3 = true;
            entry.profile.run_defaults.hle_directsound = true;
            entry.profile.run_defaults.lptdi.legacy_io_ports = true;
            entry.profile.run_defaults.lptdi.legacy_io_ports_default = true;
            entry.profile.run_defaults.lptdi.legacy_io_in_rva = 0x000a9887;
            entry.profile.run_defaults.lptdi.legacy_io_out_rva = 0x000a98bb;
            // The game's own autoplay flag, set by its demo routine and flipped
            // by input slot 0x1b. Writing it to 1 before a song starts makes that
            // song play itself, with no demo side effects; the value is latched
            // at song start. Confirmed by reading and writing it in this build
            // (tasks 295 and 296), so it is bound to this build's timestamp.
            entry.profile.game_controls.autoplay_flag_rva = 0x00629508;
            entry.profile.game_controls.build_timestamp = 0x3bca98a3;
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
            entry.profile.executable_relative_path = "EZ2DJ/EZ2DJ.EXE";
            // This dump carries no System.ini, so the drive letter and guest
            // directory stay empty rather than being copied from 1st SE.
            entry.profile.note =
                "The supplied 3rd CHD exposes EZ2DJ/EZ2DJ.EXE through a FAT32 "
                "volume. The raw-I/O helper RVAs are confirmed at 0x000a9887 (in) "
                "and 0x000a98bb (out); its version-specific protection response "
                "remains unresolved.";
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
            entry.profile.run_defaults.hle_d3d3 = true;
            entry.profile.run_defaults.hle_directsound = true;
            entry.profile.run_defaults.lptdi.legacy_io_ports = true;
            entry.profile.run_defaults.lptdi.legacy_io_ports_default = true;
            entry.profile.run_defaults.lptdi.legacy_io_in_rva = 0x000c3817;
            // The byte-width out helper, observed as the faulting instruction
            // of an untrapped `out dx, al` once graphics initialization got
            // past device creation.
            entry.profile.run_defaults.lptdi.legacy_io_out_rva = 0x000c384b;
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
            entry.profile.executable_relative_path = "EZ2DJ/EZ2DJ.EXE";
            entry.profile.note =
                "The real 4thTrax CHD contains a FAT32-LBA volume. The confirmed "
                "game executable is EZ2DJ/EZ2DJ.EXE; CHD-backed reads stay "
                "read-only and are served through the runtime VFS.";
            entry.fingerprint.executable_name = "EZ2DJ.EXE";
            entry.fingerprint.required_siblings = {
                "EZ2DJ.INI", "FONTKR.DAT", "FONTEN.DAT", "BG", "SOUND", "SYSTEM"};
            table.push_back(std::move(entry));
        }

        {
            BuiltInTargetProfile entry = MakeChdCompatibilityProfile(
                "ez2dj5th",
                "EZ2DJ 5th Trax",
                "roms/ez2dj5th",
                "EZ2DJ/EZ2DJ.EXE",
                "The 5th executable belongs to the same .protect Hardlock family "
                "as 1st, 1st SE, 3rd and 4th: it opens \\\\.\\FEnteDev and runs "
                "the initialize, handshake, descriptor and transform requests, "
                "and its descriptor reports module_address 0x4c5c. The supplied "
                "CHD is a whole-disk FAT32 volume with no partition table, "
                "which the current reader does not mount, so the boundary was "
                "observed from the extracted directory instead.");
            // Read out of this build's own decrypted .text rather than inherited
            // from 4th. Past the Hardlock transform loop the guest faults on an
            // untrapped `in al, dx` at 0x000ca067, and the code window there
            // holds the usual pair of port helpers. The runtime matches the
            // faulting instruction address, so these are the opcode bytes.
            entry.profile.run_defaults.lptdi.legacy_io_in_rva = 0x000ca067;
            entry.profile.run_defaults.lptdi.legacy_io_out_rva = 0x000ca09b;
            table.push_back(std::move(entry));
        }

        {
            BuiltInTargetProfile entry = MakeChdCompatibilityProfile(
                "ez2dj6th",
                "EZ2DJ 6th Trax",
                "roms/ez2dj6th",
                "EZ2DJ/EZ2DJ.EXE",
                "The supplied 6th CHD uses EZ2DJ/EZ2DJ.EXE as a bootstrap and "
                "EZ2DJ/EZ2DJ6th.EXE as the game executable. The shared CHD, "
                "VFS, graphics, audio, and device boundaries remain a 4th-based "
                "compatibility baseline; 6th raw-I/O helper RVAs and Hardlock "
                "responses are not confirmed.");
            entry.profile.run_defaults.follow_child_process = true;
            entry.profile.run_defaults.run_detached = false;
            entry.profile.run_defaults.lptdi.legacy_io_ports = true;
            entry.profile.run_defaults.lptdi.legacy_io_ports_default = true;
            entry.profile.run_defaults.lptdi.legacy_io_port_range_fallback = true;
            entry.profile.run_defaults.lptdi.legacy_io_in_rva = 0;
            entry.profile.run_defaults.lptdi.legacy_io_out_rva = 0;
            table.push_back(std::move(entry));
        }

        {
            // EZ2Dancer 2nd MOVE is not an EZ2DJ release. It shares the
            // Hardlock envelope and the CHD, VFS, graphics and audio
            // boundaries, but its I/O board does not match, so this entry is
            // written out rather than built from MakeChdCompatibilityProfile,
            // whose executable name, siblings and port-helper RVAs all belong
            // to 4th. See docs/analysis/ez2d2m-chd-filesystem.md.
            BuiltInTargetProfile entry;
            entry.profile.id = "ez2d2m";
            entry.profile.display_name = "EZ2Dancer 2nd MOVE";
            entry.profile.hle_profile_id = "ez2d2m";
            entry.profile.run_defaults.hdd_input_kind = HddInputKind::kMameChd;
            entry.profile.run_defaults.default_hdd_image_relative_path = "roms/ez2d2m";
            entry.profile.run_defaults.audio_gain_db = 0.0f;
            // Every setting below follows this executable's own packed import
            // directory, which is what the launcher searches for IAT slots.
            //
            // GetCommandLineA, GetWindowsDirectoryA and GetPrivateProfileIntA
            // are all absent from it, so those three boundaries cannot be
            // prepared at all and stay off.
            entry.profile.run_defaults.hle_command_line = false;
            entry.profile.run_defaults.hle_windows_directory = false;
            entry.profile.run_defaults.demo_volume.reset();
            entry.profile.run_defaults.hle_vfs = true;
            // Same .protect family as 1st, 1st SE, 3rd, 4th and 5th: the
            // protection reaches CreateFileA through GetProcAddress, so the
            // static slots alone never see its device work.
            entry.profile.run_defaults.hle_dynamic_vfs = true;
            // DDRAW.dll contributes DirectDrawCreateEx rather than
            // DirectDrawCreate, which the launcher's IAT lookup already
            // accepts. The image ships DirectX 7.0a, which agrees.
            entry.profile.run_defaults.hle_d3d3 = true;
            // DSOUND.dll ordinal 1 is present.
            entry.profile.run_defaults.hle_directsound = true;
            // Confirmed from this executable's strings: it carries
            // \\.\HARDLOCK.VXD and \\.\FEnteDev, HLW32Proc and API_1LNM.DLL,
            // exactly like the EZ2DJ builds of the same envelope.
            entry.profile.run_defaults.lptdi.device_mock_path_prefix =
                "\\\\.\\FEnteDev";
            entry.profile.run_defaults.lptdi.device_mock_enabled = true;
            entry.profile.run_defaults.lptdi.hardlock_cfg_material_default = true;
            // The same envelope's WTSQuerySessionInformationA path is present
            // in this build's strings. Whether this build stops without the
            // active-console report has not been observed here.
            entry.profile.run_defaults.hle_wts_active_console = true;
            // This board is word-wide over ports 0x300 to 0x30c, which is
            // confirmed rather than inherited: past the Hardlock protection the
            // guest faults on an untrapped `out dx, ax` whose bytes are 66 ef,
            // writing port 0x030a. See docs/analysis/ez2dancer-io-map.md.
            entry.profile.run_defaults.lptdi.legacy_io_ports = true;
            entry.profile.run_defaults.lptdi.legacy_io_ports_default = true;
            entry.profile.run_defaults.lptdi.legacy_io_width =
                LegacyIoWidth::kWord;
            // The address of that instruction's first byte, which is its 0x66
            // prefix. The runtime matches the faulting address, so this is the
            // prefix rather than the helper's entry point.
            entry.profile.run_defaults.lptdi.legacy_io_out_rva = 0x0000b565;
            // The input helper's address is not known yet: output was reached
            // first and execution stopped there. Zero leaves that direction to
            // opcode matching, which the pinned width makes unambiguous.
            entry.profile.run_defaults.lptdi.legacy_io_in_rva = 0;
            entry.profile.run_defaults.run_detached = true;
            // The image is a Windows 98 SE boot disk whose MSDOS.SYS reads
            // HostWinBootDrv=C, and the game sits at that volume's root.
            entry.profile.guest_drive_letter = 'C';
            entry.profile.guest_directory = "\\ez2dancer";
            entry.profile.executable_relative_path = "ez2dancer/EZ2Dancer.exe";
            entry.profile.note =
                "EZ2Dancer 2nd MOVE, not an EZ2DJ release. The supplied CHD is a "
                "FAT32 volume labelled EZ2DANCER holding a Windows 98 SE install "
                "whose game is ez2dancer/EZ2Dancer.exe. That executable is the "
                "same .protect Hardlock family as the EZ2DJ builds - it carries "
                "\\\\.\\FEnteDev, HLW32Proc and API_1LNM.DLL - but its graphics "
                "entry point is DirectDrawCreateEx rather than DirectDrawCreate, "
                "and its cabinet I/O is the only word-wide board here, over "
                "ports 0x300 to 0x30c. Its descriptor reports module_address "
                "0x4c5e, and with local Hardlock material it passes the "
                "protection and runs original .text: it writes an eight-step "
                "lamp sequence to port 0x30a through the confirmed helper at "
                "RVA 0x0000b565, reads its own EZ2Dancer.ini, and then returns "
                "0 from WinMain and exits through the CRT before opening any "
                "asset. Its last Hardlock request is a seven-block Function "
                "0x0011 API_CODE transform that no response row answers; "
                "whether the game gates on that answer is unresolved. It never "
                "reads an input port, so the input helper RVA stays unknown.";
            entry.fingerprint.executable_name = "EZ2Dancer.exe";
            entry.fingerprint.entry_point_rva = 0x00401240;
            entry.fingerprint.size_of_image = 0x0043b000;
            entry.fingerprint.required_siblings = {
                "EZ2DANCER.ini", "song.ini", "fontkr.dat", "fonten.dat", "Songs", "SYSTEM"};
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
            if (!FingerprintMatches(root, candidate.fingerprint, entry))
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

std::uint32_t ArmedAutoplayFlagRva(const GameControls& controls,
                                   std::uint32_t executable_timestamp)
{
    if (controls.autoplay_flag_rva == 0 || controls.build_timestamp == 0 ||
        controls.build_timestamp != executable_timestamp)
    {
        return 0;
    }
    return controls.autoplay_flag_rva;
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
