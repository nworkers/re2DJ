#ifndef RE2DJ_HLE_MODULES_KERNEL32_MODULE_H_
#define RE2DJ_HLE_MODULES_KERNEL32_MODULE_H_

#include <cstdint>

#include "re2dj/hle/modules/guest_module.h"

namespace re2dj::hle::modules
{

// GetVersion result: 6.2 build 9200 on the NT platform (bit 31 clear), which
// Windows 8.1 and later report to executables without a compatibility
// manifest. It matches the Windows-host condition the protected path was
// observed under; the original cabinet OS value is not established.
inline constexpr std::uint32_t kKernel32GuestVersion = 0x23F00206U;

// INVALID_HANDLE_VALUE as a 32-bit guest handle.
inline constexpr std::uint32_t kKernel32InvalidHandle = 0xFFFFFFFFU;

// GetCurrentProcess's pseudo-handle, (HANDLE)-1 on every Windows version.
inline constexpr std::uint32_t kKernel32CurrentProcess = 0xFFFFFFFFU;

// dwOSVersionInfoSize values GetVersionExA accepts: OSVERSIONINFOA and
// OSVERSIONINFOEXA.
inline constexpr std::uint32_t kKernel32OsVersionInfoSize = 148;
inline constexpr std::uint32_t kKernel32OsVersionInfoExSize = 156;

// The largest DeviceIoControl buffer the facade copies. The Hardlock packets
// it serves are at most a few hundred bytes.
inline constexpr std::uint32_t kKernel32MaximumIoBufferSize = 64U * 1024U;

GuestModuleDescriptor MakeKernel32ModuleDescriptor();

}  // namespace re2dj::hle::modules

#endif  // RE2DJ_HLE_MODULES_KERNEL32_MODULE_H_
