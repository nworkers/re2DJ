#ifndef RE2DJ_HLE_MODULES_GDI32_MODULE_H_
#define RE2DJ_HLE_MODULES_GDI32_MODULE_H_

#include <cstdint>

#include "re2dj/hle/modules/guest_module.h"

namespace re2dj::hle::modules
{

// GetStockObject's handle for a wingdi.h stock object index, or 0 for an
// index Windows does not have.
std::uint32_t StockObjectHandle(std::uint32_t index);

// gdi32.dll: GetStockObject implemented, the other exports the original
// imports resolve-only.
GuestModuleDescriptor MakeGdi32ModuleDescriptor();

}  // namespace re2dj::hle::modules

#endif  // RE2DJ_HLE_MODULES_GDI32_MODULE_H_
