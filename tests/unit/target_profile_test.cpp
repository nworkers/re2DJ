#include "re2dj/target/target_profile.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#include "re2dj/exe/pe_image.h"
#include "re2dj/hdd/hdd_root.h"
#include "re2dj/hdd/hdd_scan.h"
#include "synthetic_pe32.h"
#include "temporary_tree.h"
#include "test_support.h"

namespace
{

using re2dj::test::TemporaryTree;

std::vector<std::uint8_t> GuestExecutable()
{
    return re2dj::test::MakeSyntheticPe32Image();
}

std::vector<std::uint8_t> FirstRepresentativeExecutable()
{
    std::vector<std::uint8_t> bytes = GuestExecutable();
    re2dj::test::PutU32(bytes, re2dj::test::kSyntheticOptionalOffset + 16,
                        0x0199b240);
    re2dj::test::PutU32(bytes, re2dj::test::kSyntheticOptionalOffset + 56,
                        0x019b6000);
    return bytes;
}

std::vector<std::uint8_t> SecondRepresentativeExecutable()
{
    std::vector<std::uint8_t> bytes = GuestExecutable();
    re2dj::test::PutU32(bytes, re2dj::test::kSyntheticOptionalOffset + 16,
                        0x00079550);
    re2dj::test::PutU32(bytes, re2dj::test::kSyntheticOptionalOffset + 56,
                        0x0047d000);
    return bytes;
}

// Mirrors the confirmed EZ2DJ The 1st Tracks Special Edition layout. Only the entries
// the fingerprint names are created; the real dump holds thousands more.
void WriteFirstSeLayout(const TemporaryTree& tree, const std::string& prefix)
{
    tree.WriteBytes(prefix + "ez2dj.exe", GuestExecutable());
    tree.WriteBytes(prefix + "ez2dj1.exe", GuestExecutable());
    tree.WriteBytes(prefix + "Test.exe", GuestExecutable());
    tree.WriteBytes(prefix + "PlzPowerOff.exe", GuestExecutable());
    tree.WriteText(prefix + "ez2dj.ini", "[DIFFICULTY]\n");
    tree.WriteText(prefix + "System.ini", "[boot]\nshell=d:\\ez2dj\\ez2dj.exe\n");
    tree.WriteText(prefix + "Songs/_3week/placeholder", "x");
    tree.WriteText(prefix + "System/Title/placeholder", "x");
}

void WriteFirstRepresentativeLayout(const TemporaryTree& tree,
                                    const std::string& prefix)
{
    tree.WriteBytes(prefix + "ez2dj/Ez2DJ.exe", FirstRepresentativeExecutable());
}

// Mirrors the confirmed EZ2DJ 2nd Trax layout. Only the entries the
// fingerprint names are created; the real dump holds thousands more.
void WriteSecondLayout(const TemporaryTree& tree, const std::string& prefix)
{
    tree.WriteBytes(prefix + "ez2dj/EZ2DJ.exe", SecondRepresentativeExecutable());
    tree.WriteText(prefix + "ez2dj/EZ2DJ.ini", "[DIFFICULTY]\n");
    tree.WriteText(prefix + "ez2dj/bg/placeholder", "x");
    tree.WriteText(prefix + "ez2dj/sound/placeholder", "x");
    tree.WriteText(prefix + "ez2dj/system/placeholder", "x");
}

// Mirrors the confirmed EZ2DJ 3rd Trax layout. Note the executable name differs
// from 1st SE only in case, which is exactly what the sibling entries have to
// disambiguate.
void WriteThirdLayout(const TemporaryTree& tree, const std::string& prefix)
{
    tree.WriteBytes(prefix + "EZ2DJ.EXE", GuestExecutable());
    tree.WriteText(prefix + "EZ2DJ.INI", "\"FullScreen\" = 1\n");
    tree.WriteText(prefix + "FONTKR.DAT", "font");
    tree.WriteText(prefix + "FONTEN.DAT", "font");
    tree.WriteText(prefix + "BG/placeholder", "x");
    tree.WriteText(prefix + "Sound/placeholder", "x");
    tree.WriteText(prefix + "system/Common/placeholder", "x");
}

bool OpenAndBuild(const TemporaryTree& tree,
                  std::vector<re2dj::target::TargetProfile>* profiles)
{
    re2dj::hdd::HddRoot root;
    std::string error;
    if (!re2dj::hdd::HddRoot::Open(tree.root(), &root, &error))
    {
        return false;
    }
    const re2dj::hdd::HddScanResult scan = re2dj::hdd::ScanHdd(root);
    *profiles = re2dj::target::BuildTargetProfiles(root, scan);
    return true;
}

const re2dj::target::TargetProfile* Find(
    const std::vector<re2dj::target::TargetProfile>& profiles, const char* id)
{
    return re2dj::target::FindTargetProfileById(profiles, id);
}

}  // namespace

void RunTargetProfileTests(re2dj::test::Context& context)
{
    RE2DJ_CHECK_EQ(context, re2dj::target::MakeProfileId("EZ2DJ/Ez2dj.exe"), std::string("ez2dj"));
    RE2DJ_CHECK_EQ(context, re2dj::target::MakeProfileId("EZ2DJ.EXE"), std::string("ez2dj"));
    RE2DJ_CHECK_EQ(context, re2dj::target::MakeProfileId("a/b/EZ2DJ 4th.exe"),
                   std::string("ez2dj_4th"));
    RE2DJ_CHECK_EQ(context, re2dj::target::MakeProfileId("dir/.exe"), std::string("_exe"));

    // Every built-in entry must be usable: an id, a display name, and a
    // fingerprint that names an executable.
    const std::vector<re2dj::target::BuiltInTargetProfile>& built_ins =
        re2dj::target::GetBuiltInTargetProfiles();
    RE2DJ_CHECK(context, !built_ins.empty());
    for (const re2dj::target::BuiltInTargetProfile& entry : built_ins)
    {
        RE2DJ_CHECK(context, !entry.profile.id.empty());
        RE2DJ_CHECK(context, !entry.profile.display_name.empty());
        if (entry.profile.run_defaults.hdd_input_kind ==
            re2dj::target::HddInputKind::kDirectory)
        {
            RE2DJ_CHECK(context, !entry.fingerprint.executable_name.empty());
        }
        // Directory profiles are filled from fingerprint matching. CHD
        // shortcuts carry a confirmed internal executable path directly.
        if (entry.profile.run_defaults.hdd_input_kind ==
            re2dj::target::HddInputKind::kDirectory)
        {
            RE2DJ_CHECK(context, entry.profile.executable_relative_path.empty());
        }
        else
        {
            RE2DJ_CHECK(context, !entry.profile.executable_relative_path.empty());
        }
    }

    // ---- The 1st Tracks representative-only profile ----
    {
        const re2dj::target::BuiltInTargetProfile* first =
            re2dj::target::FindBuiltInTargetProfileById("ez2dj1st");
        RE2DJ_CHECK(context, first != nullptr);
        if (first != nullptr)
        {
            RE2DJ_CHECK_EQ(context, first->profile.display_name,
                           std::string("EZ2DJ The 1st Tracks"));
            RE2DJ_CHECK_EQ(context, first->profile.hle_profile_id,
                           std::string("ez2dj1st"));
            RE2DJ_CHECK_EQ(
                context,
                first->profile.run_defaults.default_hdd_directory_relative_path,
                std::string("roms/ez2dj1st"));
            RE2DJ_CHECK_EQ(context, first->fingerprint.executable_name,
                           std::string_view("Ez2DJ.exe"));
            RE2DJ_CHECK_EQ(context, first->fingerprint.entry_point_rva,
                           std::optional<std::uint32_t>{0x0199b240});
            RE2DJ_CHECK_EQ(context, first->fingerprint.size_of_image,
                           std::optional<std::uint32_t>{0x019b6000});
            RE2DJ_CHECK(context, first->fingerprint.required_siblings.empty());
            RE2DJ_CHECK(context, first->profile.run_defaults.hle_command_line);
            // GetWindowsDirectoryA is absent from this build's import table and
            // asking for it fails the whole handoff preparation.
            RE2DJ_CHECK(context, !first->profile.run_defaults.hle_windows_directory);
            RE2DJ_CHECK(context, first->profile.run_defaults.hle_vfs);
            // The .protect packer overwrites the static IAT patch, so the
            // device work is only reachable through the dynamic resolver.
            RE2DJ_CHECK(context, first->profile.run_defaults.hle_dynamic_vfs);
            RE2DJ_CHECK(context, first->profile.run_defaults.hle_d3d3);
            RE2DJ_CHECK(context, first->profile.run_defaults.hle_directsound);
            RE2DJ_CHECK(context, first->profile.run_defaults.lptdi.legacy_io_ports);
            RE2DJ_CHECK(context,
                        first->profile.run_defaults.lptdi.legacy_io_ports_default);
            // The device trace shows \\.\FEnteDev, never \\.\LPTDI, so the
            // LPTDI target-state probe has nothing to answer here.
            RE2DJ_CHECK_EQ(
                context, first->profile.run_defaults.lptdi.device_mock_path_prefix,
                std::string("\\\\.\\FEnteDev"));
            RE2DJ_CHECK(
                context,
                first->profile.run_defaults.lptdi.device_mock_target_state_hex.empty());
            RE2DJ_CHECK(
                context,
                first->profile.run_defaults.lptdi.hardlock_cfg_material_default);
            RE2DJ_CHECK(context, first->profile.guest_drive_letter == '\0');
            RE2DJ_CHECK(context, first->profile.guest_directory.empty());
        }

        const TemporaryTree tree;
        WriteFirstRepresentativeLayout(tree, "");

        std::vector<re2dj::target::TargetProfile> profiles;
        RE2DJ_CHECK(context, OpenAndBuild(tree, &profiles));
        RE2DJ_CHECK_EQ(context, profiles.size(), std::size_t{1});
        const re2dj::target::TargetProfile* matched = Find(profiles, "ez2dj1st");
        RE2DJ_CHECK(context, matched != nullptr);
        if (matched != nullptr)
        {
            RE2DJ_CHECK_EQ(context, matched->executable_relative_path,
                           std::string("ez2dj/Ez2DJ.exe"));
            RE2DJ_CHECK_EQ(context, matched->working_directory_relative_path,
                           std::string("ez2dj"));
            RE2DJ_CHECK(context, !matched->detected);
        }
    }

    // ---- 2nd Trax directory profile ----
    {
        const re2dj::target::BuiltInTargetProfile* second =
            re2dj::target::FindBuiltInTargetProfileById("EZ2DJ2ND");
        RE2DJ_CHECK(context, second != nullptr);
        if (second != nullptr)
        {
            RE2DJ_CHECK_EQ(context, second->profile.display_name,
                           std::string("EZ2DJ 2nd Trax"));
            RE2DJ_CHECK_EQ(context, second->profile.hle_profile_id,
                           std::string("ez2dj2nd"));
            RE2DJ_CHECK_EQ(
                context,
                second->profile.run_defaults.default_hdd_directory_relative_path,
                std::string("roms/ez2dj2nd"));
            RE2DJ_CHECK_EQ(context, second->fingerprint.executable_name,
                           std::string_view("EZ2DJ.exe"));
            RE2DJ_CHECK_EQ(context, second->fingerprint.entry_point_rva,
                           std::optional<std::uint32_t>{0x00079550});
            RE2DJ_CHECK_EQ(context, second->fingerprint.size_of_image,
                           std::optional<std::uint32_t>{0x0047d000});
            RE2DJ_CHECK_EQ(context, second->fingerprint.required_siblings.size(),
                           std::size_t{4});
            RE2DJ_CHECK(context, second->profile.run_defaults.hle_command_line);
            RE2DJ_CHECK(context,
                        second->profile.run_defaults.hle_windows_directory);
            RE2DJ_CHECK(context, second->profile.run_defaults.hle_vfs);
            RE2DJ_CHECK(context, second->profile.run_defaults.hle_d3d3);
            RE2DJ_CHECK(context, second->profile.run_defaults.hle_directsound);
            RE2DJ_CHECK(context, second->profile.run_defaults.lptdi.legacy_io_ports);
            RE2DJ_CHECK(context,
                        second->profile.run_defaults.lptdi.legacy_io_ports_default);
            RE2DJ_CHECK_EQ(
                context,
                second->profile.run_defaults.lptdi.legacy_io_in_rva,
                std::uintptr_t{0x000782d7});
            RE2DJ_CHECK_EQ(
                context,
                second->profile.run_defaults.lptdi.legacy_io_out_rva,
                std::uintptr_t{0x0007832b});
            RE2DJ_CHECK(context, !second->profile.run_defaults.demo_volume.has_value());
            RE2DJ_CHECK_EQ(
                context,
                second->profile.run_defaults.lptdi.device_mock_target_state_hex,
                std::string("0900000000000000"));
            // The 2nd dump has no System.ini, so its guest boot path stays
            // unresolved instead of inheriting the 1st SE path.
            RE2DJ_CHECK(context, second->profile.guest_drive_letter == '\0');
            RE2DJ_CHECK(context, second->profile.guest_directory.empty());
        }

        const TemporaryTree tree;
        WriteSecondLayout(tree, "");

        std::vector<re2dj::target::TargetProfile> profiles;
        RE2DJ_CHECK(context, OpenAndBuild(tree, &profiles));
        RE2DJ_CHECK_EQ(context, profiles.size(), std::size_t{1});
        const re2dj::target::TargetProfile* matched = Find(profiles, "ez2dj2nd");
        RE2DJ_CHECK(context, matched != nullptr);
        if (matched != nullptr)
        {
            RE2DJ_CHECK_EQ(context, matched->executable_relative_path,
                           std::string("ez2dj/EZ2DJ.exe"));
            RE2DJ_CHECK_EQ(context, matched->working_directory_relative_path,
                           std::string("ez2dj"));
            RE2DJ_CHECK(context, !matched->detected);
        }
    }

    // ---- 4th Trax CHD shortcut ----
    {
        const re2dj::target::BuiltInTargetProfile* fourth =
            re2dj::target::FindBuiltInTargetProfileById("EZ2DJ4TH");
        RE2DJ_CHECK(context, fourth != nullptr);
        if (fourth != nullptr)
        {
            RE2DJ_CHECK_EQ(context, fourth->profile.display_name,
                           std::string("EZ2DJ 4th (MAME CHD HDD)"));
            RE2DJ_CHECK(context,
                        fourth->profile.run_defaults.hdd_input_kind ==
                            re2dj::target::HddInputKind::kMameChd);
            RE2DJ_CHECK_EQ(context,
                           fourth->profile.run_defaults.default_hdd_image_relative_path,
                           std::string("roms/ez2dj4th"));
            RE2DJ_CHECK_EQ(context, fourth->profile.executable_relative_path,
                           std::string("EZ2DJ/EZ2DJ.EXE"));
            RE2DJ_CHECK(context,
                        fourth->profile.run_defaults.default_hdd_directory_relative_path.empty());
            RE2DJ_CHECK_EQ(context, fourth->fingerprint.executable_name,
                           std::string_view("EZ2DJ.EXE"));
            RE2DJ_CHECK(context, !fourth->fingerprint.required_siblings.empty());
            RE2DJ_CHECK(context, fourth->profile.run_defaults.hle_vfs);
            RE2DJ_CHECK(context, fourth->profile.run_defaults.hle_dynamic_vfs);
            RE2DJ_CHECK(context, fourth->profile.run_defaults.lptdi.hardlock_cfg_material_default);
            RE2DJ_CHECK(context, fourth->profile.run_defaults.lptdi.device_mock_enabled);
            RE2DJ_CHECK_EQ(context,
                           fourth->profile.run_defaults.lptdi.device_mock_path_prefix,
                           std::string("\\\\.\\FEnteDev"));
            // Without these the launcher terminates the original at the first
            // VFS file open and the protection stops after its first device
            // request. Both are product policy.
            RE2DJ_CHECK(context, fourth->profile.run_defaults.run_detached);
            RE2DJ_CHECK(context, fourth->profile.run_defaults.hle_wts_console_session);
            RE2DJ_CHECK(context,
                        fourth->profile.run_defaults.lptdi
                            .device_mock_target_state_hex.empty());
        }
    }

    // ---- 5th/6th Trax CHD compatibility profiles ----
    const auto check_chd_compatibility_profile =
        [&context](const char* id,
                   const char* display_name,
                   const char* image_path,
                   const char* executable_path,
                   bool legacy_io,
                   // The port helpers live at a different place in each
                   // build's .text, so the caller states the pair it expects
                   // rather than sharing one baseline.
                   std::uint32_t expected_in_rva,
                   std::uint32_t expected_out_rva,
                   bool follow_child,
                   bool run_detached) {
            const re2dj::target::BuiltInTargetProfile* profile =
                re2dj::target::FindBuiltInTargetProfileById(id);
            RE2DJ_CHECK(context, profile != nullptr);
            if (profile == nullptr)
            {
                return;
            }
            RE2DJ_CHECK_EQ(context, profile->profile.display_name,
                           std::string(display_name));
            RE2DJ_CHECK_EQ(context, profile->profile.hle_profile_id,
                           std::string(id));
            RE2DJ_CHECK(context,
                        profile->profile.run_defaults.hdd_input_kind ==
                            re2dj::target::HddInputKind::kMameChd);
            RE2DJ_CHECK_EQ(
                context,
                profile->profile.run_defaults.default_hdd_image_relative_path,
                std::string(image_path));
            RE2DJ_CHECK_EQ(context, profile->profile.executable_relative_path,
                           std::string(executable_path));
            RE2DJ_CHECK(context,
                        profile->profile.run_defaults.default_hdd_directory_relative_path.empty());
            RE2DJ_CHECK(context, profile->profile.run_defaults.hle_vfs);
            RE2DJ_CHECK(context, profile->profile.run_defaults.hle_dynamic_vfs);
            RE2DJ_CHECK(context, profile->profile.run_defaults.hle_d3d3);
            RE2DJ_CHECK(context, profile->profile.run_defaults.hle_directsound);
            RE2DJ_CHECK_EQ(context,
                           profile->profile.run_defaults.lptdi.legacy_io_ports,
                           legacy_io);
            RE2DJ_CHECK_EQ(context,
                           profile->profile.run_defaults.lptdi.legacy_io_ports_default,
                           legacy_io);
            RE2DJ_CHECK_EQ(context,
                           profile->profile.run_defaults.lptdi.legacy_io_in_rva,
                           expected_in_rva);
            RE2DJ_CHECK_EQ(context,
                           profile->profile.run_defaults.lptdi.legacy_io_out_rva,
                           expected_out_rva);
            // Every EZ2DJ board is byte-wide; only EZ2Dancer is not.
            RE2DJ_CHECK(context, profile->profile.run_defaults.lptdi.legacy_io_width ==
                                     re2dj::target::LegacyIoWidth::kByte);
            RE2DJ_CHECK(context, profile->profile.run_defaults.lptdi.device_mock_enabled);
            RE2DJ_CHECK_EQ(context,
                           profile->profile.run_defaults.lptdi.device_mock_path_prefix,
                           std::string("\\\\.\\FEnteDev"));
            RE2DJ_CHECK(context,
                        profile->profile.run_defaults.lptdi.hardlock_cfg_material_default);
            RE2DJ_CHECK(context, profile->profile.run_defaults.hle_wts_console_session);
            RE2DJ_CHECK_EQ(context,
                           profile->profile.run_defaults.follow_child_process,
                           follow_child);
            RE2DJ_CHECK_EQ(context,
                           profile->profile.run_defaults.run_detached,
                           run_detached);
            RE2DJ_CHECK(context, !profile->profile.run_defaults.demo_volume.has_value());
            RE2DJ_CHECK_EQ(context, profile->fingerprint.executable_name,
                           std::string_view("EZ2DJ.EXE"));
            RE2DJ_CHECK_EQ(context, profile->fingerprint.required_siblings.size(),
                           std::size_t{6});
            RE2DJ_CHECK(context, !profile->profile.note.empty());
        };
    // 5th's own port helpers, read from its decrypted .text; the 4th values
    // this profile used to inherit never matched it.
    check_chd_compatibility_profile(
        "ez2dj5th", "EZ2DJ 5th Trax", "roms/ez2dj5th", "EZ2DJ/EZ2DJ.EXE", true,
        0x000ca067, 0x000ca09b, false, true);
    check_chd_compatibility_profile(
        "ez2dj6th",
        "EZ2DJ 6th Trax",
        "roms/ez2dj6th",
        "EZ2DJ/EZ2DJ.EXE",
        true,
        0,
        0,
        true,
        false);
    RE2DJ_CHECK(context,
                re2dj::target::FindBuiltInTargetProfileById("ez2dj1stse_unpacked") ==
                    nullptr);

    // ---- EZ2Dancer 2nd MOVE ----
    // This is not an EZ2DJ release, so it is checked on its own rather than
    // through the shared CHD compatibility helper: its executable, its
    // DirectDrawCreateEx graphics entry and its disabled raw I/O all differ.
    {
        const re2dj::target::BuiltInTargetProfile* dancer =
            re2dj::target::FindBuiltInTargetProfileById("ez2d2m");
        RE2DJ_CHECK(context, dancer != nullptr);
        if (dancer != nullptr)
        {
            const re2dj::target::TargetProfile& profile = dancer->profile;
            RE2DJ_CHECK_EQ(context, profile.display_name,
                           std::string("EZ2Dancer 2nd MOVE"));
            RE2DJ_CHECK(context, profile.run_defaults.hdd_input_kind ==
                                     re2dj::target::HddInputKind::kMameChd);
            RE2DJ_CHECK_EQ(context,
                           profile.run_defaults.default_hdd_image_relative_path,
                           std::string("roms/ez2d2m"));
            RE2DJ_CHECK_EQ(context, profile.executable_relative_path,
                           std::string("ez2dancer/EZ2Dancer.exe"));
            RE2DJ_CHECK_EQ(context, profile.guest_drive_letter, 'C');
            RE2DJ_CHECK_EQ(context, profile.guest_directory, std::string("\\ez2dancer"));
            RE2DJ_CHECK(context, profile.run_defaults.hle_vfs);
            RE2DJ_CHECK(context, profile.run_defaults.hle_dynamic_vfs);
            RE2DJ_CHECK(context, profile.run_defaults.hle_d3d3);
            RE2DJ_CHECK(context, profile.run_defaults.hle_directsound);
            RE2DJ_CHECK(context, profile.run_defaults.hle_wts_console_session);
            RE2DJ_CHECK(context, profile.run_defaults.run_detached);
            // Absent from the packed import directory, so none of these can be
            // prepared for this build.
            RE2DJ_CHECK(context, !profile.run_defaults.hle_command_line);
            RE2DJ_CHECK(context, !profile.run_defaults.hle_windows_directory);
            RE2DJ_CHECK(context, !profile.run_defaults.demo_volume.has_value());
            // The EZ2Dancer board is word-wide, and its output helper is
            // confirmed from the guest's own privileged fault. Its reads come
            // from two helpers (task 427), so they go by opcode.
            RE2DJ_CHECK(context, profile.run_defaults.lptdi.legacy_io_ports);
            RE2DJ_CHECK(context, profile.run_defaults.lptdi.legacy_io_ports_default);
            RE2DJ_CHECK(context, profile.run_defaults.lptdi.legacy_io_width ==
                                     re2dj::target::LegacyIoWidth::kWord);
            RE2DJ_CHECK_EQ(context, profile.run_defaults.lptdi.legacy_io_out_rva,
                           std::uint32_t{0x0000b565});
            RE2DJ_CHECK_EQ(context, profile.run_defaults.lptdi.legacy_io_in_rva,
                           std::uint32_t{0});
            RE2DJ_CHECK(context, profile.run_defaults.lptdi.device_mock_enabled);
            RE2DJ_CHECK_EQ(context, profile.run_defaults.lptdi.device_mock_path_prefix,
                           std::string("\\\\.\\FEnteDev"));
            RE2DJ_CHECK(context,
                        profile.run_defaults.lptdi.hardlock_cfg_material_default);
            RE2DJ_CHECK(context, !profile.run_defaults.follow_child_process);
            RE2DJ_CHECK(context, !profile.detected);
            RE2DJ_CHECK(context, !profile.bring_up_target);
            RE2DJ_CHECK(context, !profile.note.empty());
            RE2DJ_CHECK_EQ(context, dancer->fingerprint.executable_name,
                           std::string_view("EZ2Dancer.exe"));
            RE2DJ_CHECK(context, dancer->fingerprint.entry_point_rva.has_value());
            RE2DJ_CHECK(context, dancer->fingerprint.size_of_image.has_value());
            RE2DJ_CHECK_EQ(context, dancer->fingerprint.required_siblings.size(),
                           std::size_t{6});
        }
    }

    // ---- The 1st Tracks Special Edition CHD shortcut ----
    {
        const re2dj::target::BuiltInTargetProfile* first_se_builtin =
            re2dj::target::FindBuiltInTargetProfileById("ez2dj1stse");
        RE2DJ_CHECK(context, first_se_builtin != nullptr);
        const re2dj::target::TargetProfile* canonical =
            first_se_builtin == nullptr ? nullptr : &first_se_builtin->profile;
        if (canonical != nullptr)
        {
            RE2DJ_CHECK_EQ(
                context, canonical->display_name,
                std::string("EZ2DJ The 1st Tracks Special Edition"));
            RE2DJ_CHECK(context, !canonical->detected);
            RE2DJ_CHECK(context, !canonical->bring_up_target);
            RE2DJ_CHECK_EQ(context,
                           canonical->run_defaults.hdd_input_kind,
                           re2dj::target::HddInputKind::kMameChd);
            RE2DJ_CHECK_EQ(context,
                           canonical->run_defaults.default_hdd_image_relative_path,
                           std::string("roms/ez2dj1stse"));
            RE2DJ_CHECK_EQ(context,
                           canonical->run_defaults.default_hdd_directory_relative_path,
                           std::string("roms/ez2dj1stse"));
            RE2DJ_CHECK_EQ(context, canonical->executable_relative_path,
                           std::string("ez2dj/Ez2DJ.exe"));
            RE2DJ_CHECK_EQ(context, canonical->hle_profile_id,
                           std::string("ez2dj1stse"));
            // Each HLE default below follows what the launcher can actually
            // patch in this build. GetCommandLineA and DSOUND ordinal 1 are in
            // the packed table; DirectDrawCreate is absent from it but present
            // in the surviving original .idata, which the IAT lookup also
            // searches. GetWindowsDirectoryA and GetPrivateProfileIntA are in
            // neither, so those two boundaries stay off.
            RE2DJ_CHECK(context, canonical->run_defaults.hle_command_line);
            RE2DJ_CHECK(context, !canonical->run_defaults.hle_windows_directory);
            RE2DJ_CHECK(context, canonical->run_defaults.hle_vfs);
            RE2DJ_CHECK(context, canonical->run_defaults.hle_d3d3);
            RE2DJ_CHECK(context, canonical->run_defaults.hle_directsound);
            RE2DJ_CHECK(context, !canonical->run_defaults.demo_volume.has_value());
            // The protection resolves its device APIs through GetProcAddress.
            RE2DJ_CHECK(context, canonical->run_defaults.hle_dynamic_vfs);
            RE2DJ_CHECK(context, canonical->run_defaults.run_detached);
            RE2DJ_CHECK(context, canonical->run_defaults.lptdi.legacy_io_ports);
            RE2DJ_CHECK(context, canonical->run_defaults.lptdi.device_mock_enabled);
            // Confirmed by the device trace: this build opens \\.\FEnteDev, so
            // the LPTDI target-state probe has nothing to answer and stays unset.
            RE2DJ_CHECK_EQ(context,
                           canonical->run_defaults.lptdi.device_mock_path_prefix,
                           std::string("\\\\.\\FEnteDev"));
            RE2DJ_CHECK(context,
                        canonical->run_defaults.lptdi.device_mock_target_state_hex.empty());
            RE2DJ_CHECK(context,
                        canonical->run_defaults.lptdi.hardlock_cfg_material_default);
            // Runs with and without the console-session report behaved
            // identically, so it stays off rather than being copied from 3rd.
            RE2DJ_CHECK(context, !canonical->run_defaults.hle_wts_console_session);
            RE2DJ_CHECK_EQ(context,
                           canonical->run_defaults.lptdi.legacy_io_in_rva,
                           std::uintptr_t{0x00038987});
            RE2DJ_CHECK_EQ(context,
                           canonical->run_defaults.lptdi.legacy_io_out_rva,
                           std::uintptr_t{0x000389ab});
            // The CHD boots the game from a StartUp shortcut targeting
            // C:\ez2dj\Ez2DJ.exe, so the drive letter is C - the extracted
            // dump's System.ini said d:, which described that input only.
            RE2DJ_CHECK_EQ(context, canonical->guest_drive_letter, 'C');
            RE2DJ_CHECK_EQ(context, canonical->guest_directory, std::string("\\ez2dj"));
            RE2DJ_CHECK(context, !canonical->note.empty());
        }

        // The extracted 1st SE dump is image-backed now, so the directory scan
        // detects its executables instead of claiming the built-in profile.
        const TemporaryTree tree;
        WriteFirstSeLayout(tree, "");

        std::vector<re2dj::target::TargetProfile> profiles;
        RE2DJ_CHECK(context, OpenAndBuild(tree, &profiles));
        RE2DJ_CHECK_EQ(context, profiles.size(), std::size_t{4});
        RE2DJ_CHECK(context, Find(profiles, "ez2dj1stse") == nullptr);
        RE2DJ_CHECK(context, Find(profiles, "ez2dj1stse_unpacked") == nullptr);

        const re2dj::target::TargetProfile* detected_game = Find(profiles, "ez2dj");
        RE2DJ_CHECK(context, detected_game != nullptr);
        if (detected_game != nullptr)
        {
            RE2DJ_CHECK_EQ(context, detected_game->executable_relative_path,
                           std::string("ez2dj.exe"));
            RE2DJ_CHECK(context, detected_game->detected);
        }

        const re2dj::target::TargetProfile* detected_unpacked = Find(profiles, "ez2dj1");
        RE2DJ_CHECK(context, detected_unpacked != nullptr);
        if (detected_unpacked != nullptr)
        {
            RE2DJ_CHECK_EQ(context, detected_unpacked->display_name,
                           std::string("ez2dj1.exe"));
            RE2DJ_CHECK_EQ(context, detected_unpacked->executable_relative_path,
                           std::string("ez2dj1.exe"));
            RE2DJ_CHECK(context, detected_unpacked->detected);
            RE2DJ_CHECK(context, !detected_unpacked->bring_up_target);
        }

        const re2dj::target::TargetProfile* service = Find(profiles, "test");
        RE2DJ_CHECK(context, service != nullptr);
        if (service != nullptr)
        {
            RE2DJ_CHECK(context, service->detected);
        }
        RE2DJ_CHECK(context, Find(profiles, "plzpoweroff") != nullptr);

        // This dump is not 3rd, even though EZ2DJ.EXE resolves to ez2dj.exe on
        // a case-insensitive host.
        RE2DJ_CHECK(context, Find(profiles, "ez2dj3rd") == nullptr);
    }

    // ---- 3rd Trax ----
    {
        const TemporaryTree tree;
        WriteThirdLayout(tree, "");

        std::vector<re2dj::target::TargetProfile> profiles;
        RE2DJ_CHECK(context, OpenAndBuild(tree, &profiles));
        RE2DJ_CHECK_EQ(context, profiles.size(), std::size_t{1});

        // The 3rd profile is image-backed now, so an extracted directory is
        // detected separately rather than claimed by the CHD shortcut.
        RE2DJ_CHECK(context, Find(profiles, "ez2dj3rd") == nullptr);
        RE2DJ_CHECK(context, Find(profiles, "ez2dj") != nullptr);
        const re2dj::target::BuiltInTargetProfile* third_builtin =
            re2dj::target::FindBuiltInTargetProfileById("ez2dj3rd");
        const re2dj::target::TargetProfile* third =
            third_builtin == nullptr ? nullptr : &third_builtin->profile;
        RE2DJ_CHECK(context, third != nullptr);
        if (third != nullptr)
        {
            RE2DJ_CHECK_EQ(context, third->executable_relative_path,
                           std::string("EZ2DJ/EZ2DJ.EXE"));
            RE2DJ_CHECK(context, !third->detected);
            // This dump has no System.ini, so the guest path stays unknown
            // rather than being copied from the 1st SE profile.
            RE2DJ_CHECK_EQ(context, third->guest_drive_letter, '\0');
            RE2DJ_CHECK(context, third->guest_directory.empty());
            RE2DJ_CHECK_EQ(context,
                           third->run_defaults.default_hdd_directory_relative_path,
                           std::string("roms/ez2dj3rd"));
            RE2DJ_CHECK_EQ(context,
                           third->run_defaults.hdd_input_kind,
                           re2dj::target::HddInputKind::kMameChd);
            RE2DJ_CHECK_EQ(context,
                           third->run_defaults.default_hdd_image_relative_path,
                           std::string("roms/ez2dj3rd"));
            RE2DJ_CHECK_EQ(context, third->hle_profile_id, std::string("ez2dj3rd"));
            RE2DJ_CHECK(context, third->run_defaults.hle_vfs);
            RE2DJ_CHECK(context, third->run_defaults.hle_directsound);
            RE2DJ_CHECK(context, third->run_defaults.run_detached);
            // The console-session policy is confirmed for 3rd as well: its
            // protection initialization reads the current session ID, and
            // reporting session 0 advances execution from 0x9c402468 to
            // 0x9c402450. It stays absent from profiles with no such evidence.
            RE2DJ_CHECK(context, third->run_defaults.hle_wts_console_session);
            RE2DJ_CHECK(context, third->run_defaults.hle_dynamic_vfs);
            RE2DJ_CHECK(context, third->run_defaults.lptdi.hardlock_cfg_material_default);
            RE2DJ_CHECK(context, !third->run_defaults.fullscreen);
            RE2DJ_CHECK(context, !third->run_defaults.hle_command_line);
            RE2DJ_CHECK(context, !third->run_defaults.hle_windows_directory);
            RE2DJ_CHECK(context, third->run_defaults.hle_d3d3);
            RE2DJ_CHECK(context, third->run_defaults.lptdi.legacy_io_ports);
            RE2DJ_CHECK(context, third->run_defaults.lptdi.legacy_io_ports_default);
            RE2DJ_CHECK_EQ(context,
                           third->run_defaults.lptdi.legacy_io_in_rva,
                           0x000a9887u);
            RE2DJ_CHECK_EQ(context,
                           third->run_defaults.lptdi.legacy_io_out_rva,
                           0x000a98bbu);
            RE2DJ_CHECK(context,
                        !third->run_defaults.lptdi.legacy_io_port_range_fallback);
            RE2DJ_CHECK(context, third->run_defaults.lptdi.device_mock_enabled);
            RE2DJ_CHECK_EQ(context,
                           third->run_defaults.lptdi.device_mock_path_prefix,
                           std::string("\\\\.\\FEnteDev"));
            RE2DJ_CHECK(context,
                        third->run_defaults.lptdi.device_mock_target_state_hex ==
                            "0000000000000000");
            RE2DJ_CHECK(context, !third->run_defaults.demo_volume.has_value());
        }

        RE2DJ_CHECK(context, Find(profiles, "ez2dj1stse") == nullptr);
        RE2DJ_CHECK(context, Find(profiles, "ez2dj1stse_unpacked") == nullptr);
    }

    // ---- A nested root, as when the user points at a parent directory ----
    // 2nd is the directory-matched profile this exercises; 1st SE and 3rd are
    // image-backed and never reached through a directory scan.
    {
        const TemporaryTree tree;
        WriteSecondLayout(tree, "se/");

        std::vector<re2dj::target::TargetProfile> profiles;
        RE2DJ_CHECK(context, OpenAndBuild(tree, &profiles));

        const re2dj::target::TargetProfile* canonical = Find(profiles, "ez2dj2nd");
        RE2DJ_CHECK(context, canonical != nullptr);
        if (canonical != nullptr)
        {
            RE2DJ_CHECK_EQ(context, canonical->executable_relative_path,
                           std::string("se/ez2dj/EZ2DJ.exe"));
            // The working directory follows the executable, not the root.
            RE2DJ_CHECK_EQ(context, canonical->working_directory_relative_path,
                           std::string("se/ez2dj"));
        }
    }

    // ---- An incomplete dump must not claim a built-in profile ----
    {
        const TemporaryTree tree;
        WriteSecondLayout(tree, "");
        // bg/ is part of the 2nd fingerprint, so removing it must drop the
        // built-in match rather than matching on the executable name alone.
        std::error_code code;
        std::filesystem::remove_all(tree.root() / "ez2dj" / "bg", code);

        std::vector<re2dj::target::TargetProfile> profiles;
        RE2DJ_CHECK(context, OpenAndBuild(tree, &profiles));
        RE2DJ_CHECK(context, Find(profiles, "ez2dj2nd") == nullptr);
        // Detection still offers everything it found.
        RE2DJ_CHECK_EQ(context, profiles.size(), std::size_t{1});
        RE2DJ_CHECK(context, Find(profiles, "ez2dj") != nullptr);
    }

    // ---- A dump of an unknown version still works through detection ----
    {
        const TemporaryTree tree;
        tree.WriteBytes("GAME/Unknown.exe", GuestExecutable());
        tree.WriteText("GAME/data/placeholder", "x");

        std::vector<re2dj::target::TargetProfile> profiles;
        RE2DJ_CHECK(context, OpenAndBuild(tree, &profiles));
        RE2DJ_CHECK_EQ(context, profiles.size(), std::size_t{1});
        if (!profiles.empty())
        {
            RE2DJ_CHECK_EQ(context, profiles.front().id, std::string("unknown"));
            RE2DJ_CHECK(context, profiles.front().detected);
            RE2DJ_CHECK_EQ(context, profiles.front().working_directory_relative_path,
                           std::string("GAME"));
        }
    }

    // ---- Duplicate executable names keep distinct ids ----
    {
        const TemporaryTree tree;
        tree.WriteBytes("Game.exe", GuestExecutable());
        tree.WriteBytes("BACKUP/Game.exe", GuestExecutable());

        std::vector<re2dj::target::TargetProfile> profiles;
        RE2DJ_CHECK(context, OpenAndBuild(tree, &profiles));
        RE2DJ_CHECK_EQ(context, profiles.size(), std::size_t{2});
        RE2DJ_CHECK(context, Find(profiles, "game") != nullptr);
        RE2DJ_CHECK(context, Find(profiles, "game_2") != nullptr);
    }

    // Lookup is case-insensitive, and a missing id yields nullptr.
    {
        const TemporaryTree tree;
        WriteThirdLayout(tree, "");
        std::vector<re2dj::target::TargetProfile> profiles;
        RE2DJ_CHECK(context, OpenAndBuild(tree, &profiles));
        RE2DJ_CHECK(context,
                    re2dj::target::FindBuiltInTargetProfileById("EZ2DJ3RD") != nullptr);
        RE2DJ_CHECK(context, Find(profiles, "missing") == nullptr);
    }

    // Each autoplay control is bound to the one build it was confirmed in, and
    // only the profiles with a confirmed flag offer one.
    {
        const auto* third = re2dj::target::FindBuiltInTargetProfileById("ez2dj3rd");
        RE2DJ_CHECK(context, third != nullptr);
        if (third != nullptr)
        {
            const re2dj::target::GameControls& controls = third->profile.game_controls;
            RE2DJ_CHECK_EQ(context, controls.autoplay_flag_rva, std::uint32_t{0x00629508});
            RE2DJ_CHECK_EQ(context, controls.build_timestamp, std::uint32_t{0x3bca98a3});
            RE2DJ_CHECK_EQ(context,
                           re2dj::target::ArmedAutoplayFlagRva(controls, 0x3bca98a3),
                           std::uint32_t{0x00629508});
            // A different build of the same product must never be armed.
            RE2DJ_CHECK_EQ(context,
                           re2dj::target::ArmedAutoplayFlagRva(controls, 0x3bca98a4),
                           std::uint32_t{0});
        }
        const auto* fourth = re2dj::target::FindBuiltInTargetProfileById("ez2dj4th");
        RE2DJ_CHECK(context, fourth != nullptr);
        if (fourth != nullptr)
        {
            const re2dj::target::GameControls& controls = fourth->profile.game_controls;
            RE2DJ_CHECK_EQ(context, controls.autoplay_flag_rva, std::uint32_t{0x006c29b0});
            RE2DJ_CHECK_EQ(context, controls.build_timestamp, std::uint32_t{0x3d369bfd});
            // The 3rd build's timestamp must not arm the 4th address, and the
            // other way round, although both are EZ2DJ.EXE.
            RE2DJ_CHECK_EQ(context,
                           re2dj::target::ArmedAutoplayFlagRva(controls, 0x3bca98a3),
                           std::uint32_t{0});
        }
        const auto* fifth = re2dj::target::FindBuiltInTargetProfileById("ez2dj5th");
        RE2DJ_CHECK(context, fifth != nullptr);
        if (fifth != nullptr)
        {
            const re2dj::target::GameControls& controls = fifth->profile.game_controls;
            RE2DJ_CHECK_EQ(context, controls.autoplay_flag_rva, std::uint32_t{0x006ee238});
            RE2DJ_CHECK_EQ(context, controls.build_timestamp, std::uint32_t{0x3f53377b});
            RE2DJ_CHECK_EQ(context,
                           re2dj::target::ArmedAutoplayFlagRva(controls, 0x3d369bfd),
                           std::uint32_t{0});
        }
        const auto* first_se = re2dj::target::FindBuiltInTargetProfileById("ez2dj1stse");
        RE2DJ_CHECK(context, first_se != nullptr);
        if (first_se != nullptr)
        {
            const re2dj::target::GameControls& controls = first_se->profile.game_controls;
            RE2DJ_CHECK_EQ(context, controls.autoplay_flag_rva, std::uint32_t{0x0183f3a4});
            RE2DJ_CHECK_EQ(context, controls.build_timestamp, std::uint32_t{0x3862df27});
        }
        const auto* dancer = re2dj::target::FindBuiltInTargetProfileById("ez2d2m");
        RE2DJ_CHECK(context, dancer != nullptr);
        if (dancer != nullptr)
        {
            const re2dj::target::GameControls& controls = dancer->profile.game_controls;
            RE2DJ_CHECK_EQ(context, controls.autoplay_flag_rva, std::uint32_t{0x003fa424});
            RE2DJ_CHECK_EQ(context, controls.build_timestamp, std::uint32_t{0x3a5f074c});
            RE2DJ_CHECK_EQ(context,
                           re2dj::target::ArmedAutoplayFlagRva(controls, 0x3862df27),
                           std::uint32_t{0});
        }
        // 1st Tracks has no switchable autoplay variable (task 304).
        for (const char* id : {"ez2dj1st", "ez2dj2nd", "ez2dj6th"})
        {
            const auto* other = re2dj::target::FindBuiltInTargetProfileById(id);
            RE2DJ_CHECK(context, other != nullptr);
            if (other != nullptr)
            {
                RE2DJ_CHECK_EQ(context, other->profile.game_controls.autoplay_flag_rva,
                               std::uint32_t{0});
            }
        }
        // A declaration missing either half arms nothing.
        re2dj::target::GameControls no_timestamp;
        no_timestamp.autoplay_flag_rva = 0x00629508;
        RE2DJ_CHECK_EQ(context, re2dj::target::ArmedAutoplayFlagRva(no_timestamp, 0),
                       std::uint32_t{0});
        re2dj::target::GameControls no_rva;
        no_rva.build_timestamp = 0x3bca98a3;
        RE2DJ_CHECK_EQ(context, re2dj::target::ArmedAutoplayFlagRva(no_rva, 0x3bca98a3),
                       std::uint32_t{0});
    }
}
