#ifndef RE2DJ_DIRECTX_DIRECTINPUT_H_
#define RE2DJ_DIRECTX_DIRECTINPUT_H_

#include <array>
#include <bitset>
#include <cstdint>
#include <optional>
#include <span>

#include "re2dj/directx/abi.h"

// DirectInput as the HLE models it: the guest ABI it needs, which devices
// CreateDevice makes, and how a device's state is laid out for the guest.
// Both hosts' facades follow these rules; each reads its own keyboard and
// mouse.
namespace re2dj::directx
{

// HRESULTs (dinput.h): DI_OK is S_OK, DIERR_INVALIDPARAM is E_INVALIDARG.
inline constexpr std::uint32_t kDiOk = 0;
inline constexpr std::uint32_t kDiErrInvalidParam = 0x80070057U;
inline constexpr std::uint32_t kDiErrNoAggregation = 0x80040110U;

inline constexpr std::uint32_t kDidcAttached = 0x00000001U;
inline constexpr std::uint32_t kDisclNonExclusive = 0x00000002U;
inline constexpr std::uint32_t kDisclForeground = 0x00000004U;

// DIDEVCAPS as DirectInput 5 and later define it (44 bytes).
struct DiDevCaps
{
    std::uint32_t size = 0;
    std::uint32_t flags = 0;
    std::array<std::uint32_t, 9> unreported{};
};
static_assert(sizeof(DiDevCaps) == 44);

// DIMOUSESTATE (16 bytes).
struct DiMouseState
{
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::array<std::uint8_t, 4> buttons{};
};
static_assert(sizeof(DiMouseState) == 16);

// Interface and device identifiers (dinput.h), in memory order.
inline constexpr Guid kIidDirectInputA = {0x60, 0x13, 0x52, 0x89, 0x8A, 0xAA, 0xCF, 0x11,
                                          0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00};
inline constexpr Guid kIidDirectInput7A = {0x84, 0xB6, 0x4C, 0x9A, 0x6D, 0x23, 0xD3, 0x11,
                                           0x8E, 0x9D, 0x00, 0xC0, 0x4F, 0x68, 0x44, 0xAE};
inline constexpr Guid kIidDirectInputDeviceA = {0x80, 0xE6, 0x44, 0x59, 0x2E, 0xC9, 0xCF, 0x11,
                                                0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00};
inline constexpr Guid kIidDirectInputDevice7A = {0xBC, 0xC6, 0xD7, 0x57, 0x56, 0x23, 0xD3, 0x11,
                                                 0x8E, 0x9D, 0x00, 0xC0, 0x4F, 0x68, 0x44, 0xAE};
inline constexpr Guid kGuidSysMouse = {0x60, 0x2B, 0x1D, 0x6F, 0xA0, 0xD5, 0xCF, 0x11,
                                       0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00};
inline constexpr Guid kGuidSysKeyboard = {0x61, 0x2B, 0x1D, 0x6F, 0xA0, 0xD5, 0xCF, 0x11,
                                          0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00};

// The interfaces a DirectInput object and its devices answer as themselves.
bool IsDirectInputInterface(const Guid& iid);
bool IsDirectInputDeviceInterface(const Guid& iid);

enum class InputDeviceKind : std::uint8_t
{
    kKeyboard,
    kMouse,
};

// IDirectInput::CreateDevice's devices: the system keyboard and mouse; any
// other instance has no model (nullopt).
std::optional<InputDeviceKind> DeviceKindOf(const Guid& instance);

// What the host reports as held: keys by DirectInput scan code (DIK_*) and
// the left, right, and middle mouse buttons.
struct InputSnapshot
{
    std::bitset<256> keys;
    std::array<bool, 3> mouse_buttons{};
};

// IDirectInputDevice::GetDeviceState's bytes for a buffer of `size` bytes:
// zeroed, then for a keyboard 0x80 at each held key's scan code that fits,
// and for a mouse whose buffer holds a DIMOUSESTATE 0x80 for each held
// button. Movement is never reported.
void ComposeDeviceState(InputDeviceKind kind, const InputSnapshot& snapshot, std::span<std::uint8_t> state);

}  // namespace re2dj::directx

#endif  // RE2DJ_DIRECTX_DIRECTINPUT_H_
