#ifndef RE2DJ_HLE_MODULES_DDRAW_MODULE_H_
#define RE2DJ_HLE_MODULES_DDRAW_MODULE_H_

#include <array>
#include <cstdint>

#include "re2dj/hle/modules/guest_module.h"

namespace re2dj::hle::modules
{

// ddraw.h results.
inline constexpr std::uint32_t kDdOk = 0;
inline constexpr std::uint32_t kDdErrInvalidParams = 0x80070057U;

// DirectDrawEnumerateEx flags (ddraw.h).
inline constexpr std::uint32_t kDdEnumAttachedSecondaryDevices = 0x00000001U;
inline constexpr std::uint32_t kDdEnumDetachedSecondaryDevices = 0x00000002U;
inline constexpr std::uint32_t kDdEnumNonDisplayDevices = 0x00000004U;

// The DirectDraw device GUID of \\.\DISPLAY1 as Windows 11 reports it,
// {67685559-3106-11D0-B971-00AA00342F9F}, in memory order.
inline constexpr std::array<std::uint8_t, 16> kDisplay1DeviceGuid = {
    0x59, 0x55, 0x68, 0x67, 0x06, 0x31, 0xD0, 0x11,
    0xB9, 0x71, 0x00, 0xAA, 0x00, 0x34, 0x2F, 0x9F,
};

// ddraw.dll: DirectDrawEnumerateExA and DirectDrawCreateEx, with the
// IDirectDraw7 methods as exports named "IDirectDraw7::<method>" that its
// vtable points at.
GuestModuleDescriptor MakeDdrawModuleDescriptor();

}  // namespace re2dj::hle::modules

#endif  // RE2DJ_HLE_MODULES_DDRAW_MODULE_H_
