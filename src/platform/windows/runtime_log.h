#ifndef RE2DJ_PLATFORM_WINDOWS_RUNTIME_LOG_H_
#define RE2DJ_PLATFORM_WINDOWS_RUNTIME_LOG_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// The launcher resolves this export by name and writes the runtime record's
// path into it; empty keeps the runtime channel on OutputDebugStringA only.
extern "C" __declspec(dllexport) char g_re2dj_runtime_log_path[MAX_PATH];

// Closes every channel's file, so a host can remove the files while the DLL
// stays loaded. The next record reopens its channel.
extern "C" __declspec(dllexport) void Re2djCloseRuntimeLogs();

namespace re2dj::platform::windows
{

// Each channel records to the file whose path the launcher wrote into the
// matching export.
enum class RuntimeLogChannel
{
    // General diagnostics: g_re2dj_runtime_log_path. The text also goes to
    // OutputDebugStringA unchanged, which the launcher reads.
    kRuntime,
    // g_re2dj_vfs_trace_path.
    kVfs,
    // g_re2dj_graphics_trace_path.
    kGraphics,
    // g_re2dj_audio_trace_path.
    kAudio,
};

// Records one line through spdlog. Trailing line breaks are dropped because
// the sink ends the line. The caller's last error is preserved, so a record
// never changes what the guest reads from GetLastError.
void WriteRuntimeLog(RuntimeLogChannel channel, const char* message);

}  // namespace re2dj::platform::windows

#endif  // RE2DJ_PLATFORM_WINDOWS_RUNTIME_LOG_H_
