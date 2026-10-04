#define NOMINMAX
#include <windows.h>

#include "../native/native_host_services.h"
#include "native_guest_reservation.h"
#include "native_host_protection.h"

namespace re2dj::platform::native
{

DWORD WindowsProtection(HostProtection protection)
{
    switch (protection)
    {
    case HostProtection::kNone:
        return PAGE_NOACCESS;
    case HostProtection::kRead:
        return PAGE_READONLY;
    case HostProtection::kReadWrite:
        return PAGE_READWRITE;
    case HostProtection::kReadExecute:
        return PAGE_EXECUTE_READ;
    case HostProtection::kReadWriteExecute:
        return PAGE_EXECUTE_READWRITE;
    }
    return PAGE_NOACCESS;
}

std::uint32_t HostPageSize()
{
    SYSTEM_INFO info = {};
    GetSystemInfo(&info);
    return static_cast<std::uint32_t>(info.dwPageSize);
}

void* HostMapAt(std::uintptr_t address, std::size_t size, HostProtection protection)
{
    // The guest images' range is held from process start; it is let go
    // right before its first image goes there.
    ReleaseGuestImageReservation(address, size);
    // VirtualAlloc refuses an address already in use, and rounds one off the
    // 64 KiB allocation granularity down; either way the result is not the
    // address asked for.
    void* memory = VirtualAlloc(reinterpret_cast<void*>(address), size, MEM_RESERVE | MEM_COMMIT,
                                WindowsProtection(protection));
    if (memory == nullptr)
    {
        return nullptr;
    }
    if (reinterpret_cast<std::uintptr_t>(memory) != address)
    {
        VirtualFree(memory, 0, MEM_RELEASE);
        return nullptr;
    }
    return memory;
}

void* HostMapAnywhere(std::size_t size, HostProtection protection)
{
    return VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, WindowsProtection(protection));
}

bool HostProtect(void* memory, std::size_t size, HostProtection protection)
{
    DWORD previous = 0;
    return VirtualProtect(memory, size, WindowsProtection(protection), &previous) != FALSE;
}

void HostUnmap(void* memory, std::size_t)
{
    // Every mapping is released whole, which is all VirtualFree allows.
    if (memory != nullptr)
    {
        VirtualFree(memory, 0, MEM_RELEASE);
    }
}

void HostFlushCode(void* memory, std::size_t size)
{
    FlushInstructionCache(GetCurrentProcess(), memory, size);
}

std::uint64_t HostMonotonicMilliseconds()
{
    // Callable from the vectored exception handler.
    return GetTickCount64();
}

void HostSleepMilliseconds(std::uint32_t milliseconds)
{
    Sleep(milliseconds);
}

bool HostReadWallClock(HostWallClock* clock)
{
    if (clock == nullptr)
    {
        return false;
    }
    FILETIME utc = {};
    GetSystemTimePreciseAsFileTime(&utc);
    FILETIME local = {};
    if (!FileTimeToLocalFileTime(&utc, &local))
    {
        return false;
    }
    const auto ticks = [](const FILETIME& time) {
        return (static_cast<std::uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
    };
    // FILETIME counts 100 ns from 1601-01-01; Unix time starts 11644473600 s later.
    constexpr std::uint64_t kUnixEpochTicks = 116444736000000000ULL;
    const std::uint64_t utc_ticks = ticks(utc);
    if (utc_ticks < kUnixEpochTicks)
    {
        return false;
    }
    const std::uint64_t since_epoch = utc_ticks - kUnixEpochTicks;
    clock->unix_seconds = static_cast<std::int64_t>(since_epoch / 10000000ULL);
    clock->nanoseconds = static_cast<std::uint32_t>((since_epoch % 10000000ULL) * 100ULL);
    const std::int64_t offset_ticks = static_cast<std::int64_t>(ticks(local)) - static_cast<std::int64_t>(utc_ticks);
    clock->local_offset_minutes = static_cast<std::int32_t>(offset_ticks / 600000000LL);
    return true;
}

}  // namespace re2dj::platform::native
