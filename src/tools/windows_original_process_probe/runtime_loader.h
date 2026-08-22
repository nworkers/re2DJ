#ifndef RE2DJ_TOOLS_WINDOWS_ORIGINAL_PROCESS_PROBE_RUNTIME_LOADER_H_
#define RE2DJ_TOOLS_WINDOWS_ORIGINAL_PROCESS_PROBE_RUNTIME_LOADER_H_

#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <string>

namespace re2dj::tools::windows_original_process_probe
{

bool LoadInjectedRuntime(HANDLE process,
                         HANDLE primary_thread,
                         DWORD breakpoint_process_id,
                         DWORD breakpoint_thread_id,
                         const std::filesystem::path& runtime_path,
                         std::uint32_t* runtime_base,
                         std::string* error);

}  // namespace re2dj::tools::windows_original_process_probe

#endif  // RE2DJ_TOOLS_WINDOWS_ORIGINAL_PROCESS_PROBE_RUNTIME_LOADER_H_
