#ifndef RE2DJ_PLATFORM_NATIVE_NATIVE_GUEST_THREADS_H_
#define RE2DJ_PLATFORM_NATIVE_NATIVE_GUEST_THREADS_H_

#include <cstdint>

#include "native_guest_fault.h"

namespace re2dj::platform::native
{

// Guest threads run on host threads of their own, but only one of them runs
// guest code or import handlers at a time: the one holding the guest lock.
// The facade's state is not thread-safe and does not need to be. A thread
// gives the lock up only inside an import: when it waits (Sleep, a blocked
// wait) or, while another thread waits for the lock, as an import starts.
// Guest code that spins without calling an import keeps the others out,
// which Windows' preemptive scheduling would not.

// One guest thread as the platform runs it: its guest stack and TEB, and the
// width's own per-thread state.
struct NativeGuestThread
{
    // The TEB the HLE keeps the thread's state in.
    std::uint32_t teb = 0;
    // The TEB guest code reads through FS when it is not that one: on Windows
    // FS is the host thread's real TEB and the HLE keeps a shadow (design
    // 448). Zero means the same as teb.
    std::uint32_t fs_teb = 0;
    std::uint32_t stack_limit = 0;
    std::uint32_t stack_base = 0;
    // The first thread, whose run is the process's run: it alone reports how
    // the process ended.
    bool main = false;
    void* width_state = nullptr;
};

// How the guest process ended, when a thread other than the main one ended
// it: ExitProcess from that thread, or a fault or stop no handler took.
struct NativeGuestTermination
{
    bool process_exited = false;
    std::uint32_t exit_code = 0;
    NativeGuestFault fault;
};

// Starts the process's threads with main holding the guest lock; any threads
// of an earlier process are left blocked for good.
void BeginNativeGuestThreads(NativeGuestThread* main);
// Ends the process's threads: the others never run guest code again.
void EndNativeGuestThreads();

// Registers a thread the calling lock holder created, before it first runs;
// the thread then acquires the lock itself.
void AddNativeGuestThread(NativeGuestThread* thread);
// Removes a finished thread while holding the lock, then releases it.
void RemoveNativeGuestThreadAndRelease(NativeGuestThread* thread);

// Binds the calling host thread to thread, and returns that binding.
void BindNativeGuestThread(NativeGuestThread* thread);
NativeGuestThread* CurrentNativeGuestThread();

// Takes the lock for the calling thread, waiting behind other waiters. When
// the process ended meanwhile, the main thread's run ends there (through
// AbandonNativeGuestRun) and any other thread blocks for good.
void AcquireNativeGuestLock();
void ReleaseNativeGuestLock();

// Lets waiting threads run first when there are any. Called as each import
// starts.
void YieldNativeGuestThread();
// Releases the lock, sleeps about milliseconds on CLOCK_MONOTONIC (0 only
// yields), and takes it back.
void WaitNativeGuestThread(std::uint32_t milliseconds);

// Ends the process from a thread other than the main one, while holding the
// lock: the main thread's run ends with termination, and the lock stays taken.
void TerminateNativeGuestProcess(const NativeGuestTermination& termination);

// True when [address, address + size) lies in one guest thread's stack or TEB.
bool NativeGuestThreadMemoryContains(std::uint32_t address, std::uint32_t size);

// Per width (x86/ and x64/ native_process_bootstrap.cpp): keeps a thread's
// transition state across a lock hand-over, and ends the main thread's run
// with a termination another thread recorded. The latter does not return.
void SaveNativeGuestTransition(NativeGuestThread* thread);
void RestoreNativeGuestTransition(NativeGuestThread* thread);
[[noreturn]] void AbandonNativeGuestRun(const NativeGuestTermination& termination);

}  // namespace re2dj::platform::native

#endif  // RE2DJ_PLATFORM_NATIVE_NATIVE_GUEST_THREADS_H_
