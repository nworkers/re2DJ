#include "../native_low_memory.h"

#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>

namespace re2dj::platform::linux
{
namespace
{

// Searched top-down so guest mappings sit high in the 32-bit range, as they
// do on an i386 host, leaving the low range to PE images and facades.
constexpr std::uint64_t kLowSearchTop = 0xF0000000ULL;
constexpr std::uint64_t kLowSearchBottom = 0x10000000ULL;
constexpr std::uint64_t kLowSearchStep = 0x100000ULL;

}  // namespace

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
    const long page_value = sysconf(_SC_PAGESIZE);
    const std::uint64_t page = page_value <= 0 ? 4096U : static_cast<std::uint64_t>(page_value);
    const std::uint64_t rounded = (static_cast<std::uint64_t>(size) + page - 1) & ~(page - 1);
    if (rounded > kLowSearchTop - kLowSearchBottom)
    {
        *error = "low memory mapping is too large";
        return false;
    }
    for (std::uint64_t hint = kLowSearchTop - rounded; hint >= kLowSearchBottom;
         hint -= kLowSearchStep)
    {
        void* requested = reinterpret_cast<void*>(static_cast<std::uintptr_t>(hint));
        void* memory = mmap(requested, rounded, protection,
                            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        if (memory == MAP_FAILED)
        {
            if (errno == EEXIST)
            {
                continue;
            }
            *error = "cannot map memory below 4 GiB";
            return false;
        }
        if (memory != requested)
        {
            // A kernel without MAP_FIXED_NOREPLACE treats the address as a hint.
            munmap(memory, rounded);
            continue;
        }
        mapping->memory = memory;
        mapping->address = static_cast<std::uint32_t>(hint);
        mapping->size = static_cast<std::uint32_t>(rounded);
        error->clear();
        return true;
    }
    *error = "no free range below 4 GiB for the mapping";
    return false;
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
