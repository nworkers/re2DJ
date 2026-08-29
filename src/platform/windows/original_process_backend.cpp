#include "re2dj/platform/windows/original_process_backend.h"

#include <cmath>

namespace re2dj::platform::windows
{
namespace
{

constexpr const char* kSupportedTarget = "ez2dj1stse";
constexpr const char* kLptdiTargetState = "0900000000000000";

}  // namespace

bool BuildOriginalProcessArguments(const OriginalProcessOptions& options,
                                   std::vector<std::string>* arguments,
                                   std::string* error)
{
    if (arguments == nullptr || error == nullptr || options.hdd_directory.empty() ||
        options.target_id.empty())
    {
        if (error != nullptr)
        {
            *error = "invalid Windows original-process options";
        }
        return false;
    }
    if (options.target_id != kSupportedTarget)
    {
        *error = "Windows product execution currently supports only target 'ez2dj1stse'";
        return false;
    }
    if (!std::isfinite(options.audio_gain_db) || options.audio_gain_db < -24.0f ||
        options.audio_gain_db > 18.0f)
    {
        *error = "Windows audio master gain must be between -24 and +18 dB";
        return false;
    }
    if (options.demo_volume > 3)
    {
        *error = "Windows demo volume must be between 0 and 3";
        return false;
    }

    *arguments = {
        "re2dj",
        "--hdd",
        options.hdd_directory.string(),
        "--target",
        options.target_id,
        "--hle-command-line",
        "--hle-windows-directory",
        "--hle-vfs",
        "--hle-d3d3",
        "--hle-directsound",
        "--audio-gain-db",
        std::to_string(options.audio_gain_db),
        "--demo-volume",
        std::to_string(options.demo_volume),
        "--hle-io-ports",
        "--run-detached",
        "--device-mock-lptdi-target-state",
        kLptdiTargetState,
    };
    if (options.audio_volume_trace)
    {
        arguments->push_back("--audio-volume-trace");
    }
    if (options.fullscreen)
    {
        arguments->push_back("--fullscreen");
    }
    if (!options.io_config.empty())
    {
        arguments->push_back("--io-config");
        arguments->push_back(options.io_config.string());
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
