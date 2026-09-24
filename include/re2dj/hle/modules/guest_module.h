#ifndef RE2DJ_HLE_MODULES_GUEST_MODULE_H_
#define RE2DJ_HLE_MODULES_GUEST_MODULE_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "re2dj/hle/import_dispatcher.h"
#include "re2dj/runtime/address_space.h"

namespace re2dj::hle::modules
{

struct GuestExportDescriptor
{
    std::string name;
    std::optional<std::uint16_t> ordinal;
    CallingConvention calling_convention = CallingConvention::kStdcall;
    std::uint32_t argument_count = 0;
    ImportHandler handler = nullptr;
};

struct GuestModuleDescriptor
{
    std::string name;
    std::vector<std::string> aliases;
    std::vector<GuestExportDescriptor> exports;
};

struct GuestModuleMapping
{
    runtime::GuestAddress base;
    std::uint32_t image_size = 0;
    std::vector<runtime::GuestAddress> export_thunks;
};

struct RegisteredGuestExport
{
    GuestExportDescriptor descriptor;
    runtime::GuestAddress thunk_address;
};

struct RegisteredGuestModule
{
    std::string name;
    std::vector<std::string> aliases;
    runtime::GuestAddress base;
    std::uint32_t image_size = 0;
    std::vector<RegisteredGuestExport> exports;
};

bool ValidateGuestModuleDescriptor(const GuestModuleDescriptor& descriptor,
                                   std::string* error);

}  // namespace re2dj::hle::modules

#endif  // RE2DJ_HLE_MODULES_GUEST_MODULE_H_
