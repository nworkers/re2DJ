#ifndef RE2DJ_PLATFORM_LINUX_NATIVE_DYNAMIC_THUNK_H_
#define RE2DJ_PLATFORM_LINUX_NATIVE_DYNAMIC_THUNK_H_

#include <cstdint>
#include <string>

namespace re2dj::platform::linux
{

struct NativeDynamicThunk
{
    void* memory = nullptr;
    std::uint32_t address = 0;
    std::uint32_t size = 0;
};

bool CreateNativeDynamicThunk(std::uint32_t gate_address,
                              NativeDynamicThunk* thunk,
                              std::string* error);

void ReleaseNativeDynamicThunk(NativeDynamicThunk* thunk);

}  // namespace re2dj::platform::linux

#endif  // RE2DJ_PLATFORM_LINUX_NATIVE_DYNAMIC_THUNK_H_
