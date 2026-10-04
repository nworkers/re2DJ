#ifndef RE2DJ_PLATFORM_NATIVE_NATIVE_PE_IMAGE_H_
#define RE2DJ_PLATFORM_NATIVE_NATIVE_PE_IMAGE_H_

#include <cstdint>
#include <vector>

#include "re2dj/exe/pe_image.h"

namespace re2dj::platform::native
{

struct NativePeImage
{
    void* memory = nullptr;
    std::uint32_t size = 0;
    std::uint32_t entry_point = 0;
};

bool MapNativePe32Image(const std::vector<std::uint8_t>& file,
                        const exe::PeImageInfo& info,
                        std::uint32_t requested_base,
                        NativePeImage* image);

void ReleaseNativePeImage(NativePeImage* image);

}  // namespace re2dj::platform::native

#endif  // RE2DJ_PLATFORM_NATIVE_NATIVE_PE_IMAGE_H_
