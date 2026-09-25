#ifndef RE2DJ_DIRECTX_ABI_H_
#define RE2DJ_DIRECTX_ABI_H_

#include <array>
#include <cstdint>

// DirectX 6/7 structures and constants as the 32-bit guest sees them. Every
// field is a fixed-width integer or float, so the layouts are the same on
// either host width and can be copied to and from guest memory byte for byte.
// The Windows adapter checks them against the SDK headers at compile time.
namespace re2dj::directx
{

// A GUID in memory order.
using Guid = std::array<std::uint8_t, 16>;

// HRESULTs (ddraw.h, d3d.h, winerror.h).
inline constexpr std::uint32_t kDdOk = 0;
inline constexpr std::uint32_t kDdErrInvalidParams = 0x80070057U;
inline constexpr std::uint32_t kENoInterface = 0x80004002U;
inline constexpr std::uint32_t kEPointer = 0x80004003U;
// DDERR_GENERIC is E_FAIL.
inline constexpr std::uint32_t kDdErrGeneric = 0x80004005U;
inline constexpr std::uint32_t kDdErrUnsupportedMode = 0x8876024EU;

// Enumeration callback answers (ddraw.h, d3d.h): 0 stops, 1 continues.
inline constexpr std::uint32_t kEnumCancel = 0;
inline constexpr std::uint32_t kEnumContinue = 1;

// DDPIXELFORMAT (32 bytes). The masks share storage with other members: for a
// depth format bit_count is dwZBufferBitDepth and green_mask is dwZBitMask.
struct DdPixelFormat
{
    std::uint32_t size = 0;
    std::uint32_t flags = 0;
    std::uint32_t four_cc = 0;
    std::uint32_t bit_count = 0;
    std::uint32_t red_mask = 0;
    std::uint32_t green_mask = 0;
    std::uint32_t blue_mask = 0;
    std::uint32_t alpha_mask = 0;
};
static_assert(sizeof(DdPixelFormat) == 32);

// DDCOLORKEY (8 bytes).
struct DdColorKey
{
    std::uint32_t low = 0;
    std::uint32_t high = 0;
};
static_assert(sizeof(DdColorKey) == 8);

// DDSCAPS2 (16 bytes).
struct DdsCaps2
{
    std::uint32_t caps = 0;
    std::uint32_t caps2 = 0;
    std::uint32_t caps3 = 0;
    std::uint32_t caps4 = 0;
};
static_assert(sizeof(DdsCaps2) == 16);

// DDSURFACEDESC2 (124 bytes). pitch shares storage with dwLinearSize,
// back_buffer_count with dwDepth, and refresh_rate with dwMipMapCount.
struct DdSurfaceDesc2
{
    std::uint32_t size = 0;
    std::uint32_t flags = 0;
    std::uint32_t height = 0;
    std::uint32_t width = 0;
    std::uint32_t pitch = 0;
    std::uint32_t back_buffer_count = 0;
    std::uint32_t refresh_rate = 0;
    std::uint32_t alpha_bit_depth = 0;
    std::uint32_t reserved = 0;
    std::uint32_t surface = 0;
    DdColorKey dest_overlay_key;
    DdColorKey dest_blt_key;
    DdColorKey source_overlay_key;
    DdColorKey source_blt_key;
    DdPixelFormat pixel_format;
    DdsCaps2 caps;
    std::uint32_t texture_stage = 0;
};
static_assert(sizeof(DdSurfaceDesc2) == 124);

// DDCAPS as DirectX 7 defines it (380 bytes). Only the members this facade
// reports are named; the rest stay zero.
struct DdCaps
{
    std::uint32_t size = 0;
    std::uint32_t caps = 0;
    std::uint32_t caps2 = 0;
    std::array<std::uint32_t, 88> unreported = {};
    DdsCaps2 surface_caps;
};
static_assert(sizeof(DdCaps) == 380);

// D3DPRIMCAPS (56 bytes).
struct D3dPrimCaps
{
    std::uint32_t size = 0;
    std::uint32_t misc_caps = 0;
    std::uint32_t raster_caps = 0;
    std::uint32_t z_compare_caps = 0;
    std::uint32_t source_blend_caps = 0;
    std::uint32_t dest_blend_caps = 0;
    std::uint32_t alpha_compare_caps = 0;
    std::uint32_t shade_caps = 0;
    std::uint32_t texture_caps = 0;
    std::uint32_t texture_filter_caps = 0;
    std::uint32_t texture_blend_caps = 0;
    std::uint32_t texture_address_caps = 0;
    std::uint32_t stipple_width = 0;
    std::uint32_t stipple_height = 0;
};
static_assert(sizeof(D3dPrimCaps) == 56);

// D3DDEVICEDESC7 (236 bytes).
struct D3dDeviceDesc7
{
    std::uint32_t device_caps = 0;
    D3dPrimCaps line_caps;
    D3dPrimCaps triangle_caps;
    std::uint32_t render_bit_depths = 0;
    std::uint32_t z_buffer_bit_depths = 0;
    std::uint32_t min_texture_width = 0;
    std::uint32_t min_texture_height = 0;
    std::uint32_t max_texture_width = 0;
    std::uint32_t max_texture_height = 0;
    std::uint32_t max_texture_repeat = 0;
    std::uint32_t max_texture_aspect_ratio = 0;
    std::uint32_t max_anisotropy = 0;
    float guard_band_left = 0.0f;
    float guard_band_top = 0.0f;
    float guard_band_right = 0.0f;
    float guard_band_bottom = 0.0f;
    float extents_adjust = 0.0f;
    std::uint32_t stencil_caps = 0;
    std::uint32_t fvf_caps = 0;
    std::uint32_t texture_op_caps = 0;
    std::uint16_t max_texture_blend_stages = 0;
    std::uint16_t max_simultaneous_textures = 0;
    std::uint32_t max_active_lights = 0;
    float max_vertex_w = 0.0f;
    Guid device_guid = {};
    std::uint16_t max_user_clip_planes = 0;
    std::uint16_t max_vertex_blend_matrices = 0;
    std::uint32_t vertex_processing_caps = 0;
    std::array<std::uint32_t, 4> reserved = {};
};
static_assert(sizeof(D3dDeviceDesc7) == 236);

// DDDEVICEIDENTIFIER2 (1072 bytes). The LARGE_INTEGER driver version is two
// words and the structure's 8-byte alignment padding is explicit, so the size
// does not depend on how a host aligns 64-bit integers.
struct DdDeviceIdentifier2
{
    std::array<char, 512> driver = {};
    std::array<char, 512> description = {};
    std::uint32_t driver_version_low = 0;
    std::uint32_t driver_version_high = 0;
    std::uint32_t vendor_id = 0;
    std::uint32_t device_id = 0;
    std::uint32_t subsystem_id = 0;
    std::uint32_t revision = 0;
    Guid device_identifier = {};
    std::uint32_t whql_level = 0;
    std::uint32_t padding = 0;
};
static_assert(sizeof(DdDeviceIdentifier2) == 1072);

// ddraw.h flags this facade reports.
inline constexpr std::uint32_t kDdCaps3d = 0x00000001U;
inline constexpr std::uint32_t kDdCapsBlt = 0x00000040U;
inline constexpr std::uint32_t kDdCapsColorKey = 0x00400000U;
inline constexpr std::uint32_t kDdCaps2Certified = 0x00000001U;
inline constexpr std::uint32_t kDdCaps2WideSurfaces = 0x00001000U;
inline constexpr std::uint32_t kDdCaps2NoPageLockRequired = 0x00000800U;
inline constexpr std::uint32_t kDdCaps2CanRenderWindowed = 0x00080000U;
inline constexpr std::uint32_t kDdsCapsBackBuffer = 0x00000004U;
inline constexpr std::uint32_t kDdsCapsPrimarySurface = 0x00000200U;
inline constexpr std::uint32_t kDdsCaps3dDevice = 0x00002000U;
inline constexpr std::uint32_t kDdsCapsVideoMemory = 0x00004000U;
inline constexpr std::uint32_t kDdsdHeight = 0x00000002U;
inline constexpr std::uint32_t kDdsdWidth = 0x00000004U;
inline constexpr std::uint32_t kDdsdPitch = 0x00000008U;
inline constexpr std::uint32_t kDdsdPixelFormat = 0x00001000U;
inline constexpr std::uint32_t kDdsdRefreshRate = 0x00040000U;
inline constexpr std::uint32_t kDdpfAlphaPixels = 0x00000001U;
inline constexpr std::uint32_t kDdpfRgb = 0x00000040U;
inline constexpr std::uint32_t kDdpfZBuffer = 0x00000400U;
inline constexpr std::uint32_t kDdbd16 = 0x00000400U;
inline constexpr std::uint32_t kDdbd24 = 0x00000200U;
inline constexpr std::uint32_t kDdbd32 = 0x00000100U;

// d3dcaps.h flags this facade reports.
inline constexpr std::uint32_t kD3dDevCapsFloatTlVertex = 0x00000001U;
inline constexpr std::uint32_t kD3dDevCapsExecuteSystemMemory = 0x00000010U;
inline constexpr std::uint32_t kD3dDevCapsTlVertexSystemMemory = 0x00000040U;
inline constexpr std::uint32_t kD3dDevCapsTextureSystemMemory = 0x00000100U;
inline constexpr std::uint32_t kD3dDevCapsTextureVideoMemory = 0x00000200U;
inline constexpr std::uint32_t kD3dDevCapsDrawPrimTlVertex = 0x00000400U;
inline constexpr std::uint32_t kD3dDevCapsCanRenderAfterFlip = 0x00000800U;
inline constexpr std::uint32_t kD3dDevCapsDrawPrimitives2 = 0x00002000U;
inline constexpr std::uint32_t kD3dDevCapsDrawPrimitives2Ex = 0x00008000U;
inline constexpr std::uint32_t kD3dDevCapsHwTransformAndLight = 0x00010000U;
inline constexpr std::uint32_t kD3dDevCapsHwRasterization = 0x00080000U;

inline constexpr std::uint32_t kD3dPMiscCapsMaskZ = 0x00000002U;
inline constexpr std::uint32_t kD3dPMiscCapsCullNone = 0x00000010U;
inline constexpr std::uint32_t kD3dPMiscCapsCullCw = 0x00000020U;
inline constexpr std::uint32_t kD3dPMiscCapsCullCcw = 0x00000040U;

inline constexpr std::uint32_t kD3dPRasterCapsDither = 0x00000001U;
inline constexpr std::uint32_t kD3dPRasterCapsZTest = 0x00000010U;
inline constexpr std::uint32_t kD3dPRasterCapsSubPixel = 0x00000020U;
inline constexpr std::uint32_t kD3dPRasterCapsFogVertex = 0x00000080U;
inline constexpr std::uint32_t kD3dPRasterCapsFogTable = 0x00000100U;
inline constexpr std::uint32_t kD3dPRasterCapsZBias = 0x00004000U;
inline constexpr std::uint32_t kD3dPRasterCapsWBuffer = 0x00040000U;
inline constexpr std::uint32_t kD3dPRasterCapsWFog = 0x00100000U;
inline constexpr std::uint32_t kD3dPRasterCapsZFog = 0x00200000U;

// D3DPCMPCAPS_NEVER through D3DPCMPCAPS_ALWAYS.
inline constexpr std::uint32_t kD3dPCmpCapsAll = 0x000000FFU;
// D3DPBLENDCAPS_ZERO through D3DPBLENDCAPS_SRCALPHASAT.
inline constexpr std::uint32_t kD3dPBlendCapsThroughSrcAlphaSat = 0x000007FFU;

inline constexpr std::uint32_t kD3dPShadeCapsColorGouraudRgb = 0x00000008U;
inline constexpr std::uint32_t kD3dPShadeCapsSpecularGouraudRgb = 0x00000200U;
inline constexpr std::uint32_t kD3dPShadeCapsAlphaGouraudBlend = 0x00004000U;
inline constexpr std::uint32_t kD3dPShadeCapsFogGouraud = 0x00080000U;

inline constexpr std::uint32_t kD3dPTextureCapsPerspective = 0x00000001U;
inline constexpr std::uint32_t kD3dPTextureCapsAlpha = 0x00000004U;
inline constexpr std::uint32_t kD3dPTextureCapsTransparency = 0x00000008U;
inline constexpr std::uint32_t kD3dPTextureCapsAlphaPalette = 0x00000080U;

inline constexpr std::uint32_t kD3dPTFilterCapsNearest = 0x00000001U;
inline constexpr std::uint32_t kD3dPTFilterCapsLinear = 0x00000002U;
inline constexpr std::uint32_t kD3dPTFilterCapsMipNearest = 0x00000004U;
inline constexpr std::uint32_t kD3dPTFilterCapsMipLinear = 0x00000008U;
inline constexpr std::uint32_t kD3dPTFilterCapsLinearMipNearest = 0x00000010U;
inline constexpr std::uint32_t kD3dPTFilterCapsLinearMipLinear = 0x00000020U;
inline constexpr std::uint32_t kD3dPTFilterCapsMinFPoint = 0x00000100U;
inline constexpr std::uint32_t kD3dPTFilterCapsMinFLinear = 0x00000200U;
inline constexpr std::uint32_t kD3dPTFilterCapsMipFPoint = 0x00010000U;
inline constexpr std::uint32_t kD3dPTFilterCapsMipFLinear = 0x00020000U;
inline constexpr std::uint32_t kD3dPTFilterCapsMagFPoint = 0x01000000U;
inline constexpr std::uint32_t kD3dPTFilterCapsMagFLinear = 0x02000000U;

inline constexpr std::uint32_t kD3dPTBlendCapsDecal = 0x00000001U;
inline constexpr std::uint32_t kD3dPTBlendCapsModulate = 0x00000002U;
inline constexpr std::uint32_t kD3dPTBlendCapsDecalAlpha = 0x00000004U;
inline constexpr std::uint32_t kD3dPTBlendCapsModulateAlpha = 0x00000008U;
inline constexpr std::uint32_t kD3dPTBlendCapsCopy = 0x00000040U;
inline constexpr std::uint32_t kD3dPTBlendCapsAdd = 0x00000080U;

inline constexpr std::uint32_t kD3dPTAddressCapsWrap = 0x00000001U;
inline constexpr std::uint32_t kD3dPTAddressCapsMirror = 0x00000002U;
inline constexpr std::uint32_t kD3dPTAddressCapsClamp = 0x00000004U;
inline constexpr std::uint32_t kD3dPTAddressCapsBorder = 0x00000008U;
inline constexpr std::uint32_t kD3dPTAddressCapsIndependentUv = 0x00000010U;

// D3DTEXOPCAPS_DISABLE through D3DTEXOPCAPS_BLENDCURRENTALPHA.
inline constexpr std::uint32_t kD3dTexOpCapsThroughBlendCurrentAlpha = 0x0000FFFFU;

inline constexpr std::uint32_t kD3dVtxPCapsTexGen = 0x00000001U;
inline constexpr std::uint32_t kD3dVtxPCapsMaterialSource7 = 0x00000002U;
inline constexpr std::uint32_t kD3dVtxPCapsVertexFog = 0x00000004U;
inline constexpr std::uint32_t kD3dVtxPCapsDirectionalLights = 0x00000008U;
inline constexpr std::uint32_t kD3dVtxPCapsPositionalLights = 0x00000010U;
inline constexpr std::uint32_t kD3dVtxPCapsLocalViewer = 0x00000020U;

// Interface and device identifiers (ddraw.h, d3d.h).
inline constexpr Guid kIidUnknown = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                     0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46};
inline constexpr Guid kIidDirectDraw7 = {0xC0, 0x5E, 0xE6, 0x15, 0x9C, 0x3B, 0xD2, 0x11,
                                         0xB9, 0x2F, 0x00, 0x60, 0x97, 0x97, 0xEA, 0x5B};
inline constexpr Guid kIidDirect3D7 = {0x77, 0x9E, 0x04, 0xF5, 0x61, 0x48, 0xD2, 0x11,
                                       0xA4, 0x07, 0x00, 0xA0, 0xC9, 0x06, 0x29, 0xA8};
inline constexpr Guid kIidDirect3DRgbDevice = {0x60, 0x5C, 0x66, 0xA4, 0x73, 0x26, 0xCF, 0x11,
                                               0xA3, 0x1A, 0x00, 0xAA, 0x00, 0xB9, 0x33, 0x56};
inline constexpr Guid kIidDirect3DHalDevice = {0xE0, 0x3D, 0xE6, 0x84, 0xAA, 0x46, 0xCF, 0x11,
                                               0x81, 0x6F, 0x00, 0x00, 0xC0, 0x20, 0x15, 0x6E};
inline constexpr Guid kIidDirect3DTnLHalDevice = {0x78, 0x9E, 0x04, 0xF5, 0x61, 0x48, 0xD2, 0x11,
                                                  0xA4, 0x07, 0x00, 0xA0, 0xC9, 0x06, 0x29, 0xA8};

}  // namespace re2dj::directx

#endif  // RE2DJ_DIRECTX_ABI_H_
