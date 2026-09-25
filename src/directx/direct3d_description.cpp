#include "re2dj/directx/direct3d_description.h"

#include <array>

namespace re2dj::directx
{
namespace
{

D3dPrimCaps PrimitiveCaps()
{
    D3dPrimCaps caps;
    caps.size = sizeof(D3dPrimCaps);
    caps.misc_caps = kD3dPMiscCapsCullNone | kD3dPMiscCapsCullCw | kD3dPMiscCapsCullCcw | kD3dPMiscCapsMaskZ;
    caps.raster_caps = kD3dPRasterCapsDither | kD3dPRasterCapsSubPixel | kD3dPRasterCapsZTest |
                       kD3dPRasterCapsFogVertex | kD3dPRasterCapsFogTable | kD3dPRasterCapsZBias |
                       kD3dPRasterCapsWBuffer | kD3dPRasterCapsWFog | kD3dPRasterCapsZFog;
    caps.z_compare_caps = kD3dPCmpCapsAll;
    caps.source_blend_caps = kD3dPBlendCapsThroughSrcAlphaSat;
    caps.dest_blend_caps = caps.source_blend_caps;
    caps.alpha_compare_caps = caps.z_compare_caps;
    caps.shade_caps = kD3dPShadeCapsColorGouraudRgb | kD3dPShadeCapsSpecularGouraudRgb |
                      kD3dPShadeCapsAlphaGouraudBlend | kD3dPShadeCapsFogGouraud;
    caps.texture_caps = kD3dPTextureCapsPerspective | kD3dPTextureCapsAlpha | kD3dPTextureCapsTransparency |
                        kD3dPTextureCapsAlphaPalette;
    caps.texture_filter_caps = kD3dPTFilterCapsNearest | kD3dPTFilterCapsLinear | kD3dPTFilterCapsMipNearest |
                               kD3dPTFilterCapsMipLinear | kD3dPTFilterCapsLinearMipNearest |
                               kD3dPTFilterCapsLinearMipLinear | kD3dPTFilterCapsMagFPoint |
                               kD3dPTFilterCapsMagFLinear | kD3dPTFilterCapsMinFPoint |
                               kD3dPTFilterCapsMinFLinear | kD3dPTFilterCapsMipFPoint |
                               kD3dPTFilterCapsMipFLinear;
    caps.texture_blend_caps = kD3dPTBlendCapsDecal | kD3dPTBlendCapsModulate | kD3dPTBlendCapsDecalAlpha |
                              kD3dPTBlendCapsModulateAlpha | kD3dPTBlendCapsCopy | kD3dPTBlendCapsAdd;
    caps.texture_address_caps = kD3dPTAddressCapsWrap | kD3dPTAddressCapsMirror | kD3dPTAddressCapsClamp |
                                kD3dPTAddressCapsBorder | kD3dPTAddressCapsIndependentUv;
    return caps;
}

constexpr std::array<Direct3DDevice, 3> kDirect3D7Devices = {{
    {"Microsoft Direct3D RGB Software Emulation", "RGB Emulation", kIidDirect3DRgbDevice, false},
    {"Microsoft Direct3D Hardware acceleration through Direct3D HAL", "Direct3D HAL", kIidDirect3DHalDevice,
     false},
    {"Microsoft Direct3D Hardware Transform and Lighting acceleration capable device", "Direct3D T&L HAL",
     kIidDirect3DTnLHalDevice, true},
}};

}  // namespace

std::span<const Direct3DDevice> Direct3D7Devices()
{
    return kDirect3D7Devices;
}

D3dDeviceDesc7 DeviceDescription(const Guid& device_guid, bool hardware_transform_and_light)
{
    D3dDeviceDesc7 desc;
    desc.device_caps = kD3dDevCapsFloatTlVertex | kD3dDevCapsExecuteSystemMemory |
                       kD3dDevCapsTlVertexSystemMemory | kD3dDevCapsTextureSystemMemory |
                       kD3dDevCapsTextureVideoMemory | kD3dDevCapsDrawPrimTlVertex |
                       kD3dDevCapsCanRenderAfterFlip | kD3dDevCapsDrawPrimitives2 |
                       kD3dDevCapsDrawPrimitives2Ex | kD3dDevCapsHwRasterization;
    if (hardware_transform_and_light)
    {
        desc.device_caps |= kD3dDevCapsHwTransformAndLight;
    }
    desc.line_caps = PrimitiveCaps();
    desc.triangle_caps = PrimitiveCaps();
    desc.render_bit_depths = kDdbd16 | kDdbd24 | kDdbd32;
    desc.z_buffer_bit_depths = kDdbd16 | kDdbd24 | kDdbd32;
    desc.min_texture_width = 1;
    desc.min_texture_height = 1;
    desc.max_texture_width = 2048;
    desc.max_texture_height = 2048;
    desc.max_texture_repeat = 2048;
    desc.max_texture_aspect_ratio = 2048;
    desc.max_anisotropy = 1;
    desc.guard_band_left = -32768.0f;
    desc.guard_band_top = -32768.0f;
    desc.guard_band_right = 32768.0f;
    desc.guard_band_bottom = 32768.0f;
    // The low bits of dwFVFCaps are how many texture coordinate sets a
    // flexible vertex format may carry.
    desc.fvf_caps = 8;
    desc.texture_op_caps = kD3dTexOpCapsThroughBlendCurrentAlpha;
    desc.max_texture_blend_stages = 8;
    desc.max_simultaneous_textures = 8;
    desc.max_active_lights = 8;
    desc.max_vertex_w = 1.0e10f;
    desc.device_guid = device_guid;
    desc.max_user_clip_planes = 6;
    desc.max_vertex_blend_matrices = 1;
    desc.vertex_processing_caps = kD3dVtxPCapsTexGen | kD3dVtxPCapsMaterialSource7 | kD3dVtxPCapsVertexFog |
                                  kD3dVtxPCapsDirectionalLights | kD3dVtxPCapsPositionalLights |
                                  kD3dVtxPCapsLocalViewer;
    return desc;
}

D3dDeviceDesc7 CreatedDeviceDescription()
{
    return DeviceDescription(kIidDirect3DHalDevice, false);
}

DdPixelFormat Depth16Format()
{
    DdPixelFormat format;
    format.size = sizeof(DdPixelFormat);
    format.flags = kDdpfZBuffer;
    format.bit_count = 16;
    format.green_mask = 0x0000FFFFU;
    return format;
}

DdPixelFormat Rgb565Format()
{
    DdPixelFormat format;
    format.size = sizeof(DdPixelFormat);
    format.flags = kDdpfRgb;
    format.bit_count = 16;
    format.red_mask = 0xF800U;
    format.green_mask = 0x07E0U;
    format.blue_mask = 0x001FU;
    return format;
}

}  // namespace re2dj::directx
