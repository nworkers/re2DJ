#ifndef RE2DJ_HLE_MODULES_WINMM_MODULE_H_
#define RE2DJ_HLE_MODULES_WINMM_MODULE_H_

#include <cstdint>

#include "re2dj/hle/modules/guest_module.h"

namespace re2dj::hle::modules
{

// mmsystem.h results the timer exports return.
inline constexpr std::uint32_t kTimerNoError = 0;
inline constexpr std::uint32_t kTimerNoCanDo = 97;

// winmm.dll: the multimedia timer exports implemented, the mixer exports the
// original imports resolve-only.
GuestModuleDescriptor MakeWinmmModuleDescriptor();

}  // namespace re2dj::hle::modules

#endif  // RE2DJ_HLE_MODULES_WINMM_MODULE_H_
