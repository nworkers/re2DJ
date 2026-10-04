#include "native_host_protection.h"

#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include <cerrno>

namespace re2dj::platform::native
{

int PosixProtection(HostProtection protection)
{
    switch (protection)
    {
    case HostProtection::kNone:
        return PROT_NONE;
    case HostProtection::kRead:
        return PROT_READ;
    case HostProtection::kReadWrite:
        return PROT_READ | PROT_WRITE;
    case HostProtection::kReadExecute:
        return PROT_READ | PROT_EXEC;
    case HostProtection::kReadWriteExecute:
        return PROT_READ | PROT_WRITE | PROT_EXEC;
    }
    return PROT_NONE;
}

std::uint32_t HostPageSize()
{
    return static_cast<std::uint32_t>(sysconf(_SC_PAGESIZE));
}

void* HostMapAt(std::uintptr_t address, std::size_t size, HostProtection protection)
{
    // A hint rather than MAP_FIXED, so an address already in use is refused
    // instead of replaced.
    void* memory = mmap(reinterpret_cast<void*>(address), size, PosixProtection(protection),
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED)
    {
        return nullptr;
    }
    if (reinterpret_cast<std::uintptr_t>(memory) != address)
    {
        munmap(memory, size);
        return nullptr;
    }
    return memory;
}

void* HostMapAnywhere(std::size_t size, HostProtection protection)
{
    void* memory = mmap(nullptr, size, PosixProtection(protection), MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return memory == MAP_FAILED ? nullptr : memory;
}

bool HostProtect(void* memory, std::size_t size, HostProtection protection)
{
    return mprotect(memory, size, PosixProtection(protection)) == 0;
}

void HostUnmap(void* memory, std::size_t size)
{
    if (memory != nullptr)
    {
        munmap(memory, size);
    }
}

void HostFlushCode(void* memory, std::size_t size)
{
    __builtin___clear_cache(static_cast<char*>(memory), static_cast<char*>(memory) + size);
}

std::uint64_t HostMonotonicMilliseconds()
{
    // clock_gettime is async-signal-safe, so the legacy I/O trap may call this.
    timespec now = {};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<std::uint64_t>(now.tv_sec) * 1000U + static_cast<std::uint64_t>(now.tv_nsec) / 1000000U;
}

void HostSleepMilliseconds(std::uint32_t milliseconds)
{
    timespec remaining = {static_cast<time_t>(milliseconds / 1000U),
                          static_cast<long>((milliseconds % 1000U) * 1000000L)};
    while (clock_nanosleep(CLOCK_MONOTONIC, 0, &remaining, &remaining) == EINTR)
    {
    }
}

bool HostReadWallClock(HostWallClock* clock)
{
    timespec now = {};
    tm local = {};
    if (clock == nullptr || clock_gettime(CLOCK_REALTIME, &now) != 0 || localtime_r(&now.tv_sec, &local) == nullptr)
    {
        return false;
    }
    clock->unix_seconds = static_cast<std::int64_t>(now.tv_sec);
    clock->nanoseconds = static_cast<std::uint32_t>(now.tv_nsec);
    clock->local_offset_minutes = static_cast<std::int32_t>(local.tm_gmtoff / 60);
    return true;
}

}  // namespace re2dj::platform::native
