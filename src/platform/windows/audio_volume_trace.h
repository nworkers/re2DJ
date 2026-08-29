#ifndef RE2DJ_PLATFORM_WINDOWS_AUDIO_VOLUME_TRACE_H_
#define RE2DJ_PLATFORM_WINDOWS_AUDIO_VOLUME_TRACE_H_

#define NOMINMAX
#include <windows.h>

extern "C" __declspec(dllexport) char g_re2dj_audio_trace_path[MAX_PATH];
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_audio_image_base;

void Re2djAudioTrace(const char* format, ...);

#endif  // RE2DJ_PLATFORM_WINDOWS_AUDIO_VOLUME_TRACE_H_
