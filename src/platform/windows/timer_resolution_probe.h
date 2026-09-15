#pragma once

namespace re2dj::platform::windows
{

// Reads the process's effective timer resolution, which is what decides how
// far a guest's Sleep request overshoots.
//
// This matters because the guest was written for Windows 9x, where the default
// resolution is 1 ms, while the NT family follows the system timer tick at
// 15.6 ms unless a process asks for better. The guest never asks. Whatever
// resolution it actually runs at therefore comes from somewhere else in this
// process, and this reads the value so that supplier can be identified.
//
// The read must happen inside the guest process: since Windows 10 2004 a
// resolution request applies to the requesting process rather than the system,
// so a value sampled in the launcher is not the guest's value.
struct TimerResolution
{
    // All three in milliseconds, converted from the API's 100 ns units.
    //
    // The API's names run counter to intuition: "minimum" is the coarsest
    // interval the system supports, usually 15.6 ms, and "maximum" the finest,
    // usually 0.5 ms. They are kept here rather than renamed so the values
    // stay traceable to the call that produced them.
    double minimum_milliseconds = 0.0;
    double maximum_milliseconds = 0.0;
    double current_milliseconds = 0.0;
};

// Returns false when NtQueryTimerResolution cannot be resolved or fails, in
// which case nothing is written. It is an undocumented ntdll export, so it is
// resolved dynamically and its absence disables the probe rather than breaking
// the run: this is a diagnostic, and no diagnostic may cost a guest its start.
bool QueryTimerResolution(TimerResolution* resolution);

// Samples the resolution at a named point and records a line when the value
// differs from the previous sample, so a settled process costs one line for
// the whole run rather than one per call.
//
// The point of naming the site is to bracket the suppliers: a value that
// changes across audio or backend initialization names that subsystem, and a
// value already raised at process attach means the supplier is outside this
// process entirely.
//
// Callable before the launcher arms the wait-trace switch. Samples taken
// while the switch is unknown are held, not written, and flushed in order by
// the first sample taken after it is armed; a run that never passes the
// option therefore writes nothing at all. `source` must outlive the process,
// which a string literal does.
void NoteTimerResolution(const char* source);

}  // namespace re2dj::platform::windows
