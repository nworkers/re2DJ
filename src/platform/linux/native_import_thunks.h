#ifndef RE2DJ_PLATFORM_LINUX_NATIVE_IMPORT_THUNKS_H_
#define RE2DJ_PLATFORM_LINUX_NATIVE_IMPORT_THUNKS_H_

#include <cstdint>
#include <string>
#include <vector>

#include "re2dj/exe/pe_image.h"
#include "re2dj/hle/modules/guest_module_registry.h"
#include "re2dj/runtime/pe_loader.h"

namespace re2dj::platform::linux
{

// Bounds on what an import table may ask of the thunk builder: the longest
// module or import name it reads, and the most gates it builds.
inline constexpr std::uint32_t kMaximumImportStringSize = 4096;
inline constexpr std::uint32_t kMaximumImportCount = 65536;

struct NativeImportSlotBinding
{
    std::uint8_t* slot = nullptr;
    runtime::ImportGate gate;
};

struct NativeImportThunkRegion
{
    void* memory = nullptr;
    std::uint32_t size = 0;
    std::vector<NativeImportSlotBinding> slots;
};

struct NativeGuestImportRebinding
{
    runtime::ImportGate gate;
    runtime::GuestAddress thunk_address;
};

bool BindNativeImportThunks(const exe::PeImageInfo& info,
                            void* image_memory,
                            std::uint32_t image_size,
                            std::uintptr_t bridge_address,
                            std::uintptr_t cleanup_address,
                            runtime::ImportGateTable* gates,
                            NativeImportThunkRegion* region,
                            std::string* error);

bool RebindNativeGuestModuleImports(
    NativeImportThunkRegion* region,
    const hle::modules::GuestModuleRegistry& registry,
    std::vector<NativeGuestImportRebinding>* rebindings,
    std::string* error);

void ReleaseNativeImportThunks(NativeImportThunkRegion* region);

}  // namespace re2dj::platform::linux

#endif  // RE2DJ_PLATFORM_LINUX_NATIVE_IMPORT_THUNKS_H_
