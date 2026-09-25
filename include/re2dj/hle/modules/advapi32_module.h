#ifndef RE2DJ_HLE_MODULES_ADVAPI32_MODULE_H_
#define RE2DJ_HLE_MODULES_ADVAPI32_MODULE_H_

#include "re2dj/hle/modules/guest_module.h"

namespace re2dj::hle::modules
{

// advapi32.dll with the registry exports the Hardlock API and the original
// program resolve. None has been seen called yet, so each is resolvable but
// unimplemented.
GuestModuleDescriptor MakeAdvapi32ModuleDescriptor();

}  // namespace re2dj::hle::modules

#endif  // RE2DJ_HLE_MODULES_ADVAPI32_MODULE_H_
