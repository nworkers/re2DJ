#define NOMINMAX
#include <windows.h>

#include <cstdio>

#include "display_mode_boundary.h"
#include "graphics_trace_log.h"

namespace
{

// One absorbed request per line, bounded. A guest that retries its mode change
// every frame would otherwise fill the trace, and the question this answers -
// what the guest wanted the display to be - is answered by the first few.
volatile LONG g_absorbed_requests = 0;
constexpr LONG kTracedRequests = 16;

void TraceAbsorbedRequest(const char* entry_point,
                          LPCSTR device_name,
                          const DEVMODEA* dev_mode,
                          DWORD flags)
{
    if (InterlockedIncrement(&g_absorbed_requests) > kTracedRequests)
    {
        return;
    }
    // A guest may pass a null DEVMODE to ask for the registry default, so every
    // field is read only after that check.
    re2dj::platform::windows::WriteGraphicsTraceFormat(
        "re2dj:hle:display-mode:absorbed:entry=%s:device=%s:flags=0x%08lx:"
        "fields=0x%08lx:%lux%lux%lu:refresh=%lu",
        entry_point,
        device_name == nullptr ? "default" : device_name,
        flags,
        dev_mode == nullptr ? 0UL : dev_mode->dmFields,
        dev_mode == nullptr ? 0UL : dev_mode->dmPelsWidth,
        dev_mode == nullptr ? 0UL : dev_mode->dmPelsHeight,
        dev_mode == nullptr ? 0UL : dev_mode->dmBitsPerPel,
        dev_mode == nullptr ? 0UL : dev_mode->dmDisplayFrequency);
}

}  // namespace

// The return value decides which branch the guest takes. 1st SE splits it into
// success, restart-required, and failure, and its failure branch reaches
// PostQuitMessage(0) - the executable simply stops. Reporting success is
// therefore what lets the game run, and it is honest about the only thing the
// guest can observe afterwards: the window it draws into is the size it asked
// for, scaled by the window policy.
//
// CDS_TEST gets the same answer. The mode will never be applied, so "can this
// mode be set" has one answer.
extern "C" __declspec(dllexport) LONG WINAPI Re2djHleChangeDisplaySettingsExA(
    LPCSTR device_name,
    DEVMODEA* dev_mode,
    HWND window,
    DWORD flags,
    LPVOID reserved)
{
    (void)window;
    (void)reserved;
    TraceAbsorbedRequest("ChangeDisplaySettingsExA", device_name, dev_mode, flags);
    SetLastError(ERROR_SUCCESS);
    return DISP_CHANGE_SUCCESSFUL;
}

extern "C" __declspec(dllexport) LONG WINAPI Re2djHleChangeDisplaySettingsA(
    DEVMODEA* dev_mode,
    DWORD flags)
{
    TraceAbsorbedRequest("ChangeDisplaySettingsA", nullptr, dev_mode, flags);
    SetLastError(ERROR_SUCCESS);
    return DISP_CHANGE_SUCCESSFUL;
}
