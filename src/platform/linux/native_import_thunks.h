#ifndef RE2DJ_PLATFORM_LINUX_NATIVE_IMPORT_THUNKS_H_
#define RE2DJ_PLATFORM_LINUX_NATIVE_IMPORT_THUNKS_H_

#include <cstdint>
#include <string>

#include "re2dj/exe/pe_image.h"
#include "re2dj/runtime/pe_loader.h"

namespace re2dj::platform::linux
{

struct NativeImportThunkRegion
{
    void* memory = nullptr;
    std::uint32_t size = 0;
};

bool BindNativeImportThunks(const exe::PeImageInfo& info,
                            void* image_memory,
                            std::uint32_t image_size,
                            std::uintptr_t bridge_address,
                            std::uintptr_t cleanup_address,
                            runtime::ImportGateTable* gates,
                            NativeImportThunkRegion* region,
                            std::string* error);

void ReleaseNativeImportThunks(NativeImportThunkRegion* region);

}  // namespace re2dj::platform::linux

#endif  // RE2DJ_PLATFORM_LINUX_NATIVE_IMPORT_THUNKS_H_
