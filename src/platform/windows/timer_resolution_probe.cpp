#define NOMINMAX
#include <windows.h>

#include <cstddef>

#include "graphics_trace_log.h"
#include "timer_resolution_probe.h"

// Defined in the injected runtime and written by the launcher when its wait
// trace option is passed. The resolution only reads against the sleep
// durations it explains, so it shares that switch rather than adding a second
// one for the same investigation.
extern "C" unsigned long g_re2dj_guest_wait_trace;

namespace re2dj::platform::windows
{
namespace
{

// NTSTATUS-returning ntdll entry. Declared here rather than included from the
// DDK because the public SDK does not expose it.
using NtQueryTimerResolutionProc = LONG(NTAPI*)(PULONG minimum_resolution,
                                                PULONG maximum_resolution,
                                                PULONG current_resolution);

// Resolved once. A failed resolution is cached as "unavailable" so a missing
// export is not looked up again on every summary window.
NtQueryTimerResolutionProc ResolveOnce()
{
    static NtQueryTimerResolutionProc resolved = []() -> NtQueryTimerResolutionProc {
        // GetModuleHandleW rather than LoadLibrary: ntdll is mapped into every
        // process before any of our code runs, so there is nothing to load and
        // no reference to release.
        const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        if (ntdll == nullptr)
        {
            return nullptr;
        }
        return reinterpret_cast<NtQueryTimerResolutionProc>(
            reinterpret_cast<void*>(GetProcAddress(ntdll, "NtQueryTimerResolution")));
    }();
    return resolved;
}

// The API reports 100 ns units.
constexpr double kMillisecondsPerUnit = 1.0 / 10000.0;

}  // namespace

bool QueryTimerResolution(TimerResolution* resolution)
{
    if (resolution == nullptr)
    {
        return false;
    }
    const NtQueryTimerResolutionProc query = ResolveOnce();
    if (query == nullptr)
    {
        return false;
    }
    ULONG minimum = 0;
    ULONG maximum = 0;
    ULONG current = 0;
    if (query(&minimum, &maximum, &current) < 0)
    {
        return false;
    }
    resolution->minimum_milliseconds = static_cast<double>(minimum) * kMillisecondsPerUnit;
    resolution->maximum_milliseconds = static_cast<double>(maximum) * kMillisecondsPerUnit;
    resolution->current_milliseconds = static_cast<double>(current) * kMillisecondsPerUnit;
    return true;
}

namespace
{

void WriteTimerResolutionLine(const char* source, const TimerResolution& resolution)
{
    WriteGraphicsTraceFormat(
        "re2dj:hle:timer-resolution:current_ms=%.4f:minimum_ms=%.4f:maximum_ms=%.4f:source=%s",
        resolution.current_milliseconds,
        resolution.minimum_milliseconds,
        resolution.maximum_milliseconds,
        source);
}

// Samples taken before the launcher armed the switch. Bounded because the only
// samples that reach it are the few taken during startup, and a diagnostic
// that can grow without limit is not one that may sit in a guest's process.
struct PendingSample
{
    const char* source = nullptr;
    TimerResolution resolution;
};

constexpr std::size_t kPendingCapacity = 8;
PendingSample g_pending[kPendingCapacity];
std::size_t g_pending_count = 0;

// The last sampled value, whether or not it was written. Comparing against it
// rather than against the last written value is what makes "on change" mean
// the resolution changed, not that reporting started.
double g_last_sampled_milliseconds = -1.0;

}  // namespace

void NoteTimerResolution(const char* source)
{
    TimerResolution resolution;
    if (source == nullptr || !QueryTimerResolution(&resolution))
    {
        return;
    }
    const bool changed = resolution.current_milliseconds != g_last_sampled_milliseconds;
    g_last_sampled_milliseconds = resolution.current_milliseconds;
    if (g_re2dj_guest_wait_trace == 0)
    {
        if (changed && g_pending_count < kPendingCapacity)
        {
            g_pending[g_pending_count].source = source;
            g_pending[g_pending_count].resolution = resolution;
            ++g_pending_count;
        }
        return;
    }
    for (std::size_t index = 0; index < g_pending_count; ++index)
    {
        WriteTimerResolutionLine(g_pending[index].source, g_pending[index].resolution);
    }
    g_pending_count = 0;
    if (changed)
    {
        WriteTimerResolutionLine(source, resolution);
    }
}

}  // namespace re2dj::platform::windows
