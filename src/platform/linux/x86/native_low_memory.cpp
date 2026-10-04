#include "../../native/native_low_memory.h"
#include "../native_host_protection.h"

#include <sys/mman.h>
#include <unistd.h>

namespace re2dj::platform::native
{

bool MapNativeLowMemory(std::uint32_t size,
                        HostProtection protection,
                        NativeLowMemory* mapping,
                        std::string* error)
{
    if (mapping == nullptr || error == nullptr || size == 0 || mapping->memory != nullptr)
    {
        if (error != nullptr) *error = "invalid low memory mapping arguments";
        return false;
    }
    // Every i386 user address is below 4 GiB, so any placement will do.
    void* memory = mmap(nullptr, size, PosixProtection(protection), MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (memory == MAP_FAILED)
    {
        *error = "cannot map guest-addressable memory";
        return false;
    }
    const long page_value = sysconf(_SC_PAGESIZE);
    const std::uint32_t page = page_value <= 0 ? 4096U : static_cast<std::uint32_t>(page_value);
    mapping->memory = memory;
    mapping->address = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(memory));
    mapping->size = (size + page - 1) & ~(page - 1);
    error->clear();
    return true;
}

void ReleaseNativeLowMemory(NativeLowMemory* mapping)
{
    if (mapping != nullptr && mapping->memory != nullptr)
    {
        munmap(mapping->memory, mapping->size);
        *mapping = {};
    }
}

}  // namespace re2dj::platform::native
