#ifndef RE2DJ_HLE_MODULES_GUEST_PE_FACADE_H_
#define RE2DJ_HLE_MODULES_GUEST_PE_FACADE_H_

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "re2dj/hle/modules/guest_module.h"

namespace re2dj::hle::modules
{

struct GuestPeFacadeBuildOptions
{
    runtime::GuestAddress image_base;
    runtime::GuestAddress bridge_address;
    runtime::GuestAddress cleanup_address;
    std::span<const runtime::GuestAddress> export_gates;
};

struct GuestPeFacadeImage
{
    std::vector<std::uint8_t> file_bytes;
    runtime::GuestAddress preferred_base;
    std::uint32_t image_size = 0;
    std::vector<std::uint32_t> export_thunk_rvas;
    std::vector<std::uint16_t> export_ordinals;
};

class GuestPeFacadeBuilder
{
public:
    static bool Build(const GuestModuleDescriptor& descriptor,
                      const GuestPeFacadeBuildOptions& options,
                      GuestPeFacadeImage* image,
                      std::string* error);
};

}  // namespace re2dj::hle::modules

#endif  // RE2DJ_HLE_MODULES_GUEST_PE_FACADE_H_
