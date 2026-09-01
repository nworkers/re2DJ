#include "re2dj/platform/windows/original_process_backend.h"

#include <cmath>

#include "re2dj/config/hardlock_secret_config.h"

namespace re2dj::platform::windows
{
namespace
{

bool HasExecutionPolicy(const re2dj::target::TargetRunDefaults& defaults)
{
    return defaults.hle_command_line || defaults.hle_windows_directory || defaults.hle_vfs ||
           defaults.hle_d3d3 || defaults.hle_directsound ||
           defaults.lptdi.legacy_io_ports || defaults.lptdi.device_mock_enabled ||
           defaults.run_detached;
}

}  // namespace

bool BuildOriginalProcessArguments(const OriginalProcessOptions& options,
                                   std::vector<std::string>* arguments,
                                   std::string* error)
{
    if (arguments == nullptr || error == nullptr || options.hdd_directory.empty() ||
        options.target_id.empty() || options.hle_profile_id.empty() ||
        !HasExecutionPolicy(options.profile_defaults))
    {
        if (error != nullptr)
        {
            *error = "invalid Windows original-process options";
        }
        return false;
    }
    if (!options.executable_relative_path.empty() && options.chd_image.empty())
    {
        *error = "explicit executable path requires a CHD image";
        return false;
    }
    if (options.profile_defaults.lptdi.legacy_io_ports &&
        !options.profile_defaults.lptdi.device_mock_enabled)
    {
        *error = "profile enables legacy I/O without an LPTDI device policy";
        return false;
    }
    if (options.profile_defaults.lptdi.device_mock_enabled &&
        options.profile_defaults.lptdi.device_mock_path_prefix.empty())
    {
        *error = "profile enables LPTDI device mock without a device path prefix";
        return false;
    }
    if (options.profile_defaults.lptdi.device_mock_enabled &&
        !options.profile_defaults.lptdi.hardlock_secret_config_required &&
        options.profile_defaults.lptdi.device_mock_target_state_hex.empty())
    {
        *error = "profile enables LPTDI device mock without a target-state policy";
        return false;
    }
    if (!options.profile_defaults.lptdi.device_mock_enabled &&
        !options.profile_defaults.lptdi.device_mock_path_prefix.empty())
    {
        *error = "profile has an LPTDI device path without device mock enablement";
        return false;
    }
    if (!options.profile_defaults.lptdi.device_mock_enabled &&
        !options.profile_defaults.lptdi.device_mock_target_state_hex.empty())
    {
        *error = "profile has an LPTDI target-state policy without device mock enablement";
        return false;
    }
    if (options.profile_defaults.lptdi.hardlock_secret_config_required &&
        !options.profile_defaults.lptdi.device_mock_enabled)
    {
        *error = "profile requires an external Hardlock configuration";
        return false;
    }
    if (!options.profile_defaults.lptdi.hardlock_secret_config_required &&
        !options.hardlock_config.empty())
    {
        *error = "profile does not support a Hardlock configuration";
        return false;
    }
    if (options.profile_defaults.audio_gain_db.has_value() &&
        (!std::isfinite(*options.profile_defaults.audio_gain_db) ||
         *options.profile_defaults.audio_gain_db < -24.0f ||
         *options.profile_defaults.audio_gain_db > 18.0f))
    {
        *error = "Windows audio master gain must be between -24 and +18 dB";
        return false;
    }
    if (options.profile_defaults.demo_volume.has_value() &&
        *options.profile_defaults.demo_volume > 3)
    {
        *error = "Windows demo volume must be between 0 and 3";
        return false;
    }

    *arguments = {"re2dj", "--hdd", options.hdd_directory.string(), "--target", options.target_id};
    if (!options.chd_image.empty())
    {
        arguments->push_back("--chd");
        arguments->push_back(options.chd_image.string());
        if (!options.executable_relative_path.empty())
        {
            arguments->push_back("--target-executable");
            arguments->push_back(options.executable_relative_path);
        }
    }
    const re2dj::target::TargetRunDefaults& defaults = options.profile_defaults;
    if (defaults.hle_command_line)
    {
        arguments->push_back("--hle-command-line");
    }
    if (defaults.hle_windows_directory)
    {
        arguments->push_back("--hle-windows-directory");
    }
    if (defaults.hle_vfs)
    {
        arguments->push_back("--hle-vfs");
    }
    if (defaults.hle_d3d3)
    {
        arguments->push_back("--hle-d3d3");
    }
    if (defaults.hle_directsound)
    {
        arguments->push_back("--hle-directsound");
    }
    if (defaults.audio_gain_db.has_value())
    {
        arguments->push_back("--audio-gain-db");
        arguments->push_back(std::to_string(*defaults.audio_gain_db));
    }
    if (defaults.demo_volume.has_value())
    {
        arguments->push_back("--demo-volume");
        arguments->push_back(std::to_string(*defaults.demo_volume));
    }
    if (defaults.lptdi.legacy_io_ports)
    {
        arguments->push_back("--hle-io-ports");
    }
    if (defaults.run_detached)
    {
        arguments->push_back("--run-detached");
    }
    if (defaults.lptdi.device_mock_enabled)
    {
        arguments->push_back("--device-mock-lptdi");
        arguments->push_back("--device-mock-lptdi-path-prefix");
        arguments->push_back(defaults.lptdi.device_mock_path_prefix);
    }
    if (!defaults.lptdi.device_mock_target_state_hex.empty())
    {
        arguments->push_back("--device-mock-lptdi-target-state");
        arguments->push_back(defaults.lptdi.device_mock_target_state_hex);
    }
    if (options.audio_volume_trace)
    {
        arguments->push_back("--audio-volume-trace");
    }
    if (defaults.fullscreen)
    {
        arguments->push_back("--fullscreen");
    }
    if (!options.io_config.empty())
    {
        arguments->push_back("--io-config");
        arguments->push_back(options.io_config.string());
    }
    const std::filesystem::path hardlock_config =
        defaults.lptdi.hardlock_secret_config_required && options.hardlock_config.empty()
            ? re2dj::config::DefaultHardlockSecretConfigPath()
            : options.hardlock_config;
    if (!hardlock_config.empty())
    {
        arguments->push_back("--hardlock-config");
        arguments->push_back(hardlock_config.string());
    }
    error->clear();
    return true;
}

int RunOriginalProcess(const OriginalProcessOptions& options, std::string* error)
{
    std::vector<std::string> arguments;
    if (!BuildOriginalProcessArguments(options, &arguments, error))
    {
        return -1;
    }
    std::vector<char*> pointers;
    pointers.reserve(arguments.size());
    for (std::string& argument : arguments)
    {
        pointers.push_back(argument.data());
    }
    return RunOriginalProcessLauncherCommand(
        static_cast<int>(pointers.size()), pointers.data());
}

}  // namespace re2dj::platform::windows
