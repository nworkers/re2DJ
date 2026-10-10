#include "re2dj/launcher/launcher_settings.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>
#include <system_error>

#include "re2dj/hle/private_profile.h"

namespace re2dj::launcher
{
namespace
{

constexpr const char* kFileName = "re2dj.ini";
constexpr const char* kLauncherSection = "Launcher";
constexpr const char* kVideoSection = "Video";
constexpr const char* kAudioSection = "Audio";

std::optional<std::string> FindValue(std::string_view text, const char* section, const char* key)
{
    const std::optional<std::string> raw = hle::FindPrivateProfileValue(text, section, key);
    if (!raw.has_value())
    {
        return std::nullopt;
    }
    return hle::PrivateProfileStringValue(*raw);
}

std::string Invalid(const char* section, const char* key, const std::string& value, const char* expected)
{
    return std::string("re2dj.ini [") + section + "] " + key + "=" + value + " is not " + expected + "; ignored";
}

// The shortest text that reads back as the same float, so a stored gain
// round-trips through the file unchanged.
std::string FormatGain(float value)
{
    char text[32];
    for (int precision = 1; precision <= 9; ++precision)
    {
        std::snprintf(text, sizeof(text), "%.*g", precision, static_cast<double>(value));
        if (std::strtof(text, nullptr) == value)
        {
            break;
        }
    }
    return text;
}

}  // namespace

bool operator==(const LauncherSettings& left, const LauncherSettings& right)
{
    return left.last_profile == right.last_profile && left.fullscreen == right.fullscreen &&
           left.color_depth == right.color_depth && left.post_shader == right.post_shader &&
           left.audio_gain_db == right.audio_gain_db;
}

std::filesystem::path LauncherSettingsPath(const std::filesystem::path& config_directory)
{
    return config_directory / kFileName;
}

LauncherSettingsLoad ParseLauncherSettings(std::string_view text)
{
    LauncherSettingsLoad load;
    LauncherSettings& settings = load.settings;

    if (const auto value = FindValue(text, kLauncherSection, "last_profile"))
    {
        settings.last_profile = *value;
    }
    if (const auto value = FindValue(text, kVideoSection, "fullscreen"))
    {
        if (*value == "0" || *value == "1")
        {
            settings.fullscreen = *value == "1";
        }
        else
        {
            load.warnings.push_back(Invalid(kVideoSection, "fullscreen", *value, "0 or 1"));
        }
    }
    if (const auto value = FindValue(text, kVideoSection, "color_depth"))
    {
        graphics::ColorDepth depth = graphics::ColorDepth::k16;
        if (graphics::ParseColorDepthName(*value, &depth))
        {
            settings.color_depth = depth;
        }
        else
        {
            load.warnings.push_back(Invalid(kVideoSection, "color_depth", *value, "16 or 32"));
        }
    }
    if (const auto value = FindValue(text, kVideoSection, "post_shader"))
    {
        // Whether the id names a shader is decided when the run's window
        // lists them, as for the command line.
        if (!value->empty())
        {
            settings.post_shader = *value;
        }
    }
    if (const auto value = FindValue(text, kAudioSection, "gain_db"))
    {
        char* end = nullptr;
        const float gain = std::strtof(value->c_str(), &end);
        if (!value->empty() && end == value->c_str() + value->size() && std::isfinite(gain) &&
            gain >= kLauncherMinimumGainDb && gain <= kLauncherMaximumGainDb)
        {
            settings.audio_gain_db = gain;
        }
        else
        {
            load.warnings.push_back(Invalid(kAudioSection, "gain_db", *value, "a number from -24 to 18"));
        }
    }
    return load;
}

LauncherSettingsLoad LoadLauncherSettings(const std::filesystem::path& config_directory)
{
    const std::filesystem::path path = LauncherSettingsPath(config_directory);
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        return {};
    }
    const std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    LauncherSettingsLoad load = ParseLauncherSettings(text);
    load.file_present = true;
    return load;
}

std::string FormatLauncherSettings(const LauncherSettings& settings)
{
    std::ostringstream text;
    text << "; re2DJ launcher settings. Keys left out keep the option's default.\n";
    text << "[Launcher]\n";
    if (!settings.last_profile.empty())
    {
        text << "last_profile=" << settings.last_profile << "\n";
    }
    text << "\n[Video]\n";
    if (settings.fullscreen.has_value())
    {
        text << "fullscreen=" << (*settings.fullscreen ? "1" : "0") << "\n";
    }
    if (settings.color_depth.has_value())
    {
        text << "color_depth=" << graphics::ColorDepthName(*settings.color_depth) << "\n";
    }
    if (settings.post_shader.has_value())
    {
        text << "post_shader=" << *settings.post_shader << "\n";
    }
    text << "\n[Audio]\n";
    if (settings.audio_gain_db.has_value())
    {
        text << "gain_db=" << FormatGain(*settings.audio_gain_db) << "\n";
    }
    return text.str();
}

bool SaveLauncherSettings(const std::filesystem::path& config_directory,
                          const LauncherSettings& settings,
                          std::string* error)
{
    std::error_code code;
    std::filesystem::create_directories(config_directory, code);
    const std::filesystem::path path = LauncherSettingsPath(config_directory);
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    const std::string text = FormatLauncherSettings(settings);
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    stream.close();
    if (!stream)
    {
        if (error != nullptr)
        {
            *error = "cannot write " + path.string();
        }
        return false;
    }
    return true;
}

std::vector<std::string> BuildLaunchArguments(const std::string& profile_id, const LauncherSettings& settings)
{
    std::vector<std::string> arguments;
    arguments.push_back(profile_id);
    if (settings.fullscreen.has_value())
    {
        arguments.emplace_back(*settings.fullscreen ? "--fullscreen" : "--windowed");
    }
    if (settings.color_depth.has_value())
    {
        arguments.emplace_back("--color-depth");
        arguments.emplace_back(graphics::ColorDepthName(*settings.color_depth));
    }
    if (settings.post_shader.has_value())
    {
        arguments.push_back("--post-shader=" + *settings.post_shader);
    }
    if (settings.audio_gain_db.has_value())
    {
        arguments.emplace_back("--audio-gain-db");
        arguments.push_back(FormatGain(*settings.audio_gain_db));
    }
    return arguments;
}

}  // namespace re2dj::launcher
