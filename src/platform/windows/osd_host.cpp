#include "osd_host.h"

#include "game_controls.h"
#include "graphics_trace_log.h"

namespace re2dj::platform::windows
{

re2dj::ui::Osd& ProcessOsd()
{
    static re2dj::ui::Osd* const osd = new re2dj::ui::Osd;
    return *osd;
}

void InstallProcessOsd(re2dj::graphics::Sdl3OpenGlBackend* backend)
{
    if (backend == nullptr)
    {
        return;
    }
    static bool controls_registered = false;
    if (!controls_registered)
    {
        RegisterGameControls(&ProcessOsd());
        controls_registered = true;
    }
    backend->SetPresentOverlay(&ProcessOsd());
}

bool HandleOsdWindowMessage(UINT message, WPARAM wparam, LPARAM lparam)
{
    re2dj::ui::Osd& osd = ProcessOsd();
    switch (message)
    {
    case WM_KEYDOWN:
        // Bit 30 marks auto-repeat; holding the key must not flicker the OSD.
        if (wparam == VK_OEM_3 && (static_cast<ULONG_PTR>(lparam) & (1u << 30)) == 0)
        {
            osd.ToggleVisible();
            return true;
        }
        return false;
    case WM_KEYUP:
    case WM_CHAR:
        // The rest of the backtick keystroke belongs to the toggle as well.
        return wparam == VK_OEM_3 || wparam == '`';
    case WM_MOUSEMOVE:
        osd.QueueMousePosition(static_cast<float>(static_cast<short>(LOWORD(lparam))),
                               static_cast<float>(static_cast<short>(HIWORD(lparam))));
        return false;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
        osd.QueueMouseButton(0, message == WM_LBUTTONDOWN);
        return osd.visible();
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
        osd.QueueMouseButton(1, message == WM_RBUTTONDOWN);
        return osd.visible();
    default:
        return false;
    }
}

void ReportOsdState()
{
    static bool reported_visible = false;
    static int reported_renderer = -1;
    static bool drawing_reported = false;
    re2dj::ui::Osd& osd = ProcessOsd();
    const bool visible = osd.visible();
    const int renderer = static_cast<int>(osd.renderer_state());
    if (visible != reported_visible)
    {
        reported_visible = visible;
        drawing_reported = false;
        WriteGraphicsTraceFormat("re2dj:hle:osd:visible=%d", visible ? 1 : 0);
    }
    if (renderer != reported_renderer)
    {
        reported_renderer = renderer;
        WriteGraphicsTraceFormat("re2dj:hle:osd:renderer=%s",
                                 renderer == 1   ? "ready"
                                 : renderer == 2 ? "failed"
                                                 : "not-started");
    }
    if (visible && !drawing_reported && osd.frames_drawn() > 0)
    {
        drawing_reported = true;
        WriteGraphicsTraceFormat("re2dj:hle:osd:drawing:frames=%u", osd.frames_drawn());
    }
}

}  // namespace re2dj::platform::windows
