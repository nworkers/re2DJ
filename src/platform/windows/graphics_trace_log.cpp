#define NOMINMAX
#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <vector>

#include "graphics_trace_log.h"
#include "runtime_log.h"

// The launcher resolves this export in the injected runtime and writes the
// diagnostic log's sibling ".ddraw.log" path into it before the guest reaches
// its graphics initialization. It stays an exported symbol with this exact
// name because the launcher looks it up by name.
extern "C" __declspec(dllexport) char g_re2dj_graphics_trace_path[MAX_PATH] = {};

// The launcher resolves this export by name and writes a nonzero value only
// when its draw-diagnostics option was passed. It stays zero on the product
// path, and it keeps this exact name for the same reason as the path above.
extern "C" __declspec(dllexport) unsigned long g_re2dj_graphics_draw_diagnostics = 0;

// The launcher resolves this export by name and writes the present
// synchronization policy into it: 0 vertical sync, 1 immediate, 2 adaptive.
// Zero is both the default and the behavior the product had before the policy
// became explicit, so a launcher that never writes it changes nothing. It
// keeps this exact name because the launcher looks it up by name.
extern "C" __declspec(dllexport) unsigned long g_re2dj_present_sync = 0;

namespace re2dj::platform::windows
{
void WriteGraphicsTraceLine(const char* message)
{
    if (message == nullptr)
    {
        return;
    }
    OutputDebugStringA(message);
    WriteRuntimeLog(RuntimeLogChannel::kGraphics, message);
}

void WriteGraphicsTraceFormat(const char* format, ...)
{
    if (format == nullptr)
    {
        return;
    }
    va_list arguments;
    va_start(arguments, format);
    va_list sizing_arguments;
    va_copy(sizing_arguments, arguments);
    const int length = std::vsnprintf(nullptr, 0, format, sizing_arguments);
    va_end(sizing_arguments);
    va_end(arguments);
    if (length <= 0)
    {
        return;
    }
    constexpr std::size_t kStackMessageCapacity = 2048;
    if (static_cast<std::size_t>(length) < kStackMessageCapacity)
    {
        char message[kStackMessageCapacity] = {};
        va_start(arguments, format);
        std::vsnprintf(message, sizeof(message), format, arguments);
        va_end(arguments);
        WriteGraphicsTraceLine(message);
        return;
    }
    va_start(arguments, format);
    std::vector<char> message(static_cast<std::size_t>(length) + 1, '\0');
    std::vsnprintf(message.data(), message.size(), format, arguments);
    va_end(arguments);
    WriteGraphicsTraceLine(message.data());
}

bool AreGraphicsDrawDiagnosticsEnabled()
{
    return g_re2dj_graphics_draw_diagnostics != 0;
}

bool AreCompleteDiagnosticsEnabled()
{
    // The draw-diagnostics switch is an explicit investigation request. The
    // correlated VFS trace uses the same request so later file reads cannot
    // disappear while the draw trace remains enabled.
    return AreGraphicsDrawDiagnosticsEnabled();
}

re2dj::graphics::PresentSync SelectedPresentSync()
{
    switch (g_re2dj_present_sync)
    {
    case 1:
        return re2dj::graphics::PresentSync::kImmediate;
    case 2:
        return re2dj::graphics::PresentSync::kAdaptive;
    default:
        // An unrecognized word is treated as the default rather than rejected:
        // the word is written by another process into this one, so the safe
        // reading of a value this build does not know is the old behavior.
        return re2dj::graphics::PresentSync::kVerticalSync;
    }
}

void ReportUnimplementedGraphicsCall(const char* interface_name,
                                     GraphicsCallLedger* ledger)
{
    if (interface_name == nullptr || ledger == nullptr || ledger->method == nullptr)
    {
        return;
    }
    // The ledger lives in a function-local static shared by every thread that
    // reaches the slot, so the budget is decremented atomically. Going negative
    // is harmless; the comparison is what stops the writes.
    if (InterlockedDecrement(reinterpret_cast<volatile LONG*>(&ledger->remaining)) < 0)
    {
        return;
    }
    WriteGraphicsTraceFormat(
        "re2dj:FATAL:HLE_UNIMPLEMENTED:interface=%s:method=%s",
        interface_name,
        ledger->method);
}

}  // namespace re2dj::platform::windows
