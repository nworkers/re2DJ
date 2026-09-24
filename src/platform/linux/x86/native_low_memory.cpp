#include "../native_low_memory.h"

#include <sys/mman.h>
#include <unistd.h>

namespace re2dj::platform::linux
{

bool MapNativeLowMemory(std::uint32_t size,
                        int protection,
                        NativeLowMemory* mapping,
                        std::string* error)
{
    if (mapping == nullptr || error == nullptr || size == 0 || mapping->memory != nullptr)
    {
        if (error != nullptr) *error = "invalid low memory mapping arguments";
        return false;
    }
    // Every i386 user address is below 4 GiB, so any placement will do.
    void* memory = mmap(nullptr, size, protection, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
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

}  // namespace re2dj::platform::linux
