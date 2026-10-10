#ifndef RE2DJ_LAUNCHER_LAUNCHER_SETTINGS_H_
#define RE2DJ_LAUNCHER_LAUNCHER_SETTINGS_H_

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "re2dj/graphics/color_depth.h"

namespace re2dj::launcher
{

// The options the launcher keeps between runs (#12). An empty optional means
// "never chosen": it is not written to the file and not passed to the run, so
// the option keeps its own default and precedence (a profile's audio gain,
// RE2DJ_POST_SHADER, and so on).
struct LauncherSettings
{
    // Where the list cursor starts next time; not passed to the run.
    std::string last_profile;
    std::optional<bool> fullscreen;
    // Off stretches the picture over the whole window (#14); on by default.
    std::optional<bool> keep_aspect;
    std::optional<graphics::ColorDepth> color_depth;
    std::optional<std::string> post_shader;
    std::optional<float> audio_gain_db;
};

bool operator==(const LauncherSettings& left, const LauncherSettings& right);
inline bool operator!=(const LauncherSettings& left, const LauncherSettings& right)
{
    return !(left == right);
}

// The --audio-gain-db range the command line accepts.
inline constexpr float kLauncherMinimumGainDb = -24.0F;
inline constexpr float kLauncherMaximumGainDb = 18.0F;

struct LauncherSettingsLoad
{
    LauncherSettings settings;
    bool file_present = false;
    // Values that could not be used, each read as never chosen. Never fatal:
    // a typo in the file must not keep the launcher from opening.
    std::vector<std::string> warnings;
};

// `config_directory`/re2dj.ini.
[[nodiscard]] std::filesystem::path LauncherSettingsPath(const std::filesystem::path& config_directory);

// The INI text's settings: [Launcher] last_profile, [Video] fullscreen (0/1),
// keep_aspect (0/1), color_depth (16/32) and post_shader, [Audio] gain_db
// (-24..+18). Keys are found by the same rules the guest's private profiles
// follow.
[[nodiscard]] LauncherSettingsLoad ParseLauncherSettings(std::string_view text);
[[nodiscard]] LauncherSettingsLoad LoadLauncherSettings(const std::filesystem::path& config_directory);

// The INI text for `settings`, keys in a fixed order, unchosen ones left out.
[[nodiscard]] std::string FormatLauncherSettings(const LauncherSettings& settings);
// Writes the file, creating the directory when needed. False, with `error`
// set, when it cannot be written.
bool SaveLauncherSettings(const std::filesystem::path& config_directory,
                          const LauncherSettings& settings,
                          std::string* error);

// The two display values a run starts with (#14). Every run reads them, not
// only the launcher's: the command line's choice first, then the stored
// value, then the default (the profile's fullscreen, keep-aspect on).
struct DisplayPreferences
{
    bool fullscreen = false;
    bool keep_aspect = true;
};

[[nodiscard]] DisplayPreferences ResolveDisplayPreferences(std::optional<bool> command_line_fullscreen,
                                                           std::optional<bool> command_line_keep_aspect,
                                                           const LauncherSettings& stored,
                                                           bool profile_fullscreen);

// Saves a change the user made in game (the OSD, Alt+Enter, a double click):
// the file is read again and written with only these two keys changed, so the
// launcher's other choices stay as they are.
bool SaveDisplayPreferences(const std::filesystem::path& config_directory,
                            const DisplayPreferences& preferences,
                            std::string* error);

// The arguments, after the program name, of the run that starts `profile_id`:
// the profile id, then one option per chosen setting.
[[nodiscard]] std::vector<std::string> BuildLaunchArguments(const std::string& profile_id,
                                                            const LauncherSettings& settings);

}  // namespace re2dj::launcher

#endif  // RE2DJ_LAUNCHER_LAUNCHER_SETTINGS_H_
