#ifndef RE2DJ_PLATFORM_LINUX_NATIVE_THREAD_PROBE_H_
#define RE2DJ_PLATFORM_LINUX_NATIVE_THREAD_PROBE_H_

namespace re2dj::platform::linux
{

// Guest threads on either width, with the synthetic PE32: its first import
// starts a guest thread and waits for it. The thread calls an import of its
// own, reads its TEB through FS, and returns; the main thread then finishes
// the run as usual (exit 51). Width-neutral; both in-process probes run it.
bool RunNativeGuestThreadProbe(const char* width);

// A guest thread that faults ends the process: the main thread's run ends
// with the thread's fault while it waits.
bool RunNativeGuestThreadFaultProbe(const char* width);

}  // namespace re2dj::platform::linux

#endif  // RE2DJ_PLATFORM_LINUX_NATIVE_THREAD_PROBE_H_
