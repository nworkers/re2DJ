#include "re2dj/hle/modules/advapi32_module.h"

#include <array>

#include "re2dj/hle/modules/resolve_only_modules.h"

namespace re2dj::hle::modules
{
namespace
{

// The Hardlock API resolves the registry reads; the original program imports
// RegFlushKey (winreg.h signatures).
constexpr ResolveOnlyExport kAdvapi32ResolveOnly[] = {
    {"RegOpenKeyA", 3}, {"RegQueryValueExA", 6}, {"RegCloseKey", 1}, {"RegFlushKey", 1},
};

}  // namespace

GuestModuleDescriptor MakeAdvapi32ModuleDescriptor()
{
    GuestModuleDescriptor descriptor;
    descriptor.name = "advapi32.dll";
    descriptor.aliases = {"advapi32"};
    AddResolveOnlyExports(&descriptor, kAdvapi32ResolveOnly);
    return descriptor;
}

}  // namespace re2dj::hle::modules
