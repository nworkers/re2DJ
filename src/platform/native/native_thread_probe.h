#ifndef RE2DJ_PLATFORM_NATIVE_NATIVE_THREAD_PROBE_H_
#define RE2DJ_PLATFORM_NATIVE_NATIVE_THREAD_PROBE_H_

namespace re2dj::platform::native
{

// Guest threads on either width, with the synthetic PE32: its first import
// starts a guest thread and waits for it. The thread calls an import of its
// own, reads its TEB through FS, and returns; the main thread then finishes
// the run as usual (exit 51). OS- and width-neutral; every in-process probe
// runs it, naming itself in host (for example "linux-x86").
bool RunNativeGuestThreadProbe(const char* host);

// A guest thread that faults ends the process: the main thread's run ends
// with the thread's fault while it waits.
bool RunNativeGuestThreadFaultProbe(const char* host);

}  // namespace re2dj::platform::native

#endif  // RE2DJ_PLATFORM_NATIVE_NATIVE_THREAD_PROBE_H_
