#ifndef RE2DJ_PLATFORM_WINDOWS_AUDIO_VOLUME_TRACE_H_
#define RE2DJ_PLATFORM_WINDOWS_AUDIO_VOLUME_TRACE_H_

#define NOMINMAX
#include <windows.h>

extern "C" __declspec(dllexport) char g_re2dj_audio_trace_path[MAX_PATH];

void Re2djAudioTrace(const char* format, ...);

#endif  // RE2DJ_PLATFORM_WINDOWS_AUDIO_VOLUME_TRACE_H_
