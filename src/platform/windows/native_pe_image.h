#ifndef RE2DJ_PLATFORM_WINDOWS_NATIVE_PE_IMAGE_H_
#define RE2DJ_PLATFORM_WINDOWS_NATIVE_PE_IMAGE_H_

#include <cstdint>
#include <span>
#include <string>

#include "native_import_thunks.h"
#include "re2dj/exe/pe_image.h"
#include "re2dj/runtime/pe_loader.h"

namespace re2dj::platform::windows
{

struct NativePeImage
{
    void* memory = nullptr;
    std::uint32_t size = 0;
    std::uint32_t load_base = 0;
    std::uint32_t entry_point = 0;
    NativeImportThunkRegion import_thunks;
};

bool MapNativePeImage(std::span<const std::uint8_t> file,
                      const exe::PeImageInfo& info,
                      std::uint32_t requested_base,
                      std::uintptr_t bridge_address,
                      std::uintptr_t cleanup_address,
                      runtime::ImportGateTable* gates,
                      NativePeImage* image,
                      std::string* error);

bool RunNativeTlsCallbacks(const exe::PeImageInfo& info,
                           const NativePeImage& image,
                           std::string* error);

void ReleaseNativePeImage(NativePeImage* image);

}  // namespace re2dj::platform::windows

#endif  // RE2DJ_PLATFORM_WINDOWS_NATIVE_PE_IMAGE_H_
