#ifndef RE2DJ_PLATFORM_WINDOWS_HOST_WINDOW_SHELL_H_
#define RE2DJ_PLATFORM_WINDOWS_HOST_WINDOW_SHELL_H_

#define NOMINMAX
#include <windows.h>

using Re2djHostCloseCallback = void (*)(HWND guest_window);

HWND EnsureRe2djHostWindow(HWND guest_window, Re2djHostCloseCallback close_callback);
HWND ResolveRe2djHostWindow(HWND guest_window);
bool ConfigureRe2djHostWindow(HWND host_window,
                              HWND guest_window,
                              DWORD host_style,
                              DWORD host_extended_style,
                              int x,
                              int y,
                              int width,
                              int height);
BOOL SetRe2djHostWindowTitle(HWND host_window, const char* title);
void SetRe2djHostWindowIcon(HWND host_window);

#endif  // RE2DJ_PLATFORM_WINDOWS_HOST_WINDOW_SHELL_H_
