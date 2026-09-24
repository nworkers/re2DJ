#ifndef RE2DJ_PLATFORM_LINUX_NATIVE_GUEST_MODULE_IMAGE_H_
#define RE2DJ_PLATFORM_LINUX_NATIVE_GUEST_MODULE_IMAGE_H_

#include <cstdint>
#include <span>
#include <string>

#include "re2dj/hle/modules/guest_module.h"

namespace re2dj::platform::linux
{

struct NativeGuestModuleImage
{
    void* memory = nullptr;
    hle::modules::GuestModuleMapping mapping;
};

bool MapNativeGuestModuleImage(
    const hle::modules::GuestModuleDescriptor& descriptor,
    std::span<const runtime::GuestAddress> export_gates,
    std::uintptr_t bridge_address,
    std::uintptr_t cleanup_address,
    std::uint32_t first_candidate_base,
    NativeGuestModuleImage* image,
    std::string* error);

void ReleaseNativeGuestModuleImage(NativeGuestModuleImage* image);

}  // namespace re2dj::platform::linux

#endif  // RE2DJ_PLATFORM_LINUX_NATIVE_GUEST_MODULE_IMAGE_H_
