#ifndef RE2DJ_DIRECTX_DIRECT3D_DESCRIPTION_H_
#define RE2DJ_DIRECTX_DIRECT3D_DESCRIPTION_H_

#include <span>
#include <string_view>

#include "re2dj/directx/abi.h"

// What the Direct3D HLE says about itself: the devices it enumerates, their
// capabilities, and the depth and texture formats. Both hosts' facades answer
// from here. DirectDraw's side is directdraw_description.h.
namespace re2dj::directx
{

// One device IDirect3D7::EnumDevices reports.
struct Direct3DDevice
{
    std::string_view description;
    std::string_view name;
    Guid guid;
    bool hardware_transform_and_light;
};

// The devices in the order DirectX 7 enumerates them: the software rasterizer
// first and the most capable hardware device last, as titles commonly pick by
// walking that order. The names match the retail DirectX 7 strings because
// guests are known to select a device by comparing them.
std::span<const Direct3DDevice> Direct3D7Devices();

// A device's D3DDEVICEDESC7. Every capability the legacy OpenGL backend can
// honour is reported, since a guest that walks these fields rejects a device
// whose caps are left zero.
D3dDeviceDesc7 DeviceDescription(const Guid& device_guid, bool hardware_transform_and_light);
// IDirect3DDevice7::GetCaps: the HAL device the enumeration published.
D3dDeviceDesc7 CreatedDeviceDescription();

// IDirect3D3::FindDevice(lpD3DFDS, lpD3DFDR), as the DirectX 6 facade
// answers EZ2DJ 1st: a search or result of the wrong size is
// DDERR_INVALIDPARAMS; a search that asks for a software device
// (D3DFDS_HARDWARE with bHardware FALSE) is DDERR_NOTFOUND; otherwise the
// HAL device, whose hardware description reports clipping and 16-bit render
// and depth targets, everything else zero. The result is filled only on
// DD_OK.
std::uint32_t FindDevice(const D3dFindDeviceSearch& search, D3dFindDeviceResult* result);

// The DirectX 6 HAL device's hardware description, as FindDevice and
// IDirect3DDevice3::GetCaps give it: clipping and 16-bit render and depth
// targets, everything else zero.
D3dDeviceDesc6 Direct3D3HardwareDescription();

// IDirect3DDevice3::GetCaps(lpD3DHWDevDesc, lpD3DHELDevDesc), as the DX6
// facade answers: a missing hardware description or one of the wrong size is
// DDERR_INVALIDPARAMS; otherwise it is filled, then a software description,
// when given, is zeroed with its size set, or DDERR_INVALIDPARAMS when its
// size is wrong (the hardware one stays filled).
std::uint32_t GetDevice3Caps(D3dDeviceDesc6* hardware, D3dDeviceDesc6* software);

// IDirect3D3::EnumZBufferFormats, as the DirectX 6 facade answers: only the
// HAL device class is DD_OK, anything else DDERR_INVALIDPARAMS; its one
// format is a 16-bit Z-buffer without a Z mask.
std::uint32_t CheckEnumZBufferFormats3(const Guid& device_class);
DdPixelFormat Direct3D3DepthFormat();

// The one depth format IDirect3D7::EnumZBufferFormats offers: 16-bit.
DdPixelFormat Depth16Format();
// The one texture format the shared surface backing stores: RGB565.
DdPixelFormat Rgb565Format();

}  // namespace re2dj::directx

#endif  // RE2DJ_DIRECTX_DIRECT3D_DESCRIPTION_H_
