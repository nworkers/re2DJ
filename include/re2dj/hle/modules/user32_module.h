#ifndef RE2DJ_HLE_MODULES_USER32_MODULE_H_
#define RE2DJ_HLE_MODULES_USER32_MODULE_H_

#include <cstdint>

#include "re2dj/hle/modules/guest_module.h"

namespace re2dj::hle::modules
{

// MessageBox button IDs (winuser.h).
inline constexpr std::uint32_t kIdOk = 1;
inline constexpr std::uint32_t kIdCancel = 2;
inline constexpr std::uint32_t kIdAbort = 3;
inline constexpr std::uint32_t kIdRetry = 4;
inline constexpr std::uint32_t kIdIgnore = 5;
inline constexpr std::uint32_t kIdYes = 6;
inline constexpr std::uint32_t kIdNo = 7;
inline constexpr std::uint32_t kIdTryAgain = 10;
inline constexpr std::uint32_t kIdContinue = 11;

// The ID of the default button a MessageBox of this uType shows: the button
// set from MB_TYPEMASK, the position from MB_DEFMASK, with unknown sets and
// positions falling back to the first button.
std::uint32_t MessageBoxDefaultButton(std::uint32_t type);

GuestModuleDescriptor MakeUser32ModuleDescriptor();

}  // namespace re2dj::hle::modules

#endif  // RE2DJ_HLE_MODULES_USER32_MODULE_H_
