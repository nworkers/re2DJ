#ifndef RE2DJ_PLATFORM_WINDOWS_INJECTED_RUNTIME_LOADER_H_
#define RE2DJ_PLATFORM_WINDOWS_INJECTED_RUNTIME_LOADER_H_

#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <string>

namespace re2dj::platform::windows
{

bool LoadInjectedRuntime(HANDLE process,
                         HANDLE primary_thread,
                         DWORD breakpoint_process_id,
                         DWORD breakpoint_thread_id,
                         const std::filesystem::path& runtime_path,
                         std::uint32_t* runtime_base,
                         std::string* error);

}  // namespace re2dj::platform::windows

#endif  // RE2DJ_PLATFORM_WINDOWS_INJECTED_RUNTIME_LOADER_H_
