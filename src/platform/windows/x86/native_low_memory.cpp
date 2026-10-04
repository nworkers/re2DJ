#define NOMINMAX
#include <windows.h>

#include "../../native/native_low_memory.h"
#include "../native_host_protection.h"

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
    // Every address of a 32-bit process is below 4 GiB, so any placement will do.
    void* memory = VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, WindowsProtection(protection));
    if (memory == nullptr)
    {
        *error = "cannot allocate guest-addressable memory";
        return false;
    }
    SYSTEM_INFO info = {};
    GetSystemInfo(&info);
    const std::uint32_t page = info.dwPageSize == 0 ? 4096U : static_cast<std::uint32_t>(info.dwPageSize);
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
        VirtualFree(mapping->memory, 0, MEM_RELEASE);
        *mapping = {};
    }
}

}  // namespace re2dj::platform::native
