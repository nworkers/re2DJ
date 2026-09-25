#ifndef RE2DJ_HLE_MODULES_GUEST_MODULE_H_
#define RE2DJ_HLE_MODULES_GUEST_MODULE_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
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
    // Names the real DLL does not export either. A lookup for one returns NULL
    // on Windows too, so it is an expected answer rather than a missing export.
    std::vector<std::string> absent_exports;
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
    std::vector<std::string> absent_exports;
};

bool ValidateGuestModuleDescriptor(const GuestModuleDescriptor& descriptor,
                                   std::string* error);

// Handler for an export the guest has only been seen to resolve. The address
// is real, but a call fails with a message naming the export instead of
// returning a guessed result.
bool UnimplementedExport(const ImportCall& call, ImportReturn* result, std::string* error);

// True for a DLL a normal Windows installation does not have, such as the
// Citrix client's wfapi.dll, so LoadLibraryA failing for it matches Windows.
// Case-insensitive; the ".dll" extension is optional.
bool IsAbsentGuestModule(std::string_view name);

}  // namespace re2dj::hle::modules

#endif  // RE2DJ_HLE_MODULES_GUEST_MODULE_H_
