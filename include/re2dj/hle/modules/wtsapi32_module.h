#ifndef RE2DJ_HLE_MODULES_WTSAPI32_MODULE_H_
#define RE2DJ_HLE_MODULES_WTSAPI32_MODULE_H_

#include <cstdint>

#include "re2dj/hle/modules/guest_module.h"

namespace re2dj::hle::modules
{

// WtsApi32.h values the facade accepts.
inline constexpr std::uint32_t kWtsCurrentServerHandle = 0;
inline constexpr std::uint32_t kWtsCurrentSession = 0xFFFFFFFFU;
inline constexpr std::uint32_t kWtsSessionId = 4;

// The session the guest runs in: 0, the console session the protection
// advanced with on Windows (Task 360).
inline constexpr std::uint32_t kWtsGuestSessionId = 0;

// wtsapi32.dll answering the current session's ID from the guest heap.
GuestModuleDescriptor MakeWtsapi32ModuleDescriptor();

}  // namespace re2dj::hle::modules

#endif  // RE2DJ_HLE_MODULES_WTSAPI32_MODULE_H_
