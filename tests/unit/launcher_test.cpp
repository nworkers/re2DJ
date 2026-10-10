#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "re2dj/hdd/chd_image_locator.h"
#include "re2dj/launcher/launcher_catalog.h"
#include "re2dj/launcher/launcher_settings.h"
#include "re2dj/target/target_profile.h"

#include "temporary_tree.h"
#include "test_support.h"

namespace
{

namespace hdd = re2dj::hdd;
namespace launcher = re2dj::launcher;
namespace target = re2dj::target;
using re2dj::graphics::ColorDepth;

void TestChdImageLocator(re2dj::test::Context& context)
{
    re2dj::test::TemporaryTree tree;
    tree.MakeDirectory("empty");
    tree.WriteText("one/disc.CHD", "x");
    tree.WriteText("one/readme.txt", "x");
    tree.WriteText("two/a.chd", "x");
    tree.WriteText("two/b.chd", "x");
    tree.WriteText("file.bin", "x");

    std::filesystem::path image;
    std::string error;
    RE2DJ_CHECK(context, hdd::LocateChdImage(tree.root() / "one", &image, &error) == hdd::ChdImageLookup::kFound);
    RE2DJ_CHECK_EQ(context, image.filename().string(), std::string("disc.CHD"));
    RE2DJ_CHECK(context,
                hdd::LocateChdImage(tree.root() / "one" / "disc.CHD", &image, &error) == hdd::ChdImageLookup::kFound);
    RE2DJ_CHECK(context, hdd::LocateChdImage(tree.root() / "empty", &image, &error) == hdd::ChdImageLookup::kNone);
    RE2DJ_CHECK(context, error.find("no .chd image") != std::string::npos);
    RE2DJ_CHECK(context, hdd::LocateChdImage(tree.root() / "two", &image, &error) == hdd::ChdImageLookup::kSeveral);
    RE2DJ_CHECK(context, error.find("more than one") != std::string::npos);
    RE2DJ_CHECK(context, hdd::LocateChdImage(tree.root() / "missing", &image, &error) == hdd::ChdImageLookup::kMissing);
    RE2DJ_CHECK(context,
                hdd::LocateChdImage(tree.root() / "file.bin", &image, &error) == hdd::ChdImageLookup::kNotChd);
    RE2DJ_CHECK(context, hdd::FindChdImage(tree.root() / "one", &image, &error));
    RE2DJ_CHECK(context, !hdd::FindChdImage(tree.root() / "two", &image, &error));
}

target::TargetProfile ChdProfile(const std::string& id, const std::string& image_path)
{
    target::TargetProfile profile;
    profile.id = id;
    profile.display_name = id + " title";
    profile.run_defaults.hdd_input_kind = target::HddInputKind::kMameChd;
    profile.run_defaults.default_hdd_image_relative_path = image_path;
    return profile;
}

target::TargetProfile DirectoryProfile(const std::string& id, const std::string& directory)
{
    target::TargetProfile profile;
    profile.id = id;
    profile.display_name = id + " title";
    profile.run_defaults.default_hdd_directory_relative_path = directory;
    profile.run_defaults.audio_gain_db = -6.0F;
    return profile;
}

void TestLauncherCatalog(re2dj::test::Context& context)
{
    re2dj::test::TemporaryTree tree;
    tree.WriteText("roms/ready/game.chd", "x");
    tree.WriteText("roms/several/a.chd", "x");
    tree.WriteText("roms/several/b.chd", "x");
    tree.MakeDirectory("roms/empty");
    tree.MakeDirectory("roms/dump/ez2dj");

    const launcher::LauncherEntry ready = launcher::ProbeLauncherProfile(ChdProfile("ready", "roms/ready"), tree.root());
    RE2DJ_CHECK(context, launcher::IsLauncherEntryRunnable(ready));
    RE2DJ_CHECK(context, ready.reason.empty());
    RE2DJ_CHECK_EQ(context, ready.location.filename().string(), std::string("game.chd"));
    RE2DJ_CHECK_EQ(context, ready.display_name, std::string("ready title"));
    RE2DJ_CHECK_EQ(context, ready.default_audio_gain_db, 0.0F);

    const launcher::LauncherEntry several =
        launcher::ProbeLauncherProfile(ChdProfile("several", "roms/several"), tree.root());
    RE2DJ_CHECK(context, several.availability == launcher::ProfileAvailability::kSeveralChds);
    RE2DJ_CHECK(context, several.reason.find("more than one .chd") != std::string::npos);

    const launcher::LauncherEntry empty = launcher::ProbeLauncherProfile(ChdProfile("empty", "roms/empty"), tree.root());
    RE2DJ_CHECK(context, empty.availability == launcher::ProfileAvailability::kNoChd);
    const launcher::LauncherEntry absent =
        launcher::ProbeLauncherProfile(ChdProfile("absent", "roms/absent"), tree.root());
    RE2DJ_CHECK(context, absent.availability == launcher::ProfileAvailability::kNoChd);
    RE2DJ_CHECK(context, absent.reason.find("no .chd") != std::string::npos);

    const launcher::LauncherEntry dump =
        launcher::ProbeLauncherProfile(DirectoryProfile("dump", "roms/dump"), tree.root());
    RE2DJ_CHECK(context, launcher::IsLauncherEntryRunnable(dump));
    RE2DJ_CHECK_EQ(context, dump.default_audio_gain_db, -6.0F);
    const launcher::LauncherEntry no_dump =
        launcher::ProbeLauncherProfile(DirectoryProfile("nodump", "roms/nodump"), tree.root());
    RE2DJ_CHECK(context, no_dump.availability == launcher::ProfileAvailability::kNoDirectory);
    RE2DJ_CHECK(context, no_dump.reason.find("no directory") != std::string::npos);
    const launcher::LauncherEntry no_default =
        launcher::ProbeLauncherProfile(DirectoryProfile("nodefault", ""), tree.root());
    RE2DJ_CHECK(context, no_default.availability == launcher::ProfileAvailability::kNoDirectory);

    // The built-in table: every id once, in table order, and the 6th CHD
    // found where its command-line shortcut looks.
    std::vector<launcher::LauncherEntry> catalog = launcher::BuildLauncherCatalog(tree.root());
    std::vector<std::string> ids;
    for (const launcher::LauncherEntry& entry : catalog)
    {
        ids.push_back(entry.id);
        RE2DJ_CHECK(context, !launcher::IsLauncherEntryRunnable(entry));
    }
    std::vector<std::string> unique_ids = ids;
    std::sort(unique_ids.begin(), unique_ids.end());
    RE2DJ_CHECK(context, std::adjacent_find(unique_ids.begin(), unique_ids.end()) == unique_ids.end());
    RE2DJ_CHECK(context, std::find(ids.begin(), ids.end(), "ez2dj6th") != ids.end());
    RE2DJ_CHECK(context, std::find(ids.begin(), ids.end(), "ez2dj1st") != ids.end());
    RE2DJ_CHECK_EQ(context, catalog.front().id, target::GetBuiltInTargetProfiles().front().profile.id);

    tree.WriteText("roms/ez2dj6th/6th.chd", "x");
    tree.MakeDirectory("roms/ez2dj1st");
    catalog = launcher::BuildLauncherCatalog(tree.root());
    for (const launcher::LauncherEntry& entry : catalog)
    {
        const bool expected = entry.id == "ez2dj6th" || entry.id == "ez2dj1st";
        RE2DJ_CHECK_EQ(context, launcher::IsLauncherEntryRunnable(entry), expected);
    }

    RE2DJ_CHECK_EQ(context, std::string(launcher::ProfileAvailabilityName(launcher::ProfileAvailability::kReady)),
                   std::string("Ready"));
}

void TestLauncherSettingsParse(re2dj::test::Context& context)
{
    const launcher::LauncherSettingsLoad empty = launcher::ParseLauncherSettings("");
    RE2DJ_CHECK(context, empty.warnings.empty());
    RE2DJ_CHECK(context, empty.settings == launcher::LauncherSettings{});

    const launcher::LauncherSettingsLoad full = launcher::ParseLauncherSettings(
        "[launcher]\r\nlast_profile = ez2dj6th\r\ncheck_updates=0\r\n[VIDEO]\r\nfullscreen=1\r\nkeep_aspect=0\r\ncolor_depth=32\r\n"
        "post_shader=\"crt\"\r\n[Audio]\r\ngain_db=-7.5\r\n");
    RE2DJ_CHECK(context, full.warnings.empty());
    RE2DJ_CHECK_EQ(context, full.settings.last_profile, std::string("ez2dj6th"));
    RE2DJ_CHECK(context, full.settings.check_updates == std::optional<bool>(false));
    RE2DJ_CHECK(context, full.settings.fullscreen == std::optional<bool>(true));
    RE2DJ_CHECK(context, full.settings.keep_aspect == std::optional<bool>(false));
    RE2DJ_CHECK(context, full.settings.color_depth == std::optional<ColorDepth>(ColorDepth::k32));
    RE2DJ_CHECK(context, full.settings.post_shader == std::optional<std::string>("crt"));
    RE2DJ_CHECK(context, full.settings.audio_gain_db == std::optional<float>(-7.5F));

    // Bad values are reported and read as never chosen; the rest still load.
    const launcher::LauncherSettingsLoad bad = launcher::ParseLauncherSettings(
        "[Video]\nfullscreen=yes\nkeep_aspect=2\ncolor_depth=24\npost_shader=\n[Audio]\ngain_db=30\n"
        "[Launcher]\nlast_profile=x\n");
    RE2DJ_CHECK_EQ(context, bad.warnings.size(), std::size_t{4});
    RE2DJ_CHECK(context, !bad.settings.keep_aspect.has_value());
    RE2DJ_CHECK(context, !bad.settings.fullscreen.has_value());
    RE2DJ_CHECK(context, !bad.settings.color_depth.has_value());
    RE2DJ_CHECK(context, !bad.settings.post_shader.has_value());
    RE2DJ_CHECK(context, !bad.settings.audio_gain_db.has_value());
    RE2DJ_CHECK_EQ(context, bad.settings.last_profile, std::string("x"));
    RE2DJ_CHECK(context, launcher::ParseLauncherSettings("[Audio]\ngain_db=3dB\n").warnings.size() == 1);
    RE2DJ_CHECK(context, launcher::ParseLauncherSettings("[Audio]\ngain_db=18\n").settings.audio_gain_db ==
                             std::optional<float>(18.0F));
    RE2DJ_CHECK(context, launcher::ParseLauncherSettings("[Audio]\ngain_db=-24\n").settings.audio_gain_db ==
                             std::optional<float>(-24.0F));
}

void TestLauncherSettingsFile(re2dj::test::Context& context)
{
    re2dj::test::TemporaryTree tree;
    const std::filesystem::path config = tree.root() / "cfg";
    const launcher::LauncherSettingsLoad missing = launcher::LoadLauncherSettings(config);
    RE2DJ_CHECK(context, !missing.file_present);
    RE2DJ_CHECK(context, missing.settings == launcher::LauncherSettings{});

    launcher::LauncherSettings settings;
    settings.last_profile = "ez2dj4th";
    settings.check_updates = true;
    settings.fullscreen = false;
    settings.keep_aspect = false;
    settings.color_depth = ColorDepth::k16;
    settings.post_shader = "my shader.glsl";
    settings.audio_gain_db = 2.3F;
    std::string error;
    RE2DJ_CHECK(context, launcher::SaveLauncherSettings(config, settings, &error));
    RE2DJ_CHECK(context, std::filesystem::is_regular_file(launcher::LauncherSettingsPath(config)));
    const launcher::LauncherSettingsLoad loaded = launcher::LoadLauncherSettings(config);
    RE2DJ_CHECK(context, loaded.file_present);
    RE2DJ_CHECK(context, loaded.warnings.empty());
    RE2DJ_CHECK(context, loaded.settings == settings);

    // Unchosen items stay out of the file and come back unchosen.
    launcher::LauncherSettings partial;
    partial.color_depth = ColorDepth::k32;
    const std::string text = launcher::FormatLauncherSettings(partial);
    RE2DJ_CHECK(context, text.find("fullscreen") == std::string::npos);
    RE2DJ_CHECK(context, text.find("gain_db") == std::string::npos);
    RE2DJ_CHECK(context, text.find("color_depth=32") != std::string::npos);
    RE2DJ_CHECK(context, launcher::ParseLauncherSettings(text).settings == partial);
}

void TestLaunchArguments(re2dj::test::Context& context)
{
    launcher::LauncherSettings settings;
    settings.last_profile = "ignored";
    RE2DJ_CHECK(context, launcher::BuildLaunchArguments("ez2dj6th", settings) == std::vector<std::string>{"ez2dj6th"});

    settings.fullscreen = true;
    settings.keep_aspect = false;
    settings.color_depth = ColorDepth::k32;
    settings.post_shader = "scanline";
    settings.audio_gain_db = -6.0F;
    const std::vector<std::string> expected = {"ez2dj6th",      "--fullscreen", "--stretch", "--color-depth", "32",
                                               "--post-shader=scanline",        "--audio-gain-db", "-6"};
    RE2DJ_CHECK(context, launcher::BuildLaunchArguments("ez2dj6th", settings) == expected);

    settings.fullscreen = false;
    settings.keep_aspect = true;
    const std::vector<std::string> windowed = launcher::BuildLaunchArguments("ez2dj4th", settings);
    RE2DJ_CHECK_EQ(context, windowed[1], std::string("--windowed"));
    RE2DJ_CHECK_EQ(context, windowed[2], std::string("--keep-aspect"));
}

// The command line first, then the file, then the defaults (#14).
void TestDisplayPreferences(re2dj::test::Context& context)
{
    launcher::LauncherSettings stored;
    launcher::DisplayPreferences resolved = launcher::ResolveDisplayPreferences(std::nullopt, std::nullopt, stored, false);
    RE2DJ_CHECK(context, !resolved.fullscreen);
    RE2DJ_CHECK(context, resolved.keep_aspect);
    RE2DJ_CHECK(context, launcher::ResolveDisplayPreferences(std::nullopt, std::nullopt, stored, true).fullscreen);

    stored.fullscreen = true;
    stored.keep_aspect = false;
    resolved = launcher::ResolveDisplayPreferences(std::nullopt, std::nullopt, stored, false);
    RE2DJ_CHECK(context, resolved.fullscreen);
    RE2DJ_CHECK(context, !resolved.keep_aspect);
    resolved = launcher::ResolveDisplayPreferences(false, true, stored, true);
    RE2DJ_CHECK(context, !resolved.fullscreen);
    RE2DJ_CHECK(context, resolved.keep_aspect);

    // A change made in game rewrites only the two keys.
    re2dj::test::TemporaryTree tree;
    const std::filesystem::path config = tree.root() / "cfg";
    launcher::LauncherSettings launcher_choice;
    launcher_choice.last_profile = "ez2dj6th";
    launcher_choice.color_depth = ColorDepth::k32;
    launcher_choice.post_shader = "crt";
    launcher_choice.audio_gain_db = -3.0F;
    std::string error;
    RE2DJ_CHECK(context, launcher::SaveLauncherSettings(config, launcher_choice, &error));
    launcher::DisplayPreferences changed;
    changed.fullscreen = true;
    changed.keep_aspect = false;
    RE2DJ_CHECK(context, launcher::SaveDisplayPreferences(config, changed, &error));
    launcher::LauncherSettings expected = launcher_choice;
    expected.fullscreen = true;
    expected.keep_aspect = false;
    RE2DJ_CHECK(context, launcher::LoadLauncherSettings(config).settings == expected);

    // With no file yet, one is made holding just the two keys.
    const std::filesystem::path fresh = tree.root() / "fresh";
    RE2DJ_CHECK(context, launcher::SaveDisplayPreferences(fresh, changed, &error));
    launcher::LauncherSettings only_display;
    only_display.fullscreen = true;
    only_display.keep_aspect = false;
    RE2DJ_CHECK(context, launcher::LoadLauncherSettings(fresh).settings == only_display);
}

}  // namespace

void RunLauncherTests(re2dj::test::Context& context)
{
    TestChdImageLocator(context);
    TestLauncherCatalog(context);
    TestLauncherSettingsParse(context);
    TestLauncherSettingsFile(context);
    TestLaunchArguments(context);
    TestDisplayPreferences(context);
}
