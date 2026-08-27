#include "audio_volume_trace.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>

extern "C" __declspec(dllexport) char g_re2dj_audio_trace_path[MAX_PATH] = {};

namespace
{
volatile LONG g_audio_trace_lines = 0;
constexpr LONG kMaximumAudioTraceLines = 4096;
}

void Re2djAudioTrace(const char* format, ...)
{
    if (g_re2dj_audio_trace_path[0] == '\0' || format == nullptr ||
        InterlockedIncrement(&g_audio_trace_lines) > kMaximumAudioTraceLines)
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
    HANDLE trace = CreateFileA(g_re2dj_audio_trace_path,
                               FILE_APPEND_DATA,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               nullptr,
                               OPEN_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL,
                               nullptr);
    if (trace == INVALID_HANDLE_VALUE)
    {
        return;
    }
    DWORD written = 0;
    WriteFile(trace, message, static_cast<DWORD>(used + 2), &written, nullptr);
    CloseHandle(trace);
}
