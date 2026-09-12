#ifndef RE2DJ_PLATFORM_WINDOWS_WINDOW_MODE_H_
#define RE2DJ_PLATFORM_WINDOWS_WINDOW_MODE_H_

#define NOMINMAX
#include <windows.h>

extern "C" __declspec(dllexport) volatile DWORD g_re2dj_fullscreen;
extern "C" __declspec(dllexport) void WINAPI Re2djExitIfWindowClosed(HWND window);
extern "C" __declspec(dllexport) BOOL WINAPI Re2djUpdateWindowTitle(HWND window, double fps);

bool ApplyRe2djWindowMode(HWND window, DWORD client_width, DWORD client_height);
bool SetRe2djWindowScale(HWND window, DWORD scale);
bool ToggleRe2djFullscreen(HWND window);

#endif  // RE2DJ_PLATFORM_WINDOWS_WINDOW_MODE_H_
