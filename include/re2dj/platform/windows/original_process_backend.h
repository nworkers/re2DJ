#ifndef RE2DJ_PLATFORM_WINDOWS_ORIGINAL_PROCESS_BACKEND_H_
#define RE2DJ_PLATFORM_WINDOWS_ORIGINAL_PROCESS_BACKEND_H_

#include <filesystem>
#include <string>
#include <vector>

namespace re2dj::platform::windows
{

struct OriginalProcessOptions
{
    std::filesystem::path hdd_directory;
    std::filesystem::path io_config;
    std::string target_id;
    float audio_gain_db = 0.0f;
    unsigned demo_volume = 3;
    bool audio_volume_trace = false;
    bool fullscreen = false;
};

bool BuildOriginalProcessArguments(const OriginalProcessOptions& options,
                                   std::vector<std::string>* arguments,
                                   std::string* error);

int RunOriginalProcess(const OriginalProcessOptions& options, std::string* error);

// Compatibility entry used by the diagnostic launcher executable.
int RunOriginalProcessLauncherCommand(int argc, char** argv);

}  // namespace re2dj::platform::windows

#endif  // RE2DJ_PLATFORM_WINDOWS_ORIGINAL_PROCESS_BACKEND_H_
