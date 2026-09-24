#ifndef RE2DJ_PLATFORM_LINUX_NATIVE_LOW_MEMORY_H_
#define RE2DJ_PLATFORM_LINUX_NATIVE_LOW_MEMORY_H_

#include <cstdint>
#include <string>

namespace re2dj::platform::linux
{

// Anonymous memory placed wholly below 4 GiB so 32-bit guest code can address
// it. The i386 implementation takes any mapping; the x86-64 implementation
// searches the low address range. Released by ReleaseNativeLowMemory.
struct NativeLowMemory
{
    void* memory = nullptr;
    std::uint32_t address = 0;
    std::uint32_t size = 0;
};

bool MapNativeLowMemory(std::uint32_t size,
                        int protection,
                        NativeLowMemory* mapping,
                        std::string* error);
void ReleaseNativeLowMemory(NativeLowMemory* mapping);

}  // namespace re2dj::platform::linux

#endif  // RE2DJ_PLATFORM_LINUX_NATIVE_LOW_MEMORY_H_
