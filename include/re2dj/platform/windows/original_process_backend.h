#ifndef RE2DJ_PLATFORM_WINDOWS_ORIGINAL_PROCESS_BACKEND_H_
#define RE2DJ_PLATFORM_WINDOWS_ORIGINAL_PROCESS_BACKEND_H_

#include <filesystem>
#include <string>
#include <vector>

#include "re2dj/target/target_profile.h"

namespace re2dj::platform::windows
{

struct OriginalProcessOptions
{
    std::filesystem::path hdd_directory;
    // Optional source CHD. The HDD directory then contains only the staged
    // executable and cache; injected VFS reads the original files from this
    // image.
    std::filesystem::path chd_image;
    std::filesystem::path io_config;
    std::string target_id;
    // Relative executable path within the staged CHD root. Empty for normal
    // directory-backed launches, which rediscover the target by scanning.
    std::string executable_relative_path;
    std::string hle_profile_id;
    re2dj::target::TargetRunDefaults profile_defaults;
    bool audio_volume_trace = false;
};

bool BuildOriginalProcessArguments(const OriginalProcessOptions& options,
                                   std::vector<std::string>* arguments,
                                   std::string* error);

int RunOriginalProcess(const OriginalProcessOptions& options, std::string* error);

// Compatibility entry used by the diagnostic launcher executable.
int RunOriginalProcessLauncherCommand(int argc, char** argv);

}  // namespace re2dj::platform::windows

#endif  // RE2DJ_PLATFORM_WINDOWS_ORIGINAL_PROCESS_BACKEND_H_
