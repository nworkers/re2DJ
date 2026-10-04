#ifndef RE2DJ_PLATFORM_LINUX_NATIVE_HOST_PROTECTION_H_
#define RE2DJ_PLATFORM_LINUX_NATIVE_HOST_PROTECTION_H_

#include "../native/native_host_services.h"

namespace re2dj::platform::native
{

// The PROT_* flags of a HostProtection, for the Linux backends that map
// memory themselves (low memory, the compatibility-mode pages).
int PosixProtection(HostProtection protection);

}  // namespace re2dj::platform::native

#endif  // RE2DJ_PLATFORM_LINUX_NATIVE_HOST_PROTECTION_H_
