#ifndef RE2DJ_PLATFORM_WINDOWS_NATIVE_HOST_PROTECTION_H_
#define RE2DJ_PLATFORM_WINDOWS_NATIVE_HOST_PROTECTION_H_

#include <windows.h>

#include "../native/native_host_services.h"

namespace re2dj::platform::native
{

// The PAGE_* value of a HostProtection, for the Windows backends that
// allocate memory themselves.
DWORD WindowsProtection(HostProtection protection);

}  // namespace re2dj::platform::native

#endif  // RE2DJ_PLATFORM_WINDOWS_NATIVE_HOST_PROTECTION_H_
