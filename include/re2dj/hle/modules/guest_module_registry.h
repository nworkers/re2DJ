#ifndef RE2DJ_HLE_MODULES_GUEST_MODULE_REGISTRY_H_
#define RE2DJ_HLE_MODULES_GUEST_MODULE_REGISTRY_H_

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "re2dj/hle/modules/guest_module.h"

namespace re2dj::hle::modules
{

class GuestModuleRegistry
{
public:
    bool Register(GuestModuleDescriptor descriptor,
                  GuestModuleMapping mapping,
                  std::string* error);

    const RegisteredGuestModule* FindModule(std::string_view name) const;
    const RegisteredGuestModule* FindModule(runtime::GuestAddress handle) const;

    const RegisteredGuestExport* FindExport(runtime::GuestAddress module_handle,
                                            std::string_view name) const;
    const RegisteredGuestExport* FindExport(runtime::GuestAddress module_handle,
                                            std::uint16_t ordinal) const;
    const RegisteredGuestExport* FindExport(const runtime::ImportGate& gate) const;

    std::size_t module_count() const;

private:
    std::vector<std::unique_ptr<RegisteredGuestModule>> modules_;
};

}  // namespace re2dj::hle::modules

#endif  // RE2DJ_HLE_MODULES_GUEST_MODULE_REGISTRY_H_
