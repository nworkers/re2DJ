#include "audio_volume_trace.h"

#include "runtime_log.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>

extern "C" __declspec(dllexport) char g_re2dj_audio_trace_path[MAX_PATH] = "logs/audio_debug.log";
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_audio_image_base = 0;

namespace
{
volatile LONG g_audio_trace_lines = 0;
constexpr LONG kMaximumAudioTraceLines = 4096;
}

void Re2djAudioTrace(const char* format, ...)
{
    if (format == nullptr)
    {
        return;
    }
    char message[1024] = {};
    va_list arguments;
    va_start(arguments, format);
    const int length = std::vsnprintf(message, sizeof(message) - 3, format, arguments);
    va_end(arguments);
    if (length < 0)
    {
        return;
    }
    const std::size_t used = (std::min)(static_cast<std::size_t>(length), sizeof(message) - 3);
    message[used] = '\r';
    message[used + 1] = '\n';
    message[used + 2] = '\0';
    OutputDebugStringA(message);

    if (g_re2dj_audio_trace_path[0] == '\0' ||
        InterlockedIncrement(&g_audio_trace_lines) > kMaximumAudioTraceLines)
    {
        return;
    }
    re2dj::platform::windows::WriteRuntimeLog(re2dj::platform::windows::RuntimeLogChannel::kAudio,
                                              message);
}
