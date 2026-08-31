#include "re2dj/target/target_profile.h"

#include <cstdint>
#include <filesystem>
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
        RE2DJ_CHECK(context, !entry.fingerprint.executable_name.empty());
        RE2DJ_CHECK(context, !entry.fingerprint.required_siblings.empty());
        // A built-in profile never ships a path; it is filled in from the match.
        RE2DJ_CHECK(context, entry.profile.executable_relative_path.empty());
    }

    // ---- The 1st Tracks Special Edition ----
    {
        const TemporaryTree tree;
        WriteFirstSeLayout(tree, "");

        std::vector<re2dj::target::TargetProfile> profiles;
        RE2DJ_CHECK(context, OpenAndBuild(tree, &profiles));
        RE2DJ_CHECK_EQ(context, profiles.size(), std::size_t{4});

        // The defect this design fixes: the default must be the game, not the
        // larger service tool the size-ordered scan used to put first.
        if (!profiles.empty())
        {
            RE2DJ_CHECK_EQ(context, profiles.front().id, std::string("ez2dj1stse"));
            RE2DJ_CHECK_EQ(context, profiles.front().executable_relative_path,
                           std::string("ez2dj.exe"));
        }

        const re2dj::target::TargetProfile* canonical = Find(profiles, "ez2dj1stse");
        RE2DJ_CHECK(context, canonical != nullptr);
        if (canonical != nullptr)
        {
            RE2DJ_CHECK_EQ(
                context, canonical->display_name,
                std::string("EZ2DJ The 1st Tracks Special Edition"));
            RE2DJ_CHECK(context, !canonical->detected);
            RE2DJ_CHECK(context, !canonical->bring_up_target);
            RE2DJ_CHECK(context, canonical->run_defaults.lptdi.legacy_io_ports);
            RE2DJ_CHECK(context, canonical->run_defaults.lptdi.device_mock_enabled);
            RE2DJ_CHECK_EQ(context,
                           canonical->run_defaults.lptdi.device_mock_path_prefix,
                           std::string("\\\\.\\LPTDI"));
            RE2DJ_CHECK_EQ(context,
                           canonical->run_defaults.lptdi.device_mock_target_state_hex,
                           std::string("0900000000000000"));
            // Confirmed from the System.ini shell entry in the real dump.
            RE2DJ_CHECK_EQ(context, canonical->guest_drive_letter, 'D');
            RE2DJ_CHECK_EQ(context, canonical->guest_directory, std::string("\\ez2dj"));
            RE2DJ_CHECK(context, canonical->working_directory_relative_path.empty());
            RE2DJ_CHECK(context, !canonical->note.empty());
        }

        const re2dj::target::TargetProfile* unpacked = Find(profiles, "ez2dj1stse_unpacked");
        RE2DJ_CHECK(context, unpacked != nullptr);
        if (unpacked != nullptr)
        {
            RE2DJ_CHECK_EQ(
                context, unpacked->display_name,
                std::string(
                    "EZ2DJ The 1st Tracks Special Edition (unprotected build)"));
            RE2DJ_CHECK_EQ(context, unpacked->executable_relative_path,
                           std::string("ez2dj1.exe"));
            // Marked so behavior seen through it is never cited as original.
            RE2DJ_CHECK(context, unpacked->bring_up_target);
        }

        // The two executables no built-in claimed remain available by detection.
        const re2dj::target::TargetProfile* service = Find(profiles, "test");
        RE2DJ_CHECK(context, service != nullptr);
        if (service != nullptr)
        {
            RE2DJ_CHECK(context, service->detected);
        }
        RE2DJ_CHECK(context, Find(profiles, "plzpoweroff") != nullptr);

        // A built-in match must not also appear as a detected duplicate.
        RE2DJ_CHECK(context, Find(profiles, "ez2dj") == nullptr);
        RE2DJ_CHECK(context, Find(profiles, "ez2dj1") == nullptr);

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

        const re2dj::target::TargetProfile* third = Find(profiles, "ez2dj3rd");
        RE2DJ_CHECK(context, third != nullptr);
        if (third != nullptr)
        {
            RE2DJ_CHECK_EQ(context, third->executable_relative_path,
                           std::string("EZ2DJ.EXE"));
            RE2DJ_CHECK(context, !third->detected);
            // This dump has no System.ini, so the guest path stays unknown
            // rather than being copied from the 1st SE profile.
            RE2DJ_CHECK_EQ(context, third->guest_drive_letter, '\0');
            RE2DJ_CHECK(context, third->guest_directory.empty());
            RE2DJ_CHECK_EQ(context,
                           third->run_defaults.default_hdd_directory_relative_path,
                           std::string("roms/ez2dj3rd"));
            RE2DJ_CHECK_EQ(context, third->hle_profile_id, std::string("ez2dj3rd"));
            RE2DJ_CHECK(context, third->run_defaults.hle_vfs);
            RE2DJ_CHECK(context, third->run_defaults.hle_directsound);
            RE2DJ_CHECK(context, third->run_defaults.run_detached);
            RE2DJ_CHECK(context, !third->run_defaults.fullscreen);
            RE2DJ_CHECK(context, !third->run_defaults.hle_command_line);
            RE2DJ_CHECK(context, !third->run_defaults.hle_windows_directory);
            RE2DJ_CHECK(context, !third->run_defaults.hle_d3d3);
            RE2DJ_CHECK(context, !third->run_defaults.lptdi.legacy_io_ports);
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
    {
        const TemporaryTree tree;
        WriteFirstSeLayout(tree, "se/ez2dj/");

        std::vector<re2dj::target::TargetProfile> profiles;
        RE2DJ_CHECK(context, OpenAndBuild(tree, &profiles));

        const re2dj::target::TargetProfile* canonical = Find(profiles, "ez2dj1stse");
        RE2DJ_CHECK(context, canonical != nullptr);
        if (canonical != nullptr)
        {
            RE2DJ_CHECK_EQ(context, canonical->executable_relative_path,
                           std::string("se/ez2dj/ez2dj.exe"));
            // The working directory follows the executable, not the root.
            RE2DJ_CHECK_EQ(context, canonical->working_directory_relative_path,
                           std::string("se/ez2dj"));
        }
    }

    // ---- An incomplete dump must not claim a built-in profile ----
    {
        const TemporaryTree tree;
        WriteFirstSeLayout(tree, "");
        // Songs/ is part of both 1st SE fingerprints, so removing it must drop
        // both rather than matching on the executable name alone.
        std::error_code code;
        std::filesystem::remove_all(tree.root() / "Songs", code);

        std::vector<re2dj::target::TargetProfile> profiles;
        RE2DJ_CHECK(context, OpenAndBuild(tree, &profiles));
        RE2DJ_CHECK(context, Find(profiles, "ez2dj1stse") == nullptr);
        RE2DJ_CHECK(context, Find(profiles, "ez2dj1stse_unpacked") == nullptr);
        // Detection still offers everything it found.
        RE2DJ_CHECK_EQ(context, profiles.size(), std::size_t{4});
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
        RE2DJ_CHECK(context, Find(profiles, "EZ2DJ3RD") != nullptr);
        RE2DJ_CHECK(context, Find(profiles, "missing") == nullptr);
    }
}
