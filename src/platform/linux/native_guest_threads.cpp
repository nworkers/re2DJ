#include "native_guest_threads.h"

#include <time.h>

#include <algorithm>
#include <cerrno>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <vector>

namespace re2dj::platform::linux
{
namespace
{

// One guest process's lock and threads. A process's scheduler is never
// freed: threads it left blocked keep waiting on it until the host exits.
struct NativeGuestScheduler
{
    std::mutex mutex;
    std::condition_variable changed;
    NativeGuestThread* owner = nullptr;
    NativeGuestThread* main = nullptr;
    std::deque<NativeGuestThread*> waiters;
    std::vector<NativeGuestThread*> threads;
    // No thread other than the main one runs again.
    bool ended = false;
    // Another thread ended the process; the main thread's run ends with this.
    bool terminated = false;
    NativeGuestTermination termination;
};

std::mutex g_current_mutex;
NativeGuestScheduler* g_current = nullptr;

thread_local NativeGuestScheduler* t_scheduler = nullptr;
thread_local NativeGuestThread* t_thread = nullptr;

bool Contains(std::uint32_t start, std::uint32_t end, std::uint32_t address, std::uint32_t size)
{
    return start != 0 && address >= start && address <= end && size <= end - address;
}

}  // namespace

void BeginNativeGuestThreads(NativeGuestThread* main)
{
    auto* scheduler = new NativeGuestScheduler;
    main->main = true;
    scheduler->main = main;
    scheduler->owner = main;
    scheduler->threads.push_back(main);
    {
        const std::lock_guard<std::mutex> lock(g_current_mutex);
        g_current = scheduler;
    }
    t_scheduler = scheduler;
    t_thread = main;
}

void EndNativeGuestThreads()
{
    NativeGuestScheduler* scheduler = t_scheduler;
    if (scheduler == nullptr)
    {
        return;
    }
    {
        const std::lock_guard<std::mutex> lock(scheduler->mutex);
        scheduler->ended = true;
        scheduler->owner = scheduler->main;
        scheduler->threads.clear();
    }
    scheduler->changed.notify_all();
    {
        const std::lock_guard<std::mutex> lock(g_current_mutex);
        if (g_current == scheduler)
        {
            g_current = nullptr;
        }
    }
    t_scheduler = nullptr;
    t_thread = nullptr;
}

void AddNativeGuestThread(NativeGuestThread* thread)
{
    NativeGuestScheduler* scheduler = t_scheduler;
    if (scheduler == nullptr)
    {
        return;
    }
    const std::lock_guard<std::mutex> lock(scheduler->mutex);
    scheduler->threads.push_back(thread);
}

void RemoveNativeGuestThreadAndRelease(NativeGuestThread* thread)
{
    NativeGuestScheduler* scheduler = t_scheduler;
    if (scheduler == nullptr)
    {
        return;
    }
    {
        const std::lock_guard<std::mutex> lock(scheduler->mutex);
        scheduler->threads.erase(std::remove(scheduler->threads.begin(), scheduler->threads.end(), thread),
                                 scheduler->threads.end());
        if (scheduler->owner == thread)
        {
            scheduler->owner = nullptr;
        }
    }
    scheduler->changed.notify_all();
}

void BindNativeGuestThread(NativeGuestThread* thread)
{
    const std::lock_guard<std::mutex> lock(g_current_mutex);
    t_scheduler = g_current;
    t_thread = thread;
}

NativeGuestThread* CurrentNativeGuestThread()
{
    return t_thread;
}

void AcquireNativeGuestLock()
{
    NativeGuestScheduler* scheduler = t_scheduler;
    NativeGuestThread* self = t_thread;
    if (scheduler == nullptr || self == nullptr)
    {
        return;
    }
    std::unique_lock<std::mutex> lock(scheduler->mutex);
    scheduler->waiters.push_back(self);
    for (;;)
    {
        if (scheduler->terminated && self == scheduler->main)
        {
            scheduler->waiters.erase(std::find(scheduler->waiters.begin(), scheduler->waiters.end(), self));
            scheduler->owner = self;
            const NativeGuestTermination termination = scheduler->termination;
            lock.unlock();
            AbandonNativeGuestRun(termination);
        }
        if (!scheduler->ended && scheduler->owner == nullptr && scheduler->waiters.front() == self)
        {
            break;
        }
        scheduler->changed.wait(lock);
    }
    scheduler->waiters.pop_front();
    scheduler->owner = self;
    lock.unlock();
    RestoreNativeGuestTransition(self);
}

void ReleaseNativeGuestLock()
{
    NativeGuestScheduler* scheduler = t_scheduler;
    NativeGuestThread* self = t_thread;
    if (scheduler == nullptr || self == nullptr)
    {
        return;
    }
    SaveNativeGuestTransition(self);
    {
        const std::lock_guard<std::mutex> lock(scheduler->mutex);
        if (scheduler->owner == self)
        {
            scheduler->owner = nullptr;
        }
    }
    scheduler->changed.notify_all();
}

void YieldNativeGuestThread()
{
    NativeGuestScheduler* scheduler = t_scheduler;
    if (scheduler == nullptr || t_thread == nullptr)
    {
        return;
    }
    {
        const std::lock_guard<std::mutex> lock(scheduler->mutex);
        if (scheduler->waiters.empty() && !scheduler->terminated)
        {
            return;
        }
    }
    ReleaseNativeGuestLock();
    AcquireNativeGuestLock();
}

void WaitNativeGuestThread(std::uint32_t milliseconds)
{
    ReleaseNativeGuestLock();
    if (milliseconds != 0)
    {
        timespec remaining = {static_cast<time_t>(milliseconds / 1000U),
                              static_cast<long>((milliseconds % 1000U) * 1000000L)};
        while (clock_nanosleep(CLOCK_MONOTONIC, 0, &remaining, &remaining) == EINTR)
        {
        }
    }
    AcquireNativeGuestLock();
}

void TerminateNativeGuestProcess(const NativeGuestTermination& termination)
{
    NativeGuestScheduler* scheduler = t_scheduler;
    if (scheduler == nullptr)
    {
        return;
    }
    {
        const std::lock_guard<std::mutex> lock(scheduler->mutex);
        scheduler->terminated = true;
        scheduler->ended = true;
        scheduler->termination = termination;
        scheduler->owner = nullptr;
    }
    scheduler->changed.notify_all();
}

bool NativeGuestThreadMemoryContains(std::uint32_t address, std::uint32_t size)
{
    constexpr std::uint32_t kTebSize = 0x1000U;
    NativeGuestScheduler* scheduler = t_scheduler;
    if (scheduler == nullptr)
    {
        const NativeGuestThread* self = t_thread;
        return self != nullptr && (Contains(self->stack_limit, self->stack_base, address, size) ||
                                   Contains(self->teb, self->teb + kTebSize, address, size));
    }
    const std::lock_guard<std::mutex> lock(scheduler->mutex);
    for (const NativeGuestThread* thread : scheduler->threads)
    {
        if (Contains(thread->stack_limit, thread->stack_base, address, size) ||
            Contains(thread->teb, thread->teb + kTebSize, address, size))
        {
            return true;
        }
    }
    return false;
}

}  // namespace re2dj::platform::linux
