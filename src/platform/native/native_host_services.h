#ifndef RE2DJ_PLATFORM_NATIVE_NATIVE_HOST_SERVICES_H_
#define RE2DJ_PLATFORM_NATIVE_NATIVE_HOST_SERVICES_H_

#include <cstddef>
#include <cstdint>

// What the shared in-process runner needs from the host OS (task 446):
// memory it maps, protects and releases whole, the instruction cache, the
// page size, clocks and sleeping. Each OS implements it in its own platform
// directory (linux/native_host_services.cpp, windows/x86/...), so nothing
// under src/platform/native/ includes an OS header.
namespace re2dj::platform::native
{

enum class HostProtection
{
    kNone,
    kRead,
    kReadWrite,
    kReadExecute,
    kReadWriteExecute,
};

// The host's page size.
std::uint32_t HostPageSize();

// Maps size bytes of zeroed memory at exactly address, or returns null and
// leaves whatever is there alone.
void* HostMapAt(std::uintptr_t address, std::size_t size, HostProtection protection);
// Maps size bytes of zeroed memory wherever the host places it.
void* HostMapAnywhere(std::size_t size, HostProtection protection);
// Changes the protection of whole pages inside one mapping.
bool HostProtect(void* memory, std::size_t size, HostProtection protection);
// Releases a whole mapping made by HostMapAt or HostMapAnywhere.
void HostUnmap(void* memory, std::size_t size);
// Makes code just written to memory visible to execution.
void HostFlushCode(void* memory, std::size_t size);

// Milliseconds on a clock that never goes back, from an arbitrary origin.
// Safe to call from a guest fault handler.
std::uint64_t HostMonotonicMilliseconds();
// Sleeps for at least milliseconds.
void HostSleepMilliseconds(std::uint32_t milliseconds);

// The wall clock: seconds and nanoseconds since 1970-01-01 UTC, and the
// local time zone's offset from UTC at that moment.
struct HostWallClock
{
    std::int64_t unix_seconds = 0;
    std::uint32_t nanoseconds = 0;
    std::int32_t local_offset_minutes = 0;
};
bool HostReadWallClock(HostWallClock* clock);

}  // namespace re2dj::platform::native

#endif  // RE2DJ_PLATFORM_NATIVE_NATIVE_HOST_SERVICES_H_
