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
// DDERR_UNSUPPORTED is E_NOTIMPL and DDERR_OUTOFMEMORY is E_OUTOFMEMORY.
inline constexpr std::uint32_t kDdErrUnsupported = 0x80004001U;
inline constexpr std::uint32_t kDdErrOutOfMemory = 0x8007000EU;
inline constexpr std::uint32_t kDdErrCannotAttachSurface = 0x8876000AU;
inline constexpr std::uint32_t kDdErrInvalidObject = 0x88760082U;
inline constexpr std::uint32_t kDdErrInvalidPixelFormat = 0x88760091U;
inline constexpr std::uint32_t kDdErrNotFound = 0x887600FFU;
inline constexpr std::uint32_t kDdErrDcAlreadyCreated = 0x8876026CU;
inline constexpr std::uint32_t kD3dErrSceneInScene = 0x887602F8U;
inline constexpr std::uint32_t kD3dErrSceneNotInScene = 0x887602F9U;
inline constexpr std::uint32_t kD3dErrVertexBufferLocked = 0x8876080EU;
inline constexpr std::uint32_t kDdErrNotLocked = 0x88760248U;
inline constexpr std::uint32_t kDdErrSurfaceBusy = 0x887601AEU;
inline constexpr std::uint32_t kD3dErrTextureLoadFailed = 0x887602D5U;
inline constexpr std::uint32_t kDdErrInvalidRect = 0x88760096U;
inline constexpr std::uint32_t kDdErrNoColorKey = 0x887600D7U;
inline constexpr std::uint32_t kDdErrNoClipperAttached = 0x887600CDU;
// CLASS_E_NOAGGREGATION, for a creation given an outer unknown.
inline constexpr std::uint32_t kClassENoAggregation = 0x80040110U;

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

// DirectX 6's D3DDEVICEDESC (DIRECT3D_VERSION 0x0600), as EZ2DJ 1st and the
// Windows DX6 facade use it: 252 bytes.
struct D3dDeviceDesc6
{
    std::uint32_t size = 0;
    std::uint32_t flags = 0;
    std::uint32_t color_model = 0;
    std::uint32_t device_caps = 0;
    std::uint32_t transform_caps[2] = {};
    std::uint32_t clipping = 0;
    std::uint32_t lighting_caps[4] = {};
    D3dPrimCaps line_caps;
    D3dPrimCaps triangle_caps;
    std::uint32_t render_bit_depth = 0;
    std::uint32_t z_buffer_bit_depth = 0;
    std::uint32_t max_buffer_size = 0;
    std::uint32_t max_vertex_count = 0;
    std::uint32_t texture_and_stipple_limits[8] = {};
    std::uint32_t max_texture_repeat = 0;
    std::uint32_t max_texture_aspect_ratio = 0;
    std::uint32_t max_anisotropy = 0;
    float guard_band[4] = {};
    float extents_adjust = 0;
    std::uint32_t stencil_caps = 0;
    std::uint32_t fvf_caps = 0;
    std::uint32_t texture_op_caps = 0;
    std::uint16_t max_texture_blend_stages = 0;
    std::uint16_t max_simultaneous_textures = 0;
};
static_assert(sizeof(D3dDeviceDesc6) == 252);

// D3DFINDDEVICESEARCH (92 bytes) and D3DFINDDEVICERESULT (524 bytes, with
// the DirectX 6 device descriptions).
struct D3dFindDeviceSearch
{
    std::uint32_t size = 0;
    std::uint32_t flags = 0;
    std::uint32_t hardware = 0;
    std::uint32_t color_model = 0;
    Guid guid = {};
    std::uint32_t caps = 0;
    D3dPrimCaps primitive_caps;
};
static_assert(sizeof(D3dFindDeviceSearch) == 92);

struct D3dFindDeviceResult
{
    std::uint32_t size = 0;
    Guid guid = {};
    D3dDeviceDesc6 hardware;
    D3dDeviceDesc6 software;
};
static_assert(sizeof(D3dFindDeviceResult) == 524);

// D3DVIEWPORT2, which IDirect3DViewport3 keeps: 44 bytes.
struct D3dViewport2
{
    std::uint32_t size = 0;
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    float clip_x = 0;
    float clip_y = 0;
    float clip_width = 0;
    float clip_height = 0;
    float min_z = 0;
    float max_z = 0;
};
static_assert(sizeof(D3dViewport2) == 44);

// D3DFDS_* and D3DDD_* (d3dcaps.h).
inline constexpr std::uint32_t kD3dFdsHardware = 0x00000004U;
inline constexpr std::uint32_t kD3dDdBClipping = 0x00000010U;
inline constexpr std::uint32_t kD3dDdDeviceRenderBitDepth = 0x00000080U;
inline constexpr std::uint32_t kD3dDdDeviceZBufferBitDepth = 0x00000100U;
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

// D3DMATRIX: sixteen floats, row by row (_11, _12, ... _44).
struct D3dMatrix
{
    std::array<float, 16> values{};
};
static_assert(sizeof(D3dMatrix) == 64);

// D3DVIEWPORT7.
struct D3dViewport7
{
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    float min_z = 0.0f;
    float max_z = 1.0f;
};
static_assert(sizeof(D3dViewport7) == 24);

// D3DCOLORVALUE and D3DMATERIAL7.
struct D3dColorValue
{
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 0.0f;
};
struct D3dMaterial7
{
    D3dColorValue diffuse;
    D3dColorValue ambient;
    D3dColorValue specular;
    D3dColorValue emissive;
    float power = 0.0f;
};
static_assert(sizeof(D3dMaterial7) == 68);

// ddraw.h flags this facade reports.
inline constexpr std::uint32_t kDdCaps3d = 0x00000001U;
inline constexpr std::uint32_t kDdCapsBlt = 0x00000040U;
// IDirectDrawSurface::Blt: DDBLT_COLORFILL and the DDBLTFX it reads (100 bytes,
// dwFillColor at +80).
inline constexpr std::uint32_t kDdBltColorFill = 0x00000400U;
inline constexpr std::uint32_t kDdBltKeySrc = 0x00008000U;
inline constexpr std::uint32_t kDdBltWait = 0x01000000U;
inline constexpr std::uint32_t kDdBltFxSize = 100;
inline constexpr std::uint32_t kDdBltFxFillColorOffset = 80;
// IDirectDrawSurface::BltFast flags.
inline constexpr std::uint32_t kDdBltFastSrcColorKey = 0x00000001U;
inline constexpr std::uint32_t kDdBltFastWait = 0x00000010U;
inline constexpr std::uint32_t kDdCapsColorKey = 0x00400000U;
inline constexpr std::uint32_t kDdCaps2Certified = 0x00000001U;
inline constexpr std::uint32_t kDdCaps2WideSurfaces = 0x00001000U;
inline constexpr std::uint32_t kDdCaps2NoPageLockRequired = 0x00000800U;
inline constexpr std::uint32_t kDdCaps2CanRenderWindowed = 0x00080000U;
inline constexpr std::uint32_t kDdsCapsBackBuffer = 0x00000004U;
inline constexpr std::uint32_t kDdsCapsComplex = 0x00000008U;
inline constexpr std::uint32_t kDdsCapsFlip = 0x00000010U;
inline constexpr std::uint32_t kDdsCapsOffscreenPlain = 0x00000040U;
inline constexpr std::uint32_t kDdsCapsTexture = 0x00001000U;
inline constexpr std::uint32_t kDdsCapsZBuffer = 0x00020000U;
inline constexpr std::uint32_t kDdsCapsPrimarySurface = 0x00000200U;
inline constexpr std::uint32_t kDdsCaps3dDevice = 0x00002000U;
inline constexpr std::uint32_t kDdsCapsVideoMemory = 0x00004000U;
inline constexpr std::uint32_t kDdsdCaps = 0x00000001U;
inline constexpr std::uint32_t kDdckeySrcBlt = 0x00000008U;
inline constexpr std::uint32_t kDdsdHeight = 0x00000002U;
inline constexpr std::uint32_t kDdsdWidth = 0x00000004U;
inline constexpr std::uint32_t kDdsdPitch = 0x00000008U;
inline constexpr std::uint32_t kDdsdBackBufferCount = 0x00000020U;
inline constexpr std::uint32_t kDdsdLpSurface = 0x00000800U;
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

// Device state indices (d3dtypes.h): D3DRENDERSTATETYPE,
// D3DTEXTURESTAGESTATETYPE, and D3DTRANSFORMSTATETYPE members, and the
// values a new device starts with.
inline constexpr std::uint32_t kD3dRenderStateSrcBlend = 19;
inline constexpr std::uint32_t kD3dRenderStateDestBlend = 20;
inline constexpr std::uint32_t kD3dRenderStateCullMode = 22;
inline constexpr std::uint32_t kD3dTssColorOp = 1;
inline constexpr std::uint32_t kD3dTssColorArg1 = 2;
inline constexpr std::uint32_t kD3dTssColorArg2 = 3;
inline constexpr std::uint32_t kD3dTssAddressU = 13;
inline constexpr std::uint32_t kD3dTssAddressV = 14;
inline constexpr std::uint32_t kD3dTssMagFilter = 16;
inline constexpr std::uint32_t kD3dTssMinFilter = 17;
inline constexpr std::uint32_t kD3dTransformWorld = 1;
inline constexpr std::uint32_t kD3dTransformView = 2;
inline constexpr std::uint32_t kD3dTransformProjection = 3;
inline constexpr std::uint32_t kD3dCullCcw = 3;
inline constexpr std::uint32_t kD3dBlendZero = 1;
inline constexpr std::uint32_t kD3dBlendOne = 2;
inline constexpr std::uint32_t kD3dTopModulate = 4;
inline constexpr std::uint32_t kD3dTaDiffuse = 0;
inline constexpr std::uint32_t kD3dTaTexture = 2;
inline constexpr std::uint32_t kD3dTfgPoint = 1;
inline constexpr std::uint32_t kD3dTfnPoint = 1;
inline constexpr std::uint32_t kD3dTAddressWrap = 1;
inline constexpr std::uint32_t kD3dTAddressMirror = 2;
inline constexpr std::uint32_t kD3dTAddressClamp = 3;
inline constexpr std::uint32_t kD3dTfnLinear = 2;
inline constexpr std::uint32_t kD3dCullNone = 1;
inline constexpr std::uint32_t kD3dCullCw = 2;
inline constexpr std::uint32_t kD3dRenderStateZEnable = 7;
inline constexpr std::uint32_t kD3dRenderStateZWriteEnable = 14;
inline constexpr std::uint32_t kD3dRenderStateAlphaTestEnable = 15;
inline constexpr std::uint32_t kD3dRenderStateZFunc = 23;
inline constexpr std::uint32_t kD3dRenderStateAlphaRef = 24;
inline constexpr std::uint32_t kD3dRenderStateAlphaFunc = 25;
inline constexpr std::uint32_t kD3dRenderStateAlphaBlendEnable = 27;
inline constexpr std::uint32_t kD3dRenderStateColorKeyEnable = 41;
// DirectX 7's D3DRENDERSTATE_LIGHTING and D3DRENDERSTATE_AMBIENT.
inline constexpr std::uint32_t kD3dRenderStateLighting = 137;
inline constexpr std::uint32_t kD3dRenderStateAmbient = 139;
// D3DCMPFUNC, D3DCMP_NEVER (1) through D3DCMP_ALWAYS (8).
inline constexpr std::uint32_t kD3dCmpNever = 1;
inline constexpr std::uint32_t kD3dCmpLessEqual = 4;
inline constexpr std::uint32_t kD3dCmpNotEqual = 6;
inline constexpr std::uint32_t kD3dCmpAlways = 8;
// D3DPRIMITIVETYPE members this facade draws.
inline constexpr std::uint32_t kD3dPtLineList = 2;
inline constexpr std::uint32_t kD3dPtTriangleList = 4;
inline constexpr std::uint32_t kD3dPtTriangleStrip = 5;
// Flexible vertex formats: D3DFVF_TLVERTEX, D3DFVF_VERTEX, D3DFVF_LVERTEX.
inline constexpr std::uint32_t kD3dFvfTlVertex = 0x000001C4U;
inline constexpr std::uint32_t kD3dFvfVertex = 0x00000112U;
inline constexpr std::uint32_t kD3dFvfLVertex = 0x000001E2U;
inline constexpr std::uint32_t kD3dClearTarget = 0x00000001U;
inline constexpr std::uint32_t kD3dClearZBuffer = 0x00000002U;
inline constexpr std::uint32_t kDdErrNotFlippable = 0x88760246U;

// IDirectDraw7::EnumSurfaces flags (DDENUMSURFACES_*).
inline constexpr std::uint32_t kDdEnumSurfacesAll = 0x00000001U;
inline constexpr std::uint32_t kDdEnumSurfacesMatch = 0x00000002U;
inline constexpr std::uint32_t kDdEnumSurfacesNoMatch = 0x00000004U;
inline constexpr std::uint32_t kDdEnumSurfacesCanBeCreated = 0x00000008U;
inline constexpr std::uint32_t kDdEnumSurfacesDoesExist = 0x00000010U;

// Interface and device identifiers (ddraw.h, d3d.h).
inline constexpr Guid kIidUnknown = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                     0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46};
// DirectX 6 (EZ2DJ 1st): IDirectDraw, IDirectDraw4, and IDirect3D3.
inline constexpr Guid kIidDirectDraw = {0x80, 0xDB, 0x14, 0x6C, 0x33, 0xA7, 0xCE, 0x11,
                                        0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60};
inline constexpr Guid kIidDirectDraw4 = {0x9A, 0x50, 0x59, 0x9C, 0xBD, 0x39, 0xD1, 0x11,
                                         0x8C, 0x4A, 0x00, 0xC0, 0x4F, 0xD9, 0x30, 0xC5};
inline constexpr Guid kIidDirect3D3 = {0x40, 0x32, 0x22, 0xBB, 0x2B, 0xE7, 0xD0, 0x11,
                                       0xA9, 0xB4, 0x00, 0xAA, 0x00, 0xC0, 0x99, 0x3E};
inline constexpr Guid kIidDirectDrawSurface4 = {0x30, 0x86, 0x2B, 0x0B, 0x35, 0xAD, 0xD0, 0x11,
                                                0x8E, 0xA6, 0x00, 0x60, 0x97, 0x97, 0xEA, 0x5B};
inline constexpr Guid kIidDirect3DViewport3 = {0x61, 0x3B, 0xAB, 0xB0, 0xD7, 0x33, 0xD1, 0x11,
                                               0xA9, 0x81, 0x00, 0xC0, 0x4F, 0xD7, 0xB1, 0x74};
// IID_IDirect3DTexture2, as 1st SE passes it (checked in its decrypted image).
inline constexpr Guid kIidDirect3DTexture2 = {0x02, 0x15, 0x28, 0x93, 0xF8, 0x8C, 0xD0, 0x11,
                                              0x89, 0xAB, 0x00, 0xA0, 0xC9, 0x05, 0x41, 0x29};
inline constexpr Guid kIidDirectDraw7 = {0xC0, 0x5E, 0xE6, 0x15, 0x9C, 0x3B, 0xD2, 0x11,
                                         0xB9, 0x2F, 0x00, 0x60, 0x97, 0x97, 0xEA, 0x5B};
inline constexpr Guid kIidDirectDrawSurface7 = {0x80, 0x5A, 0x67, 0x06, 0x9B, 0x3B, 0xD2, 0x11,
                                                0xB9, 0x2F, 0x00, 0x60, 0x97, 0x97, 0xEA, 0x5B};
inline constexpr Guid kIidDirect3D7 = {0x77, 0x9E, 0x04, 0xF5, 0x61, 0x48, 0xD2, 0x11,
                                       0xA4, 0x07, 0x00, 0xA0, 0xC9, 0x06, 0x29, 0xA8};
inline constexpr Guid kIidDirect3DDevice7 = {0x79, 0x9E, 0x04, 0xF5, 0x61, 0x48, 0xD2, 0x11,
                                             0xA4, 0x07, 0x00, 0xA0, 0xC9, 0x06, 0x29, 0xA8};
inline constexpr Guid kIidDirect3DRgbDevice = {0x60, 0x5C, 0x66, 0xA4, 0x73, 0x26, 0xCF, 0x11,
                                               0xA3, 0x1A, 0x00, 0xAA, 0x00, 0xB9, 0x33, 0x56};
inline constexpr Guid kIidDirect3DHalDevice = {0xE0, 0x3D, 0xE6, 0x84, 0xAA, 0x46, 0xCF, 0x11,
                                               0x81, 0x6F, 0x00, 0x00, 0xC0, 0x20, 0x15, 0x6E};
inline constexpr Guid kIidDirect3DTnLHalDevice = {0x78, 0x9E, 0x04, 0xF5, 0x61, 0x48, 0xD2, 0x11,
                                                  0xA4, 0x07, 0x00, 0xA0, 0xC9, 0x06, 0x29, 0xA8};
inline constexpr Guid kIidDirect3DVertexBuffer7 = {0x7D, 0x9E, 0x04, 0xF5, 0x61, 0x48, 0xD2, 0x11,
                                                   0xA4, 0x07, 0x00, 0xA0, 0xC9, 0x06, 0x29, 0xA8};
inline constexpr Guid kIidDirect3DVertexBuffer = {0x55, 0x35, 0x50, 0x7A, 0x83, 0x4A, 0xD1, 0x11,
                                                  0xA5, 0xDB, 0x00, 0xA0, 0xC9, 0x03, 0x67, 0xF8};

}  // namespace re2dj::directx

#endif  // RE2DJ_DIRECTX_ABI_H_
