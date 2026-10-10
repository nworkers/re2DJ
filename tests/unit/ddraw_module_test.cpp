#include "re2dj/hle/modules/ddraw_module.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

#include "memory_services.h"
#include "re2dj/directx/direct3d_description.h"
#include "re2dj/directx/directdraw_description.h"
#include "re2dj/graphics/color_depth.h"
#include "re2dj/graphics/true_color.h"
#include "re2dj/hle/guest_process.h"
#include "re2dj/hle/guest_user.h"
#include "re2dj/hle/host_presentation.h"
#include "re2dj/hle/modules/gdi32_module.h"
#include "re2dj/hle/modules/user32_module.h"
#include "test_support.h"

namespace
{

using re2dj::test::CallModuleExport;
using re2dj::test::MemoryServices;
namespace modules = re2dj::hle::modules;

std::string ReadText(const MemoryServices& services, std::uint32_t address)
{
    std::string text;
    std::string error;
    services.ReadGuestString(re2dj::runtime::GuestAddress(address), &text, &error);
    return text;
}

// With DDENUM_ATTACHEDSECONDARYDEVICES the callback sees the primary driver
// (no GUID, no monitor, the Korean description) and then the one monitor's
// device; without it, only the primary. A FALSE answer stops the enumeration.
void CheckEnumerate(re2dj::test::Context& context)
{
    const auto descriptor = modules::MakeDdrawModuleDescriptor();
    MemoryServices services;
    std::vector<std::vector<std::string>> seen;
    services.guest_function = [&](const std::vector<std::uint32_t>& arguments) {
        std::vector<std::uint8_t> guid(16, 0);
        std::string error;
        if (arguments[0] != 0)
        {
            services.ReadGuestBytes(re2dj::runtime::GuestAddress(arguments[0]), guid, &error);
        }
        seen.push_back({arguments[0] == 0 ? "NULL" : std::to_string(guid[0]), ReadText(services, arguments[1]),
                        ReadText(services, arguments[2]), std::to_string(arguments[3]),
                        arguments[4] == 0 ? "0" : "monitor"});
        return 1U;
    };
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "DirectDrawEnumerateExA",
                                             {0x00401000U, 0x55, 7}).eax,
                   modules::kDdOk);
    RE2DJ_CHECK_EQ(context, seen.size(), std::size_t{2});
    if (seen.size() == 2)
    {
        const std::vector<std::string> primary = {
            "NULL", "\xC1\xD6 \xB5\xF0\xBD\xBA\xC7\xC3\xB7\xB9\xC0\xCC \xB5\xE5\xB6\xF3\xC0\xCC\xB9\xF6",
            "display", "85", "0"};
        RE2DJ_CHECK(context, seen[0] == primary);
        const std::vector<std::string> display1 = {"89", "re2DJ Display Adapter", "\\\\.\\DISPLAY1", "85",
                                                   "monitor"};
        RE2DJ_CHECK(context, seen[1] == display1);
    }
    // The strings live only for the call.
    RE2DJ_CHECK_EQ(context, services.Process()->live_blocks(), std::size_t{0});

    seen.clear();
    CallModuleExport(context, services, descriptor, "DirectDrawEnumerateExA", {0x00401000U, 0, 0});
    RE2DJ_CHECK_EQ(context, seen.size(), std::size_t{1});
    seen.clear();
    services.guest_function = [&](const std::vector<std::uint32_t>&) {
        seen.push_back({});
        return 0U;
    };
    CallModuleExport(context, services, descriptor, "DirectDrawEnumerateExA", {0x00401000U, 0, 7});
    RE2DJ_CHECK_EQ(context, seen.size(), std::size_t{1});

    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "DirectDrawEnumerateExA",
                                             {0, 0, 7}).eax,
                   modules::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "DirectDrawEnumerateExA",
                                             {0x00401000U, 0, 8}).eax,
                   modules::kDdErrInvalidParams);

    // DirectDrawEnumerateA: the primary driver alone, four arguments, as
    // measured on Windows 11.
    std::vector<std::size_t> argument_counts;
    services.guest_function = [&](const std::vector<std::uint32_t>& arguments) {
        argument_counts.push_back(arguments.size());
        seen.push_back({arguments[0] == 0 ? "NULL" : "GUID", ReadText(services, arguments[2])});
        return 1U;
    };
    seen.clear();
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "DirectDrawEnumerateA",
                                             {0x00401000U, 0x55}).eax,
                   modules::kDdOk);
    const std::vector<std::vector<std::string>> primary_only = {{"NULL", "display"}};
    RE2DJ_CHECK(context, seen == primary_only);
    RE2DJ_CHECK(context, argument_counts == std::vector<std::size_t>{4});
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "DirectDrawEnumerateA", {0, 0}).eax,
                   modules::kDdErrInvalidParams);
}

// DirectDrawCreateEx gives an IDirectDraw7 whose vtable holds the facade
// exports in ddraw.h order; QueryInterface answers IUnknown and IDirectDraw7
// with the object itself and stops on an older DirectDraw; Release frees it
// at zero.
void CheckCreate(re2dj::test::Context& context)
{
    const auto descriptor = modules::MakeDdrawModuleDescriptor();
    MemoryServices services;
    services.extra_module = "ddraw.dll";
    std::uint32_t next = 0x6F001000U;
    std::size_t method_count = 0;
    for (const auto& export_descriptor : descriptor.exports)
    {
        if (export_descriptor.name.find("::") != std::string::npos)
        {
            services.extra_exports[export_descriptor.name] = next;
            next += 0x10;
            ++method_count;
        }
    }
    // IDirectDraw7's 30 methods, IDirect3D7's 8, IDirectDrawSurface7's 49,
    // IDirect3DDevice7's 49, IDirect3DVertexBuffer7's 9, IDirectDraw4's 28,
    // IDirect3D3's 12, IDirectDrawSurface4's 45, IDirect3DDevice3's 42,
    // IDirect3DViewport3's 21, IDirect3DTexture2's 6, IDirect3DVertexBuffer's 8,
    // and IDirectDrawClipper's 9.
    RE2DJ_CHECK_EQ(context, method_count, std::size_t{316});

    constexpr std::uint32_t kIid = MemoryServices::kBase + 0x40;
    constexpr std::uint32_t kOut = MemoryServices::kBase + 0x60;
    const std::uint8_t direct_draw7[16] = {0xC0, 0x5E, 0xE6, 0x15, 0x9C, 0x3B, 0xD2, 0x11,
                                           0xB9, 0x2F, 0x00, 0x60, 0x97, 0x97, 0xEA, 0x5B};
    const std::uint8_t direct_draw4[16] = {0x9A, 0x50, 0x59, 0x9C, 0xBD, 0x39, 0xD1, 0x11,
                                           0x8C, 0x4A, 0x00, 0xC0, 0x4F, 0xD9, 0x30, 0xC5};
    const auto put_iid = [&](const std::uint8_t (&iid)[16]) {
        for (std::uint32_t index = 0; index < 16; ++index)
        {
            services.Byte(kIid + index) = iid[index];
        }
    };

    put_iid(direct_draw4);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "DirectDrawCreateEx",
                                             {0, kOut, kIid, 0}).eax,
                   modules::kDdErrInvalidParams);
    put_iid(direct_draw7);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "DirectDrawCreateEx",
                                             {0, kOut, kIid, 0}).eax,
                   modules::kDdOk);
    const std::uint32_t object = services.U32(kOut);
    RE2DJ_CHECK(context, object != 0);
    const std::uint32_t vtable = services.U32(object);
    RE2DJ_CHECK_EQ(context, services.U32(vtable), services.extra_exports["IDirectDraw7::QueryInterface"]);
    RE2DJ_CHECK_EQ(context, services.U32(vtable + 20 * 4),
                   services.extra_exports["IDirectDraw7::SetCooperativeLevel"]);

    constexpr std::uint32_t kInterface = MemoryServices::kBase + 0x70;
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "IDirectDraw7::QueryInterface",
                                             {object, kIid, kInterface}).eax,
                   modules::kDdOk);
    RE2DJ_CHECK_EQ(context, services.U32(kInterface), object);
    put_iid(direct_draw4);
    bool handled = true;
    std::string error;
    CallModuleExport(context, services, descriptor, "IDirectDraw7::QueryInterface", {object, kIid, kInterface},
                     &handled, &error);
    RE2DJ_CHECK(context, !handled);
    RE2DJ_CHECK(context, error.find("{9C59509A-39BD-11D1-8C4A-00C04FD930C5}") != std::string::npos);

    // Created with 1, one QueryInterface, then two AddRef and four Release.
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "IDirectDraw7::AddRef", {object}).eax, 3U);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "IDirectDraw7::Release", {object}).eax, 2U);
    CallModuleExport(context, services, descriptor, "IDirectDraw7::Release", {object});
    const std::size_t blocks = services.Process()->live_blocks();
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "IDirectDraw7::Release", {object}).eax, 0U);
    RE2DJ_CHECK_EQ(context, services.Process()->live_blocks(), blocks - 1);
    CallModuleExport(context, services, descriptor, "IDirectDraw7::Release", {object}, &handled);
    RE2DJ_CHECK(context, !handled);
    // Methods not modelled yet stop, naming themselves.
    CallModuleExport(context, services, descriptor, "IDirectDraw7::GetCaps", {object, 0, 0}, &handled, &error);
    RE2DJ_CHECK(context, !handled);
    RE2DJ_CHECK(context, error.find("ddraw.dll!IDirectDraw7::GetCaps") != std::string::npos);
}

// DirectDrawCreate gives EZ2DJ 1st's DirectX 6 object: IUnknown,
// IDirectDraw, and IDirectDraw4 answer with it, IDirect3D3 with a Direct3D
// object whose own QueryInterface leads back; the Direct3D object holds the
// DirectDraw object alive.
void CheckCreateDirectX6(re2dj::test::Context& context)
{
    const auto descriptor = modules::MakeDdrawModuleDescriptor();
    MemoryServices services;
    services.extra_module = "ddraw.dll";
    std::uint32_t next = 0x6F001000U;
    for (const auto& export_descriptor : descriptor.exports)
    {
        services.extra_exports[export_descriptor.name] = next;
        next += 0x10;
    }
    constexpr std::uint32_t kIid = MemoryServices::kBase + 0x40;
    constexpr std::uint32_t kOut = MemoryServices::kBase + 0x60;
    constexpr std::uint32_t kInterface = MemoryServices::kBase + 0x70;
    const auto put_iid = [&](const re2dj::directx::Guid& iid) {
        for (std::uint32_t index = 0; index < 16; ++index)
        {
            services.Byte(kIid + index) = iid[index];
        }
    };
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "DirectDrawCreate", {0, 0, 0}).eax,
                   modules::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "DirectDrawCreate", {0, kOut, 1}).eax,
                   0x80040110U);
    RE2DJ_CHECK_EQ(context, services.U32(kOut), 0U);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "DirectDrawCreate", {0, kOut, 0}).eax,
                   modules::kDdOk);
    const std::uint32_t direct_draw = services.U32(kOut);
    RE2DJ_CHECK(context, direct_draw != 0);
    RE2DJ_CHECK_EQ(context, services.U32(services.U32(direct_draw) + 20 * 4),
                   services.extra_exports["IDirectDraw4::SetCooperativeLevel"]);

    put_iid(re2dj::directx::kIidDirectDraw4);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "IDirectDraw4::QueryInterface",
                                             {direct_draw, kIid, kInterface}).eax,
                   modules::kDdOk);
    RE2DJ_CHECK_EQ(context, services.U32(kInterface), direct_draw);
    put_iid(re2dj::directx::kIidDirect3D3);
    CallModuleExport(context, services, descriptor, "IDirectDraw4::QueryInterface", {direct_draw, kIid, kInterface});
    const std::uint32_t direct3d = services.U32(kInterface);
    RE2DJ_CHECK(context, direct3d != 0 && direct3d != direct_draw);
    RE2DJ_CHECK_EQ(context, services.U32(services.U32(direct3d) + 8 * 4),
                   services.extra_exports["IDirect3D3::CreateDevice"]);
    put_iid(re2dj::directx::kIidDirectDraw);
    CallModuleExport(context, services, descriptor, "IDirect3D3::QueryInterface", {direct3d, kIid, kInterface});
    RE2DJ_CHECK_EQ(context, services.U32(kInterface), direct_draw);

    // Created, then the DirectDraw object's two QueryInterface answers, the
    // Direct3D object's hold, and its own QueryInterface answer: five.
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "IDirectDraw4::AddRef", {direct_draw}).eax,
                   5U);
    // FindDevice writes the shared core's answer into the guest's result.
    constexpr std::uint32_t kSearch = MemoryServices::kBase + 0x1000;
    constexpr std::uint32_t kFound = MemoryServices::kBase + 0x1100;
    services.PutU32(kSearch, 92);
    services.PutU32(kFound, 524);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "IDirect3D3::FindDevice",
                                             {direct3d, kSearch, kFound}).eax,
                   modules::kDdOk);
    RE2DJ_CHECK_EQ(context, services.U32(kFound + 20), 252U);
    RE2DJ_CHECK_EQ(context, services.U32(kFound + 20 + 24), 1U);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "IDirect3D3::FindDevice",
                                             {direct3d, 0, kFound}).eax,
                   modules::kDdErrInvalidParams);

    // SetCooperativeLevel and SetDisplayMode follow the shared core, as
    // IDirectDraw7's do: no window is DDERR_INVALIDPARAMS, 640x480x16 is
    // the one mode set.
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "IDirectDraw4::SetCooperativeLevel",
                                             {direct_draw, 0, 0x811}).eax,
                   modules::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "IDirectDraw4::SetDisplayMode",
                                             {direct_draw, 640, 480, 16, 0, 0}).eax,
                   modules::kDdOk);
    RE2DJ_CHECK(context, CallModuleExport(context, services, descriptor, "IDirectDraw4::SetDisplayMode",
                                          {direct_draw, 800, 600, 16, 0, 0}).eax != modules::kDdOk);
    // Another interface, and a method not modelled yet, stop.
    put_iid(re2dj::directx::kIidDirectDraw7);
    bool handled = true;
    CallModuleExport(context, services, descriptor, "IDirectDraw4::QueryInterface", {direct_draw, kIid, kInterface},
                     &handled);
    RE2DJ_CHECK(context, !handled);
    std::string error;
    CallModuleExport(context, services, descriptor, "IDirectDraw4::GetCaps", {direct_draw, 0, 0}, &handled, &error);
    RE2DJ_CHECK(context, !handled);
    RE2DJ_CHECK(context, error.find("ddraw.dll!IDirectDraw4::GetCaps") != std::string::npos);
}

// A DirectDraw4 object's surfaces are IDirectDrawSurface4s; IDirect3D3
// enumerates the 16-bit depth format and makes an IDirect3DDevice3 on the
// back buffer, whose viewports, current viewport, and GetCaps follow the
// Windows DX6 facade.
void CheckDirectX6Device(re2dj::test::Context& context)
{
    namespace dx = re2dj::directx;
    const auto descriptor = modules::MakeDdrawModuleDescriptor();
    MemoryServices services;
    services.extra_module = "ddraw.dll";
    std::uint32_t next = 0x6F001000U;
    for (const auto& export_descriptor : descriptor.exports)
    {
        services.extra_exports[export_descriptor.name] = next;
        next += 0x10;
    }
    constexpr std::uint32_t kGuid = MemoryServices::kBase + 0x40;
    constexpr std::uint32_t kOut = MemoryServices::kBase + 0x70;
    constexpr std::uint32_t kData = MemoryServices::kBase + 0x1000;
    constexpr std::uint32_t kSecond = MemoryServices::kBase + 0x1200;
    const auto put_guid = [&](const dx::Guid& guid) {
        for (std::uint32_t index = 0; index < 16; ++index)
        {
            services.Byte(kGuid + index) = guid[index];
        }
    };
    const auto call = [&](const char* name, std::initializer_list<std::uint32_t> arguments) {
        return CallModuleExport(context, services, descriptor, name, arguments).eax;
    };
    const auto vtable_entry = [&](std::uint32_t object, std::uint32_t index) {
        return services.U32(services.U32(object) + index * 4);
    };

    call("DirectDrawCreate", {0, kOut, 0});
    const std::uint32_t direct_draw = services.U32(kOut);
    put_guid(dx::kIidDirect3D3);
    call("IDirectDraw4::QueryInterface", {direct_draw, kGuid, kOut});
    const std::uint32_t direct3d = services.U32(kOut);

    // EnumZBufferFormats: the HAL device's one 16-bit format with its
    // context; another class, or no callback, refuses.
    std::vector<std::vector<std::uint32_t>> formats;
    services.guest_function = [&](const std::vector<std::uint32_t>& arguments) {
        formats.push_back({services.U32(arguments[0] + 4), services.U32(arguments[0] + 12),
                           services.U32(arguments[0] + 20), arguments[1]});
        return 1U;
    };
    put_guid(dx::kIidDirect3DHalDevice);
    RE2DJ_CHECK_EQ(context, call("IDirect3D3::EnumZBufferFormats", {direct3d, kGuid, 0x00401000U, 0x5150}),
                   dx::kDdOk);
    RE2DJ_CHECK(context, formats == (std::vector<std::vector<std::uint32_t>>{{dx::kDdpfZBuffer, 16, 0, 0x5150}}));
    RE2DJ_CHECK_EQ(context, call("IDirect3D3::EnumZBufferFormats", {direct3d, kGuid, 0, 0}), dx::kDdErrInvalidParams);
    put_guid(dx::kIidDirect3DRgbDevice);
    RE2DJ_CHECK_EQ(context, call("IDirect3D3::EnumZBufferFormats", {direct3d, kGuid, 0x00401000U, 0}),
                   dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, formats.size(), std::size_t{1});

    // A flipping chain whose surfaces carry IDirectDrawSurface4's vtable and
    // answer its interface.
    dx::DdSurfaceDesc2 request;
    request.size = sizeof(request);
    request.flags = dx::kDdsdCaps | dx::kDdsdBackBufferCount;
    request.caps.caps = 0x00002218U;
    request.back_buffer_count = 1;
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&request);
    for (std::uint32_t index = 0; index < sizeof(request); ++index)
    {
        services.Byte(kData + index) = bytes[index];
    }
    RE2DJ_CHECK_EQ(context, call("IDirectDraw4::CreateSurface", {direct_draw, kData, kOut, 0}), dx::kDdOk);
    const std::uint32_t front = services.U32(kOut);
    RE2DJ_CHECK_EQ(context, vtable_entry(front, 12), services.extra_exports["IDirectDrawSurface4::GetAttachedSurface"]);
    services.PutU32(kData, dx::kDdsCapsBackBuffer);
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface4::GetAttachedSurface", {front, kData, kOut}), dx::kDdOk);
    const std::uint32_t back = services.U32(kOut);
    put_guid(dx::kIidDirectDrawSurface4);
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface4::QueryInterface", {back, kGuid, kOut}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, services.U32(kOut), back);
    call("IDirectDrawSurface4::Release", {back});

    // CreateDevice: the facade's refusals, then a device on the back buffer
    // with IDirect3DDevice3's vtable.
    put_guid(dx::kIidDirect3DHalDevice);
    RE2DJ_CHECK_EQ(context, call("IDirect3D3::CreateDevice", {direct3d, kGuid, back, 0, 0}), dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, call("IDirect3D3::CreateDevice", {direct3d, kGuid, back, kOut, 1}),
                   dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, call("IDirect3D3::CreateDevice", {direct3d, kGuid, direct3d, kOut, 0}),
                   dx::kDdErrInvalidObject);
    RE2DJ_CHECK_EQ(context, services.U32(kOut), 0U);
    RE2DJ_CHECK_EQ(context, call("IDirect3D3::CreateDevice", {direct3d, kGuid, back, kOut, 0}), dx::kDdOk);
    const std::uint32_t device = services.U32(kOut);
    RE2DJ_CHECK_EQ(context, vtable_entry(device, 12), services.extra_exports["IDirect3DDevice3::SetCurrentViewport"]);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice3::GetRenderTarget", {device, kOut}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, services.U32(kOut), back);

    // GetCaps: the hardware description the core gives, a wrong size refused.
    services.PutU32(kData, 252);
    services.PutU32(kSecond, 252);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice3::GetCaps", {device, kData, kSecond}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, services.U32(kData + 4), 0x190U);
    RE2DJ_CHECK_EQ(context, services.U32(kData + 24), 1U);
    RE2DJ_CHECK_EQ(context, services.U32(kSecond), 252U);
    RE2DJ_CHECK_EQ(context, services.U32(kSecond + 24), 0U);
    services.PutU32(kData, 204);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice3::GetCaps", {device, kData, 0}), dx::kDdErrInvalidParams);

    // A viewport: a zeroed D3DVIEWPORT2 with its size, the size checked both
    // ways, and only its own interface.
    RE2DJ_CHECK_EQ(context, call("IDirect3D3::CreateViewport", {direct3d, kOut, 1}), dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, call("IDirect3D3::CreateViewport", {direct3d, kOut, 0}), dx::kDdOk);
    const std::uint32_t viewport = services.U32(kOut);
    RE2DJ_CHECK_EQ(context, vtable_entry(viewport, 17), services.extra_exports["IDirect3DViewport3::SetViewport2"]);
    services.PutU32(kData, 44);
    services.PutU32(kData + 12, 99);
    RE2DJ_CHECK_EQ(context, call("IDirect3DViewport3::GetViewport2", {viewport, kData}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, services.U32(kData + 12), 0U);
    services.PutU32(kData + 12, 640);
    services.PutU32(kData + 16, 480);
    RE2DJ_CHECK_EQ(context, call("IDirect3DViewport3::SetViewport2", {viewport, kData}), dx::kDdOk);
    services.PutU32(kSecond, 40);
    RE2DJ_CHECK_EQ(context, call("IDirect3DViewport3::SetViewport2", {viewport, kSecond}), dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, call("IDirect3DViewport3::GetViewport2", {viewport, kSecond}), dx::kDdErrInvalidParams);
    services.PutU32(kSecond, 44);
    RE2DJ_CHECK_EQ(context, call("IDirect3DViewport3::GetViewport2", {viewport, kSecond}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, services.U32(kSecond + 12), 640U);
    RE2DJ_CHECK_EQ(context, services.U32(kSecond + 16), 480U);
    RE2DJ_CHECK_EQ(context, call("IDirect3DViewport3::QueryInterface", {viewport, kGuid, kOut}), dx::kENoInterface);
    RE2DJ_CHECK_EQ(context, services.U32(kOut), 0U);

    // The device's viewports: one attached, one current, each held.
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice3::GetCurrentViewport", {device, kOut}), dx::kDdErrNotFound);
    RE2DJ_CHECK_EQ(context, services.U32(kOut), 0U);
    const std::uint32_t references = services.Process()->com().Find(viewport)->reference_count;
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice3::AddViewport", {device, viewport}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice3::AddViewport", {device, viewport}), dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice3::SetCurrentViewport", {device, 0}), dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice3::SetCurrentViewport", {device, viewport}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, services.Process()->com().Find(viewport)->reference_count, references + 2);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice3::GetCurrentViewport", {device, kOut}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, services.U32(kOut), viewport);
    call("IDirect3DViewport3::Release", {viewport});
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice3::DeleteViewport", {device, device}), dx::kDdErrNotFound);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice3::DeleteViewport", {device, viewport}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, services.Process()->com().Find(viewport)->reference_count, references);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice3::GetCurrentViewport", {device, kOut}), dx::kDdErrNotFound);

    // Textures (task 422): a texture surface gives IDirect3DTexture2, one
    // block per surface, whose references count on the surface.
    const auto make_texture = [&](std::uint32_t width, std::uint32_t height) {
        dx::DdSurfaceDesc2 texture_desc;
        texture_desc.size = sizeof(texture_desc);
        texture_desc.flags = 0x00001007U;
        texture_desc.caps.caps = 0x10005000U;
        texture_desc.width = width;
        texture_desc.height = height;
        texture_desc.pixel_format = dx::Rgb565Format();
        const auto* desc_bytes = reinterpret_cast<const std::uint8_t*>(&texture_desc);
        for (std::uint32_t index = 0; index < sizeof(texture_desc); ++index)
        {
            services.Byte(kData + index) = desc_bytes[index];
        }
        call("IDirectDraw4::CreateSurface", {direct_draw, kData, kOut, 0});
        return services.U32(kOut);
    };
    const auto references_of = [&](std::uint32_t object) {
        return services.Process()->com().Find(object)->reference_count;
    };
    const std::uint32_t surface = make_texture(4, 2);
    const std::uint32_t other = make_texture(4, 2);
    const std::uint32_t small = make_texture(2, 2);
    put_guid(dx::kIidDirect3DTexture2);
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface4::QueryInterface", {surface, kGuid, kOut}), dx::kDdOk);
    const std::uint32_t texture = services.U32(kOut);
    RE2DJ_CHECK(context, texture != 0 && texture != surface);
    RE2DJ_CHECK_EQ(context, vtable_entry(texture, 5), services.extra_exports["IDirect3DTexture2::Load"]);
    RE2DJ_CHECK_EQ(context, references_of(surface), 2U);
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface4::QueryInterface", {surface, kGuid, kOut}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, services.U32(kOut), texture);
    RE2DJ_CHECK_EQ(context, call("IDirect3DTexture2::AddRef", {texture}), 4U);
    RE2DJ_CHECK_EQ(context, call("IDirect3DTexture2::Release", {texture}), 3U);
    RE2DJ_CHECK_EQ(context, call("IDirect3DTexture2::Release", {texture}), 2U);
    RE2DJ_CHECK_EQ(context, call("IDirect3DTexture2::QueryInterface", {texture, kGuid, kOut}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, services.U32(kOut), texture);
    put_guid(dx::kIidDirectDrawSurface4);
    RE2DJ_CHECK_EQ(context, call("IDirect3DTexture2::QueryInterface", {texture, kGuid, kOut}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, services.U32(kOut), surface);
    RE2DJ_CHECK_EQ(context, references_of(surface), 4U);
    call("IDirectDrawSurface4::Release", {surface});
    call("IDirect3DTexture2::Release", {texture});
    RE2DJ_CHECK_EQ(context, call("IDirect3DTexture2::GetHandle", {texture, device, kOut}), dx::kDdErrUnsupported);
    RE2DJ_CHECK_EQ(context, call("IDirect3DTexture2::PaletteChanged", {texture, 0, 1}), dx::kDdErrUnsupported);
    // A surface that is no texture is not modelled.
    put_guid(dx::kIidDirect3DTexture2);
    bool texture_handled = true;
    CallModuleExport(context, services, descriptor, "IDirectDrawSurface4::QueryInterface", {back, kGuid, kOut},
                     &texture_handled);
    RE2DJ_CHECK(context, !texture_handled);

    // Load: the Windows facade's checks, then rows (padding cleared) and the
    // source color key.
    call("IDirectDrawSurface4::QueryInterface", {other, kGuid, kOut});
    const std::uint32_t other_texture = services.U32(kOut);
    call("IDirectDrawSurface4::QueryInterface", {small, kGuid, kOut});
    const std::uint32_t small_texture = services.U32(kOut);
    RE2DJ_CHECK_EQ(context, call("IDirect3DTexture2::Load", {texture, 0}), dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, call("IDirect3DTexture2::Load", {texture, texture}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, call("IDirect3DTexture2::Load", {texture, surface}), dx::kDdErrInvalidObject);
    RE2DJ_CHECK_EQ(context, call("IDirect3DTexture2::Load", {texture, small_texture}), dx::kD3dErrTextureLoadFailed);
    services.PutU32(kSecond, 0);
    call("IDirectDrawSurface4::GetDC", {other, kSecond});
    const std::uint32_t other_dc = services.U32(kSecond);
    RE2DJ_CHECK_EQ(context, call("IDirect3DTexture2::Load", {texture, other_texture}), dx::kDdErrSurfaceBusy);
    call("IDirectDrawSurface4::ReleaseDC", {other, other_dc});
    const re2dj::hle::GuestBitmap* other_pixels =
        services.Process()->gdi().FindBitmap(services.Process()->gdi().FindDc(other_dc)->bitmap);
    const re2dj::hle::GuestBitmap* own_pixels = nullptr;
    services.PutU32(kSecond, 0);
    call("IDirectDrawSurface4::GetDC", {surface, kSecond});
    own_pixels = services.Process()->gdi().FindBitmap(services.Process()->gdi().FindDc(services.U32(kSecond))->bitmap);
    call("IDirectDrawSurface4::ReleaseDC", {surface, services.U32(kSecond)});
    for (std::uint32_t y = 0; y < 2; ++y)
    {
        for (std::uint32_t offset = 0; offset < own_pixels->pitch; ++offset)
        {
            services.Byte(own_pixels->bits + y * own_pixels->pitch + offset) = 0xCC;
            services.Byte(other_pixels->bits + y * other_pixels->pitch + offset) =
                offset < 8 ? static_cast<std::uint8_t>(y * 8 + offset) : 0xDD;
        }
    }
    services.PutU32(kSecond, 0x0000F800U);
    services.PutU32(kSecond + 4, 0x0000F800U);
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface4::SetColorKey", {other, 8, kSecond}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, call("IDirect3DTexture2::Load", {texture, other_texture}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, services.U32(own_pixels->bits), 0x03020100U);
    RE2DJ_CHECK_EQ(context, services.U32(own_pixels->bits + own_pixels->pitch + 4), 0x0F0E0D0CU);
    if (own_pixels->pitch > 8)
    {
        RE2DJ_CHECK_EQ(context, services.Byte(own_pixels->bits + 8), std::uint8_t{0});
    }

    // Blt as a color fill (the Windows facade's answers): the whole surface or
    // a rectangle; a wrong DDBLTFX, other flags, a rectangle outside, or a
    // held DC refuse.
    constexpr std::uint32_t kFx = MemoryServices::kBase + 0x1400;
    constexpr std::uint32_t kRect = MemoryServices::kBase + 0x1480;
    for (std::uint32_t offset = 0; offset < dx::kDdBltFxSize; offset += 4)
    {
        services.PutU32(kFx + offset, 0);
    }
    services.PutU32(kFx, dx::kDdBltFxSize);
    services.PutU32(kFx + dx::kDdBltFxFillColorOffset, 0x0001F81FU);
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface4::Blt", {other, 0, 0, 0, dx::kDdBltColorFill, kFx}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, services.U32(other_pixels->bits), 0xF81FF81FU);
    RE2DJ_CHECK_EQ(context, services.U32(other_pixels->bits + other_pixels->pitch + 4), 0xF81FF81FU);
    services.PutU32(kFx + dx::kDdBltFxFillColorOffset, 0x07E0U);
    services.PutU32(kRect, 1);
    services.PutU32(kRect + 4, 1);
    services.PutU32(kRect + 8, 3);
    services.PutU32(kRect + 12, 2);
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface4::Blt", {other, kRect, 0, 0, dx::kDdBltColorFill, kFx}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, services.U32(other_pixels->bits + other_pixels->pitch), 0x07E0F81FU);
    RE2DJ_CHECK_EQ(context, services.U32(other_pixels->bits + other_pixels->pitch + 4), 0xF81F07E0U);
    RE2DJ_CHECK_EQ(context, services.U32(other_pixels->bits), 0xF81FF81FU);
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface4::Blt", {other, 0, 0, 0, dx::kDdBltColorFill, 0}),
                   dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface4::Blt", {other, 0, 0, 0, dx::kDdBltColorFill | 0x01000000U, kFx}),
                   dx::kDdErrUnsupported);
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface4::Blt", {other, 0, 0, kRect, dx::kDdBltColorFill, kFx}),
                   dx::kDdErrUnsupported);
    services.PutU32(kRect + 8, 5);
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface4::Blt", {other, kRect, 0, 0, dx::kDdBltColorFill, kFx}),
                   dx::kDdErrInvalidRect);
    services.PutU32(kFx, 96);
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface4::Blt", {other, 0, 0, 0, dx::kDdBltColorFill, kFx}),
                   dx::kDdErrInvalidParams);
    services.PutU32(kFx, dx::kDdBltFxSize);
    call("IDirectDrawSurface4::GetDC", {other, kSecond});
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface4::Blt", {other, 0, 0, 0, dx::kDdBltColorFill, kFx}),
                   dx::kDdErrSurfaceBusy);
    call("IDirectDrawSurface4::ReleaseDC", {other, services.U32(kSecond)});
    // A blit from a surface is BltFast's copy between equal rectangles; a
    // stretch, a DDBLTFX, or other flags are DDERR_UNSUPPORTED.
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface4::Blt", {other, 0, surface, 0, 0, 0}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface4::Blt", {other, 0, surface, 0, dx::kDdBltKeySrc, kFx}),
                   dx::kDdErrUnsupported);
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface4::Blt", {other, 0, surface, 0, dx::kDdBltColorFill, 0}),
                   dx::kDdErrUnsupported);
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface4::Blt", {other, 0, small, 0, 0, 0}), dx::kDdErrUnsupported);
    constexpr std::uint32_t kInto = MemoryServices::kBase + 0x14A0;
    services.PutU32(kInto, 0);
    services.PutU32(kInto + 4, 0);
    services.PutU32(kInto + 8, 2);
    services.PutU32(kInto + 12, 2);
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface4::Blt", {other, kInto, small, 0, dx::kDdBltKeySrc, 0}),
                   dx::kDdErrNoColorKey);
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface4::Blt", {other, kInto, small, 0, dx::kDdBltWait, 0}), dx::kDdOk);

    // BltFast (the Windows facade's answers): a copy with source-keyed pixels
    // left alone, and its refusals.
    {
        const std::uint32_t key_surface = small;
        services.PutU32(kFx + dx::kDdBltFxFillColorOffset, 0x001FU);
        call("IDirectDrawSurface4::Blt", {key_surface, 0, 0, 0, dx::kDdBltColorFill, kFx});
        services.PutU32(kRect, 1);
        services.PutU32(kRect + 4, 0);
        services.PutU32(kRect + 8, 2);
        services.PutU32(kRect + 12, 1);
        services.PutU32(kFx + dx::kDdBltFxFillColorOffset, 0xFFFFU);
        call("IDirectDrawSurface4::Blt", {key_surface, kRect, 0, 0, dx::kDdBltColorFill, kFx});
        services.PutU32(kFx + dx::kDdBltFxFillColorOffset, 0x0000U);
        call("IDirectDrawSurface4::Blt", {other, 0, 0, 0, dx::kDdBltColorFill, kFx});
        RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface4::BltFast", {other, 1, 0, key_surface, 0, 1}),
                       dx::kDdErrNoColorKey);
        services.PutU32(kSecond, 0xFFFFU);
        services.PutU32(kSecond + 4, 0xFFFFU);
        call("IDirectDrawSurface4::SetColorKey", {key_surface, 8, kSecond});
        RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface4::BltFast", {other, 1, 0, key_surface, 0, 0x11}), dx::kDdOk);
        // Row 0: x=1 gets 001F, x=2 keeps 0000 (the keyed FFFF); row 1: 001F 001F.
        RE2DJ_CHECK_EQ(context, services.U32(other_pixels->bits), 0x001F0000U);
        RE2DJ_CHECK_EQ(context, services.U32(other_pixels->bits + 4), 0x00000000U);
        RE2DJ_CHECK_EQ(context, services.U32(other_pixels->bits + other_pixels->pitch), 0x001F0000U);
        RE2DJ_CHECK_EQ(context, services.U32(other_pixels->bits + other_pixels->pitch + 4), 0x0000001FU);
        RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface4::BltFast", {other, 3, 0, key_surface, 0, 0}),
                       dx::kDdErrInvalidRect);
        RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface4::BltFast", {other, 0, 0, 0, 0, 0}), dx::kDdErrUnsupported);
        RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface4::BltFast", {other, 0, 0, key_surface, 0, 2}),
                       dx::kDdErrUnsupported);
        services.PutU32(kRect + 8, 3);
        RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface4::BltFast", {other, 0, 0, key_surface, kRect, 0}),
                       dx::kDdErrInvalidRect);
        // Onto the primary it presents; onto the back buffer it copies.
        RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface4::BltFast", {front, 0, 0, key_surface, 0, 0}), dx::kDdOk);
        RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface4::BltFast", {back, 0, 0, key_surface, 0, 0}), dx::kDdOk);
    }

    // SetTexture takes a texture on stage 0 and holds its surface.
    const std::uint32_t before = references_of(surface);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice3::SetTexture", {device, 1, texture}), dx::kDdErrUnsupported);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice3::SetTexture", {device, 0, surface}), dx::kDdErrInvalidObject);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice3::SetTexture", {device, 0, texture}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, references_of(surface), before + 1);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice3::SetTexture", {device, 0, 0}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, references_of(surface), before);

    // The surface's last reference, whichever interface drops it, frees the
    // texture block with it.
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface4::Release", {surface}), 1U);
    RE2DJ_CHECK_EQ(context, call("IDirect3DTexture2::Release", {texture}), 0U);
    RE2DJ_CHECK(context, services.Process()->com().Find(surface) == nullptr);
    RE2DJ_CHECK(context, services.Process()->com().Find(texture) == nullptr);

    // No facade surface is ever lost.
    RE2DJ_CHECK_EQ(context, call("IDirectDraw4::RestoreAllSurfaces", {direct_draw}), dx::kDdOk);
    // Nothing to restore: the host's display mode is never changed.
    RE2DJ_CHECK_EQ(context, call("IDirectDraw4::RestoreDisplayMode", {direct_draw}), dx::kDdOk);

    // DirectX 6 vertex buffers: IDirect3D3::CreateVertexBuffer's aggregation
    // check, then DirectX 7's buffer with IDirect3DVertexBuffer's vtable.
    services.PutU32(kData, 16);
    services.PutU32(kData + 4, 0);
    services.PutU32(kData + 8, dx::kD3dFvfTlVertex);
    services.PutU32(kData + 12, 4);
    services.PutU32(kOut, 0x12345678U);
    RE2DJ_CHECK_EQ(context, call("IDirect3D3::CreateVertexBuffer", {direct3d, kData, kOut, 0, 1}),
                   dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, services.U32(kOut), 0x12345678U);
    RE2DJ_CHECK_EQ(context, call("IDirect3D3::CreateVertexBuffer", {direct3d, 0, kOut, 0, 0}), dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, services.U32(kOut), 0U);
    RE2DJ_CHECK_EQ(context, call("IDirect3D3::CreateVertexBuffer", {direct3d, kData, kOut, 0, 0}), dx::kDdOk);
    const std::uint32_t vertex_buffer = services.U32(kOut);
    RE2DJ_CHECK_EQ(context, vtable_entry(vertex_buffer, 7), services.extra_exports["IDirect3DVertexBuffer::Optimize"]);
    RE2DJ_CHECK_EQ(context, call("IDirect3DVertexBuffer::Lock", {vertex_buffer, 0, kOut, kSecond}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, services.U32(kSecond), 4U * 32U);
    // A locked buffer does not draw; no indices or count, or no buffer, are
    // invalid parameters.
    constexpr std::uint32_t kIndices = MemoryServices::kBase + 0x1500;
    services.PutU32(kIndices, 0x00010000U);
    services.PutU32(kIndices + 4, 0x00030002U);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice3::DrawIndexedPrimitiveVB", {device, 4, vertex_buffer, kIndices, 3, 0}),
                   dx::kD3dErrVertexBufferLocked);
    RE2DJ_CHECK_EQ(context, call("IDirect3DVertexBuffer::Unlock", {vertex_buffer}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice3::DrawIndexedPrimitiveVB", {device, 4, vertex_buffer, 0, 3, 0}),
                   dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice3::DrawIndexedPrimitiveVB", {device, 4, vertex_buffer, kIndices, 0, 0}),
                   dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice3::DrawIndexedPrimitiveVB", {device, 4, 0, kIndices, 3, 0}),
                   dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice3::DrawPrimitiveVB", {device, 4, 0, 0, 3, 0}), dx::kDdErrInvalidParams);
    call("IDirect3DVertexBuffer::Release", {vertex_buffer});

    // Light states keep what they are given, within the core's table.
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice3::SetLightState", {device, 2, 0xFFFFFFFFU}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice3::GetLightState", {device, 2, kOut}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, services.U32(kOut), 0xFFFFFFFFU);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice3::SetLightState", {device, 256, 1}), dx::kDdErrInvalidParams);

    // A method not modelled yet stops.
    bool handled = true;
    CallModuleExport(context, services, descriptor, "IDirect3DDevice3::GetStats", {device, kData}, &handled);
    RE2DJ_CHECK(context, !handled);
}

// Fills services' extra module with the ddraw facade exports and returns a
// new IDirectDraw7.
std::uint32_t CreateDirectDraw(re2dj::test::Context& context,
                               MemoryServices& services,
                               const re2dj::hle::modules::GuestModuleDescriptor& descriptor)
{
    services.extra_module = "ddraw.dll";
    std::uint32_t next = 0x6F001000U;
    for (const auto& export_descriptor : descriptor.exports)
    {
        services.extra_exports[export_descriptor.name] = next;
        next += 0x10;
    }
    constexpr std::uint32_t kIid = MemoryServices::kBase + 0x40;
    constexpr std::uint32_t kOut = MemoryServices::kBase + 0x60;
    for (std::uint32_t index = 0; index < 16; ++index)
    {
        services.Byte(kIid + index) = re2dj::directx::kIidDirectDraw7[index];
    }
    CallModuleExport(context, services, descriptor, "DirectDrawCreateEx", {0, kOut, kIid, 0});
    return services.U32(kOut);
}

// IDirectDraw7's description methods write the shared core's structures.
void CheckDirectDrawDescriptions(re2dj::test::Context& context)
{
    namespace dx = re2dj::directx;
    const auto descriptor = modules::MakeDdrawModuleDescriptor();
    MemoryServices services;
    const std::uint32_t direct_draw = CreateDirectDraw(context, services, descriptor);
    constexpr std::uint32_t kDriver = MemoryServices::kArenaBase;
    constexpr std::uint32_t kHel = MemoryServices::kArenaBase + 0x200;
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "IDirectDraw7::GetCaps",
                                             {direct_draw, kDriver, kHel}).eax,
                   dx::kDdOk);
    RE2DJ_CHECK_EQ(context, services.U32(kDriver), 380U);
    RE2DJ_CHECK_EQ(context, services.U32(kDriver + 8) & dx::kDdCaps2CanRenderWindowed,
                   dx::kDdCaps2CanRenderWindowed);
    RE2DJ_CHECK_EQ(context, services.U32(kHel + 4), dx::DirectDraw7Caps().caps);

    // Every mode until the callback cancels at the third.
    std::vector<std::uint32_t> widths;
    services.guest_function = [&](const std::vector<std::uint32_t>& arguments) {
        widths.push_back(services.U32(arguments[0] + 12));
        RE2DJ_CHECK_EQ(context, arguments[1], 0x77U);
        return widths.size() < 3 ? 1U : 0U;
    };
    CallModuleExport(context, services, descriptor, "IDirectDraw7::EnumDisplayModes",
                     {direct_draw, 0, 0, 0x77, 0x00401000U});
    RE2DJ_CHECK_EQ(context, widths.size(), std::size_t{3});
    widths.clear();
    services.guest_function = [&](const std::vector<std::uint32_t>& arguments) {
        widths.push_back(services.U32(arguments[0] + 12));
        return 1U;
    };
    CallModuleExport(context, services, descriptor, "IDirectDraw7::EnumDisplayModes",
                     {direct_draw, 0, 0, 0, 0x00401000U});
    RE2DJ_CHECK_EQ(context, widths.size(), dx::DisplayModes().size());

    constexpr std::uint32_t kMode = MemoryServices::kArenaBase + 0x400;
    CallModuleExport(context, services, descriptor, "IDirectDraw7::GetDisplayMode", {direct_draw, kMode});
    RE2DJ_CHECK_EQ(context, services.U32(kMode + 12), 640U);
    RE2DJ_CHECK_EQ(context, services.U32(kMode + 8), 480U);
    RE2DJ_CHECK_EQ(context, services.U32(kMode + 72 + 12), 16U);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "IDirectDraw7::GetDisplayMode",
                                             {direct_draw, 0}).eax,
                   dx::kDdErrInvalidParams);

    constexpr std::uint32_t kWords = MemoryServices::kArenaBase + 0x500;
    CallModuleExport(context, services, descriptor, "IDirectDraw7::GetMonitorFrequency", {direct_draw, kWords});
    RE2DJ_CHECK_EQ(context, services.U32(kWords), 60U);
    CallModuleExport(context, services, descriptor, "IDirectDraw7::GetAvailableVidMem",
                     {direct_draw, 0, kWords, kWords + 4});
    RE2DJ_CHECK_EQ(context, services.U32(kWords + 4), dx::kReportedVideoMemory);

    constexpr std::uint32_t kIdentifier = MemoryServices::kArenaBase + 0x1000;
    CallModuleExport(context, services, descriptor, "IDirectDraw7::GetDeviceIdentifier", {direct_draw, kIdentifier, 0});
    RE2DJ_CHECK_EQ(context, ReadText(services, kIdentifier), std::string("re2dj.dll"));
    RE2DJ_CHECK_EQ(context, ReadText(services, kIdentifier + 512), std::string("re2DJ HLE Direct3D 7"));
}

// QueryInterface(IID_IDirect3D7) gives a Direct3D object that enumerates the
// shared core's three devices and one depth format, and holds the DirectDraw
// object until it is released.
void CheckDirect3D(re2dj::test::Context& context)
{
    namespace dx = re2dj::directx;
    const auto descriptor = modules::MakeDdrawModuleDescriptor();
    MemoryServices services;
    const std::uint32_t direct_draw = CreateDirectDraw(context, services, descriptor);
    constexpr std::uint32_t kIid = MemoryServices::kBase + 0x40;
    constexpr std::uint32_t kOut = MemoryServices::kBase + 0x70;
    for (std::uint32_t index = 0; index < 16; ++index)
    {
        services.Byte(kIid + index) = dx::kIidDirect3D7[index];
    }
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "IDirectDraw7::QueryInterface",
                                             {direct_draw, kIid, kOut}).eax,
                   dx::kDdOk);
    const std::uint32_t direct3d = services.U32(kOut);
    RE2DJ_CHECK(context, direct3d != 0 && direct3d != direct_draw);
    RE2DJ_CHECK_EQ(context, services.U32(services.U32(direct3d)),
                   services.extra_exports["IDirect3D7::QueryInterface"]);
    RE2DJ_CHECK_EQ(context, services.U32(services.U32(direct3d) + 3 * 4),
                   services.extra_exports["IDirect3D7::EnumDevices"]);

    std::vector<std::string> names;
    std::vector<std::uint32_t> device_caps;
    services.guest_function = [&](const std::vector<std::uint32_t>& arguments) {
        names.push_back(ReadText(services, arguments[1]));
        device_caps.push_back(services.U32(arguments[2]));
        RE2DJ_CHECK_EQ(context, arguments[3], 0x99U);
        return 1U;
    };
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "IDirect3D7::EnumDevices",
                                             {direct3d, 0x00401000U, 0x99}).eax,
                   dx::kDdOk);
    const std::vector<std::string> expected = {"RGB Emulation", "Direct3D HAL", "Direct3D T&L HAL"};
    RE2DJ_CHECK(context, names == expected);
    if (device_caps.size() == 3)
    {
        RE2DJ_CHECK_EQ(context, device_caps[1] & dx::kD3dDevCapsHwTransformAndLight, 0U);
        RE2DJ_CHECK_EQ(context, device_caps[2] & dx::kD3dDevCapsHwTransformAndLight,
                       dx::kD3dDevCapsHwTransformAndLight);
    }

    std::vector<std::uint32_t> depth_bits;
    services.guest_function = [&](const std::vector<std::uint32_t>& arguments) {
        depth_bits.push_back(services.U32(arguments[0] + 12));
        return 1U;
    };
    CallModuleExport(context, services, descriptor, "IDirect3D7::EnumZBufferFormats", {direct3d, 0, 0x00401000U, 0});
    RE2DJ_CHECK(context, depth_bits == std::vector<std::uint32_t>{16});

    // IDirect3D7 answers only itself.
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "IDirect3D7::QueryInterface",
                                             {direct3d, kIid, kOut}).eax,
                   dx::kDdOk);
    for (std::uint32_t index = 0; index < 16; ++index)
    {
        services.Byte(kIid + index) = dx::kIidDirectDraw7[index];
    }
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "IDirect3D7::QueryInterface",
                                             {direct3d, kIid, kOut}).eax,
                   dx::kENoInterface);
    RE2DJ_CHECK_EQ(context, services.U32(kOut), 0U);

    // The DirectDraw object outlives its own last reference while the
    // Direct3D object holds one.
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "IDirectDraw7::Release", {direct_draw}).eax,
                   1U);
    CallModuleExport(context, services, descriptor, "IDirect3D7::Release", {direct3d});
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "IDirect3D7::Release", {direct3d}).eax,
                   0U);
    RE2DJ_CHECK(context, services.Process()->com().Find(direct_draw) == nullptr);
}

// SetCooperativeLevel takes a window the guest created, SetDisplayMode only
// 640x480x16, and GetDisplayMode reports the mode set.
void CheckCooperativeLevelAndMode(re2dj::test::Context& context)
{
    namespace dx = re2dj::directx;
    const auto descriptor = modules::MakeDdrawModuleDescriptor();
    MemoryServices services;
    const std::uint32_t direct_draw = CreateDirectDraw(context, services, descriptor);
    re2dj::hle::GuestWindow window;
    const std::uint32_t hwnd = services.Process()->user().AddWindow(window);

    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "IDirectDraw7::SetCooperativeLevel",
                                             {direct_draw, 0, 0x813}).eax,
                   dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "IDirectDraw7::SetCooperativeLevel",
                                             {direct_draw, 0x7777, 0x813}).eax,
                   dx::kDdErrGeneric);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "IDirectDraw7::SetCooperativeLevel",
                                             {direct_draw, hwnd, 0x813}).eax,
                   dx::kDdOk);

    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "IDirectDraw7::SetDisplayMode",
                                             {direct_draw, 800, 600, 16, 60, 0}).eax,
                   dx::kDdErrUnsupportedMode);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "IDirectDraw7::SetDisplayMode",
                                             {direct_draw, 640, 480, 16, 60, 0}).eax,
                   dx::kDdOk);
    constexpr std::uint32_t kMode = MemoryServices::kArenaBase;
    CallModuleExport(context, services, descriptor, "IDirectDraw7::GetDisplayMode", {direct_draw, kMode});
    RE2DJ_CHECK_EQ(context, services.U32(kMode + 12), 640U);
    RE2DJ_CHECK_EQ(context, services.U32(kMode + 72 + 12), 16U);
}

// Records what the facade asked the host to show, and can refuse.
class FakePresentation final : public re2dj::hle::HostPresentation
{
public:
    bool refuse = false;
    std::vector<std::vector<std::uint32_t>> shown;

    bool ShowGuestWindow(std::uint32_t guest_window,
                         std::uint32_t width,
                         std::uint32_t height,
                         std::string* error) override
    {
        if (refuse)
        {
            *error = "no display";
            return false;
        }
        shown.push_back({guest_window, width, height});
        return true;
    }
    bool retains = false;
    std::vector<std::uint16_t> clears;
    std::vector<re2dj::graphics::LegacyDrawCommand> draws;
    std::vector<bool> textured;
    int presents = 0;
    void SetRetainBetweenFrames(bool retain) override { retains = retain; }
    bool ClearTarget(std::uint16_t rgb565, std::string*) override
    {
        clears.push_back(rgb565);
        return true;
    }
    std::vector<std::uint32_t> true_color_clears;
    bool ClearTargetColor(std::uint32_t xrgb, std::string*) override
    {
        true_color_clears.push_back(xrgb);
        return true;
    }
    // Reads fill each pixel with target_pixel; writes keep their rectangle
    // and first pixel.
    std::uint16_t target_pixel = 0;
    struct TargetCopy
    {
        std::uint32_t x = 0;
        std::uint32_t y = 0;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint32_t pitch = 0;
        std::uint16_t first = 0;
    };
    std::vector<TargetCopy> reads;
    std::vector<TargetCopy> writes;
    bool ReadTarget(std::uint32_t x,
                    std::uint32_t y,
                    std::uint32_t width,
                    std::uint32_t height,
                    std::span<std::uint8_t> pixels,
                    std::uint32_t pitch,
                    std::string*) override
    {
        for (std::uint32_t row = 0; row < height; ++row)
        {
            for (std::uint32_t column = 0; column < width; ++column)
            {
                pixels[row * pitch + column * 2] = static_cast<std::uint8_t>(target_pixel);
                pixels[row * pitch + column * 2 + 1] = static_cast<std::uint8_t>(target_pixel >> 8);
            }
        }
        reads.push_back({x, y, width, height, pitch, target_pixel});
        return true;
    }
    bool WriteTarget(std::uint32_t x,
                     std::uint32_t y,
                     std::uint32_t width,
                     std::uint32_t height,
                     std::span<const std::uint8_t> pixels,
                     std::uint32_t pitch,
                     std::string*) override
    {
        writes.push_back({x, y, width, height, pitch,
                          static_cast<std::uint16_t>(pixels[0] | (pixels[1] << 8))});
        return true;
    }
    bool Draw(const re2dj::graphics::LegacyDrawCommand& command,
              const re2dj::graphics::LegacyFixedFunctionState&,
              std::uint32_t,
              std::uint32_t,
              const re2dj::graphics::LegacyTextureView* texture,
              std::string*) override
    {
        draws.push_back(command);
        textured.push_back(texture != nullptr);
        // The first row of the texture's true-color plane, empty without one.
        std::vector<std::uint32_t> plane_row;
        if (texture != nullptr && texture->true_color != nullptr)
        {
            plane_row.assign(texture->true_color, texture->true_color + texture->width);
        }
        true_color_rows.push_back(plane_row);
        return true;
    }
    std::vector<std::vector<std::uint32_t>> true_color_rows;
    bool Present(std::string*) override
    {
        ++presents;
        return true;
    }
    std::vector<std::uint64_t> discarded;
    void DiscardTexture(std::uint64_t identity) override { discarded.push_back(identity); }
    bool CloseRequested() const override { return false; }
    re2dj::hle::HostInputState input;
    const re2dj::hle::HostInputState& Input() const override { return input; }
};

// SetCooperativeLevel shows the guest window through the host's presentation
// at the display mode's size; a host that cannot show it stops the run.
void CheckHostWindow(re2dj::test::Context& context)
{
    namespace dx = re2dj::directx;
    const auto descriptor = modules::MakeDdrawModuleDescriptor();
    MemoryServices services;
    FakePresentation presentation;
    services.presentation = &presentation;
    const std::uint32_t direct_draw = CreateDirectDraw(context, services, descriptor);
    re2dj::hle::GuestWindow window;
    const std::uint32_t hwnd = services.Process()->user().AddWindow(window);

    // An unknown window never reaches the host.
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "IDirectDraw7::SetCooperativeLevel",
                                             {direct_draw, 0x7777, 0x813}).eax,
                   dx::kDdErrGeneric);
    RE2DJ_CHECK(context, presentation.shown.empty());
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "IDirectDraw7::SetCooperativeLevel",
                                             {direct_draw, hwnd, 0x813}).eax,
                   dx::kDdOk);
    RE2DJ_CHECK_EQ(context, presentation.shown.size(), std::size_t{1});
    if (presentation.shown.size() == 1)
    {
        const std::vector<std::uint32_t> expected = {hwnd, 640, 480};
        RE2DJ_CHECK(context, presentation.shown[0] == expected);
    }

    presentation.refuse = true;
    bool handled = true;
    std::string error;
    CallModuleExport(context, services, descriptor, "IDirectDraw7::SetCooperativeLevel", {direct_draw, hwnd, 0x813},
                     &handled, &error);
    RE2DJ_CHECK(context, !handled);
    RE2DJ_CHECK(context, error.find("no display") != std::string::npos);
}

// CreateSurface makes the 4th's flipping primary with its back buffer, a
// depth surface that attaches, and textures; surfaces describe themselves
// through the shared core and give their memory back when released.
void CheckSurfaces(re2dj::test::Context& context)
{
    namespace dx = re2dj::directx;
    const auto descriptor = modules::MakeDdrawModuleDescriptor();
    MemoryServices services;
    const std::uint32_t direct_draw = CreateDirectDraw(context, services, descriptor);
    const std::size_t blocks_before = services.Process()->live_blocks();

    constexpr std::uint32_t kDesc = MemoryServices::kBase + 0x100;
    constexpr std::uint32_t kOut = MemoryServices::kBase + 0x40;
    const auto put_desc = [&](const dx::DdSurfaceDesc2& desc) {
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(&desc);
        for (std::uint32_t index = 0; index < sizeof(desc); ++index)
        {
            services.Byte(kDesc + index) = bytes[index];
        }
    };
    dx::DdSurfaceDesc2 primary;
    primary.size = sizeof(primary);
    primary.flags = dx::kDdsdCaps | dx::kDdsdBackBufferCount;
    primary.caps.caps = 0x00002218U;
    primary.back_buffer_count = 1;
    put_desc(primary);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "IDirectDraw7::CreateSurface",
                                             {direct_draw, kDesc, kOut, 0}).eax,
                   dx::kDdOk);
    const std::uint32_t front = services.U32(kOut);
    RE2DJ_CHECK(context, front != 0);

    // The back buffer is attached from the start.
    constexpr std::uint32_t kCaps = MemoryServices::kBase + 0x60;
    services.PutU32(kCaps, dx::kDdsCapsBackBuffer);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "IDirectDrawSurface7::GetAttachedSurface",
                                             {front, kCaps, kOut}).eax,
                   dx::kDdOk);
    const std::uint32_t back = services.U32(kOut);
    RE2DJ_CHECK(context, back != 0 && back != front);
    services.PutU32(kCaps, dx::kDdsCapsZBuffer);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "IDirectDrawSurface7::GetAttachedSurface",
                                             {back, kCaps, kOut}).eax,
                   dx::kDdErrNotFound);

    // GetSurfaceDesc reports the back buffer's shape and pitch.
    constexpr std::uint32_t kReport = MemoryServices::kArenaBase + 0x28000;
    services.PutU32(kReport, sizeof(dx::DdSurfaceDesc2));
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "IDirectDrawSurface7::GetSurfaceDesc",
                                             {back, kReport}).eax,
                   dx::kDdOk);
    RE2DJ_CHECK_EQ(context, services.U32(kReport + 12), 640U);
    RE2DJ_CHECK_EQ(context, services.U32(kReport + 16), 1280U);
    RE2DJ_CHECK_EQ(context, services.U32(kReport + 104), dx::kDdsCapsBackBuffer | dx::kDdsCaps3dDevice);
    services.PutU32(kReport, 100);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "IDirectDrawSurface7::GetSurfaceDesc",
                                             {back, kReport}).eax,
                   dx::kDdErrInvalidParams);

    // Lock hands out the back buffer's own pixels, a rectangle at its first
    // pixel; Unlock takes them back. An event is refused.
    constexpr std::uint32_t kLockRect = MemoryServices::kArenaBase + 0x28100;
    services.PutU32(kReport, sizeof(dx::DdSurfaceDesc2));
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "IDirectDrawSurface7::Lock",
                                             {back, 0, kReport, 1, 0}).eax,
                   dx::kDdOk);
    const std::uint32_t pixels = services.U32(kReport + 36);
    RE2DJ_CHECK(context, pixels != 0);
    RE2DJ_CHECK_EQ(context, services.U32(kReport + 4) & dx::kDdsdLpSurface, dx::kDdsdLpSurface);
    services.PutU32(kLockRect, 10);
    services.PutU32(kLockRect + 4, 20);
    services.PutU32(kLockRect + 8, 30);
    services.PutU32(kLockRect + 12, 60);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "IDirectDrawSurface7::Lock",
                                             {back, kLockRect, kReport, 1, 0}).eax,
                   dx::kDdOk);
    RE2DJ_CHECK_EQ(context, services.U32(kReport + 36), pixels + 20 * 1280 + 20);
    RE2DJ_CHECK_EQ(context, services.U32(kReport + 12), 20U);
    RE2DJ_CHECK_EQ(context, services.U32(kReport + 8), 40U);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "IDirectDrawSurface7::Unlock",
                                             {back, 0}).eax,
                   dx::kDdOk);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "IDirectDrawSurface7::Lock",
                                             {back, 0, kReport, 1, 0x1234}).eax,
                   dx::kDdErrInvalidParams);

    // A depth surface attaches to the back buffer; a texture does not.
    dx::DdSurfaceDesc2 depth;
    depth.size = sizeof(depth);
    depth.flags = 0x00001007U;
    depth.caps.caps = 0x00024000U;
    depth.width = 640;
    depth.height = 480;
    depth.pixel_format = dx::Depth16Format();
    put_desc(depth);
    CallModuleExport(context, services, descriptor, "IDirectDraw7::CreateSurface", {direct_draw, kDesc, kOut, 0});
    const std::uint32_t z = services.U32(kOut);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "IDirectDrawSurface7::AddAttachedSurface",
                                             {back, z}).eax,
                   dx::kDdOk);
    services.PutU32(kCaps, dx::kDdsCapsZBuffer);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "IDirectDrawSurface7::GetAttachedSurface",
                                             {back, kCaps, kOut}).eax,
                   dx::kDdOk);
    RE2DJ_CHECK_EQ(context, services.U32(kOut), z);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "IDirectDrawSurface7::AddAttachedSurface",
                                             {back, front}).eax,
                   dx::kDdErrCannotAttachSurface);

    dx::DdSurfaceDesc2 texture;
    texture.size = sizeof(texture);
    texture.flags = 0x00001007U;
    texture.caps.caps = 0x10005000U;
    texture.width = 16;
    texture.height = 16;
    texture.pixel_format = dx::Rgb565Format();
    texture.pixel_format.bit_count = 32;
    put_desc(texture);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "IDirectDraw7::CreateSurface",
                                             {direct_draw, kDesc, kOut, 0}).eax,
                   dx::kDdErrInvalidPixelFormat);
    RE2DJ_CHECK_EQ(context, services.U32(kOut), 0U);

    // Releasing every reference frees the surfaces and their pixel memory:
    // the back buffer (GetAttachedSurface's), the depth surface (the
    // guest's and the lookup's), then the primary, which lets go of both.
    CallModuleExport(context, services, descriptor, "IDirectDrawSurface7::Release", {back});
    CallModuleExport(context, services, descriptor, "IDirectDrawSurface7::Release", {z});
    CallModuleExport(context, services, descriptor, "IDirectDrawSurface7::Release", {z});
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "IDirectDrawSurface7::Release", {front}).eax,
                   0U);
    RE2DJ_CHECK(context, services.Process()->com().Find(back) == nullptr);
    RE2DJ_CHECK(context, services.Process()->com().Find(z) == nullptr);
    // Only IDirectDrawSurface7's vtable stays, like a DLL's static table.
    RE2DJ_CHECK_EQ(context, services.Process()->live_blocks(), blocks_before + 1);
    RE2DJ_CHECK_EQ(context, services.Process()->com().Find(direct_draw)->reference_count, 1U);
}

// CreateDevice renders into a 3D surface of an enumerated class, starts in
// the shared core's state, answers state calls by the core's rules, and holds
// its render target until it is released.
void CheckDevice(re2dj::test::Context& context)
{
    namespace dx = re2dj::directx;
    const auto descriptor = modules::MakeDdrawModuleDescriptor();
    MemoryServices services;
    const std::uint32_t direct_draw = CreateDirectDraw(context, services, descriptor);
    constexpr std::uint32_t kGuid = MemoryServices::kBase + 0x40;
    constexpr std::uint32_t kOut = MemoryServices::kBase + 0x70;
    constexpr std::uint32_t kData = MemoryServices::kBase + 0x100;
    const auto put_guid = [&](const dx::Guid& guid) {
        for (std::uint32_t index = 0; index < 16; ++index)
        {
            services.Byte(kGuid + index) = guid[index];
        }
    };
    const auto call = [&](const char* name, std::initializer_list<std::uint32_t> arguments) {
        return CallModuleExport(context, services, descriptor, name, arguments).eax;
    };

    dx::DdSurfaceDesc2 request;
    request.size = sizeof(request);
    request.flags = dx::kDdsdCaps | dx::kDdsdBackBufferCount;
    request.caps.caps = 0x00002218U;
    request.back_buffer_count = 1;
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&request);
    for (std::uint32_t index = 0; index < sizeof(request); ++index)
    {
        services.Byte(kData + index) = bytes[index];
    }
    RE2DJ_CHECK_EQ(context, call("IDirectDraw7::CreateSurface", {direct_draw, kData, kOut, 0}), dx::kDdOk);
    const std::uint32_t front = services.U32(kOut);
    services.PutU32(kData, dx::kDdsCapsBackBuffer);
    call("IDirectDrawSurface7::GetAttachedSurface", {front, kData, kOut});
    const std::uint32_t back = services.U32(kOut);
    put_guid(dx::kIidDirect3D7);
    call("IDirectDraw7::QueryInterface", {direct_draw, kGuid, kOut});
    const std::uint32_t direct3d = services.U32(kOut);

    // Refusals: no out pointer, no target, a class the enumeration does not
    // report, and a target that is not a surface.
    put_guid(dx::kIidDirect3DHalDevice);
    RE2DJ_CHECK_EQ(context, call("IDirect3D7::CreateDevice", {direct3d, kGuid, back, 0}), dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, call("IDirect3D7::CreateDevice", {direct3d, kGuid, 0, kOut}), dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, call("IDirect3D7::CreateDevice", {direct3d, kGuid, direct3d, kOut}),
                   dx::kDdErrInvalidObject);
    put_guid(dx::kIidDirectDraw7);
    RE2DJ_CHECK_EQ(context, call("IDirect3D7::CreateDevice", {direct3d, kGuid, back, kOut}), dx::kDdErrInvalidObject);
    RE2DJ_CHECK_EQ(context, services.U32(kOut), 0U);

    const std::uint32_t back_references = services.Process()->com().Find(back)->reference_count;
    put_guid(dx::kIidDirect3DHalDevice);
    RE2DJ_CHECK_EQ(context, call("IDirect3D7::CreateDevice", {direct3d, kGuid, back, kOut}), dx::kDdOk);
    const std::uint32_t device = services.U32(kOut);
    RE2DJ_CHECK(context, device != 0);
    RE2DJ_CHECK_EQ(context, services.Process()->com().Find(back)->reference_count, back_references + 1);

    // The core's initial state, and its rules.
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice7::GetRenderState", {device, dx::kD3dRenderStateCullMode, kOut}),
                   dx::kDdOk);
    RE2DJ_CHECK_EQ(context, services.U32(kOut), dx::kD3dCullCcw);
    // A Direct3D 7 device starts lit, with LESSEQUAL depth, as on Windows 11.
    call("IDirect3DDevice7::GetRenderState", {device, dx::kD3dRenderStateLighting, kOut});
    RE2DJ_CHECK_EQ(context, services.U32(kOut), 1U);
    call("IDirect3DDevice7::GetRenderState", {device, dx::kD3dRenderStateZFunc, kOut});
    RE2DJ_CHECK_EQ(context, services.U32(kOut), dx::kD3dCmpLessEqual);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice7::SetRenderState", {device, 137, 1}), dx::kDdOk);
    call("IDirect3DDevice7::GetRenderState", {device, 137, kOut});
    RE2DJ_CHECK_EQ(context, services.U32(kOut), 1U);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice7::SetRenderState", {device, 256, 1}), dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice7::GetRenderState", {device, 137, 0}), dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice7::SetTextureStageState", {device, 0, dx::kD3dTssColorOp, 2}),
                   dx::kDdOk);
    call("IDirect3DDevice7::GetTextureStageState", {device, 0, dx::kD3dTssColorOp, kOut});
    RE2DJ_CHECK_EQ(context, services.U32(kOut), 2U);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice7::SetTextureStageState", {device, 8, 1, 2}),
                   dx::kDdErrInvalidParams);

    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice7::BeginScene", {device}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice7::BeginScene", {device}), dx::kD3dErrSceneInScene);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice7::EndScene", {device}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice7::EndScene", {device}), dx::kD3dErrSceneNotInScene);

    // The viewport: none until set, and never an empty one.
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice7::GetViewport", {device, kData}), dx::kDdErrNotFound);
    dx::D3dViewport7 viewport;
    const auto put_viewport = [&] {
        const auto* view_bytes = reinterpret_cast<const std::uint8_t*>(&viewport);
        for (std::uint32_t index = 0; index < sizeof(viewport); ++index)
        {
            services.Byte(kData + index) = view_bytes[index];
        }
    };
    put_viewport();
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice7::SetViewport", {device, kData}), dx::kDdErrInvalidParams);
    viewport.width = 640;
    viewport.height = 480;
    put_viewport();
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice7::SetViewport", {device, kData}), dx::kDdOk);
    services.PutU32(kData + 8, 0);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice7::GetViewport", {device, kData}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, services.U32(kData + 8), 640U);

    // Transforms start as identity and keep what is set.
    constexpr std::uint32_t kMatrix = MemoryServices::kBase + 0x200;
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice7::GetTransform", {device, dx::kD3dTransformWorld, kMatrix}),
                   dx::kDdOk);
    RE2DJ_CHECK_EQ(context, services.U32(kMatrix), 0x3F800000U);
    RE2DJ_CHECK_EQ(context, services.U32(kMatrix + 4), 0U);
    services.PutU32(kMatrix + 4, 0x40000000U);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice7::SetTransform", {device, dx::kD3dTransformView, kMatrix}),
                   dx::kDdOk);
    services.PutU32(kMatrix + 4, 0);
    call("IDirect3DDevice7::GetTransform", {device, dx::kD3dTransformView, kMatrix});
    RE2DJ_CHECK_EQ(context, services.U32(kMatrix + 4), 0x40000000U);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice7::SetTransform", {device, 32, kMatrix}), dx::kDdErrInvalidParams);

    // The material: zero on a new device, then what was set (task 430).
    constexpr std::uint32_t kMaterial = MemoryServices::kBase + 0x300;
    services.PutU32(kMaterial + 12, 0xCDCDCDCDU);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice7::GetMaterial", {device, kMaterial}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, services.U32(kMaterial + 12), 0U);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice7::GetMaterial", {device, 0}), dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice7::SetMaterial", {device, kMatrix}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice7::GetMaterial", {device, kMaterial}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, services.U32(kMaterial + 4), 0x40000000U);

    // The render target: the same one again changes nothing, and it must be
    // a surface.
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice7::SetRenderTarget", {device, back, 0}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, services.Process()->com().Find(back)->reference_count, back_references + 1);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice7::SetRenderTarget", {device, direct3d, 0}),
                   dx::kDdErrInvalidObject);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice7::GetRenderTarget", {device, kOut}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, services.U32(kOut), back);
    call("IDirectDrawSurface7::Release", {back});

    // Releasing the device lets go of its render target.
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice7::Release", {device}), 0U);
    RE2DJ_CHECK_EQ(context, services.Process()->com().Find(back)->reference_count, back_references);
}

// EnumSurfaces lists the existing surfaces, attached ones included, newest
// first, each AddRef'd for the callback and described as GetSurfaceDesc
// describes it; DDENUMRET_CANCEL stops it, and bad flags are refused.
// RestoreAllSurfaces has nothing lost to restore.
void CheckEnumSurfaces(re2dj::test::Context& context)
{
    namespace dx = re2dj::directx;
    const auto descriptor = modules::MakeDdrawModuleDescriptor();
    MemoryServices services;
    const std::uint32_t direct_draw = CreateDirectDraw(context, services, descriptor);
    const auto call = [&](const char* name, std::initializer_list<std::uint32_t> arguments) {
        return CallModuleExport(context, services, descriptor, name, arguments).eax;
    };
    constexpr std::uint32_t kOut = MemoryServices::kBase + 0x70;
    constexpr std::uint32_t kData = MemoryServices::kBase + 0x100;
    const auto put_desc = [&](const dx::DdSurfaceDesc2& desc) {
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(&desc);
        for (std::uint32_t index = 0; index < sizeof(desc); ++index)
        {
            services.Byte(kData + index) = bytes[index];
        }
    };
    dx::DdSurfaceDesc2 primary;
    primary.size = sizeof(primary);
    primary.flags = dx::kDdsdCaps | dx::kDdsdBackBufferCount;
    primary.caps.caps = 0x00002218U;
    primary.back_buffer_count = 1;
    put_desc(primary);
    call("IDirectDraw7::CreateSurface", {direct_draw, kData, kOut, 0});
    const std::uint32_t front = services.U32(kOut);
    services.PutU32(kData, dx::kDdsCapsBackBuffer);
    call("IDirectDrawSurface7::GetAttachedSurface", {front, kData, kOut});
    const std::uint32_t back = services.U32(kOut);
    call("IDirectDrawSurface7::Release", {back});
    dx::DdSurfaceDesc2 texture_desc;
    texture_desc.size = sizeof(texture_desc);
    texture_desc.flags = 0x00001007U;
    texture_desc.caps.caps = 0x10005000U;
    texture_desc.width = 4;
    texture_desc.height = 2;
    texture_desc.pixel_format = dx::Rgb565Format();
    put_desc(texture_desc);
    call("IDirectDraw7::CreateSurface", {direct_draw, kData, kOut, 0});
    const std::uint32_t texture = services.U32(kOut);
    const auto references = [&](std::uint32_t surface) {
        return services.Process()->com().Find(surface)->reference_count;
    };
    const std::uint32_t front_references = references(front);
    const std::uint32_t back_references = references(back);
    const std::uint32_t texture_references = references(texture);

    struct Seen
    {
        std::uint32_t surface;
        std::uint32_t description_size;
        std::uint32_t width;
        std::uint32_t pitch;
        std::uint32_t context;
    };
    std::vector<Seen> seen;
    std::uint32_t answer = 1;
    services.guest_function = [&](const std::vector<std::uint32_t>& arguments) {
        seen.push_back({arguments[0], services.U32(arguments[1]), services.U32(arguments[1] + 12),
                        services.U32(arguments[1] + 16), arguments[2]});
        return answer;
    };
    constexpr std::uint32_t kCallback = 0x00401F9BU;
    RE2DJ_CHECK_EQ(context,
                   call("IDirectDraw7::EnumSurfaces",
                        {direct_draw, dx::kDdEnumSurfacesAll | dx::kDdEnumSurfacesDoesExist, 0, 0x5150, kCallback}),
                   dx::kDdOk);
    RE2DJ_CHECK_EQ(context, seen.size(), std::size_t{3});
    if (seen.size() == 3)
    {
        RE2DJ_CHECK_EQ(context, seen[0].surface, texture);
        RE2DJ_CHECK_EQ(context, seen[1].surface, back);
        RE2DJ_CHECK_EQ(context, seen[2].surface, front);
        RE2DJ_CHECK_EQ(context, seen[0].description_size, static_cast<std::uint32_t>(sizeof(dx::DdSurfaceDesc2)));
        RE2DJ_CHECK_EQ(context, seen[0].width, 4U);
        RE2DJ_CHECK_EQ(context, seen[0].pitch, 8U);
        RE2DJ_CHECK_EQ(context, seen[2].width, 640U);
        RE2DJ_CHECK_EQ(context, seen[2].context, 0x5150U);
    }
    // The callback owns the reference each surface was handed with.
    RE2DJ_CHECK_EQ(context, references(front), front_references + 1);
    RE2DJ_CHECK_EQ(context, references(back), back_references + 1);
    RE2DJ_CHECK_EQ(context, references(texture), texture_references + 1);

    // DDENUMRET_CANCEL stops after the first; the result is still DD_OK.
    seen.clear();
    answer = dx::kEnumCancel;
    RE2DJ_CHECK_EQ(context,
                   call("IDirectDraw7::EnumSurfaces",
                        {direct_draw, dx::kDdEnumSurfacesAll | dx::kDdEnumSurfacesDoesExist, 0, 0, kCallback}),
                   dx::kDdOk);
    RE2DJ_CHECK_EQ(context, seen.size(), std::size_t{1});
    call("IDirectDrawSurface7::Release", {texture});

    // Refusals call nothing.
    seen.clear();
    answer = 1;
    const std::initializer_list<std::uint32_t> refused_flags = {
        dx::kDdEnumSurfacesDoesExist, dx::kDdEnumSurfacesAll, 0,
        dx::kDdEnumSurfacesMatch | dx::kDdEnumSurfacesDoesExist,
        dx::kDdEnumSurfacesAll | dx::kDdEnumSurfacesMatch | dx::kDdEnumSurfacesDoesExist,
        dx::kDdEnumSurfacesAll | dx::kDdEnumSurfacesCanBeCreated,
        dx::kDdEnumSurfacesAll | dx::kDdEnumSurfacesDoesExist | 0x100U};
    for (const std::uint32_t flags : refused_flags)
    {
        RE2DJ_CHECK_EQ(context, call("IDirectDraw7::EnumSurfaces", {direct_draw, flags, 0, 0, kCallback}),
                       dx::kDdErrInvalidParams);
    }
    RE2DJ_CHECK_EQ(context,
                   call("IDirectDraw7::EnumSurfaces",
                        {direct_draw, dx::kDdEnumSurfacesAll | dx::kDdEnumSurfacesDoesExist, 0, 0, 0}),
                   dx::kDdErrInvalidParams);
    RE2DJ_CHECK(context, seen.empty());

    // A matching search is not modelled and stops the run.
    bool handled = true;
    std::string error;
    CallModuleExport(context, services, descriptor, "IDirectDraw7::EnumSurfaces",
                     {direct_draw, dx::kDdEnumSurfacesMatch | dx::kDdEnumSurfacesDoesExist, kData, 0, kCallback},
                     &handled, &error);
    RE2DJ_CHECK(context, !handled);

    // A surface the guest has let go of is no longer listed.
    while (call("IDirectDrawSurface7::Release", {texture}) != 0)
    {
    }
    RE2DJ_CHECK(context, services.Process()->com().Find(texture) == nullptr);
    seen.clear();
    call("IDirectDraw7::EnumSurfaces",
         {direct_draw, dx::kDdEnumSurfacesAll | dx::kDdEnumSurfacesDoesExist, 0, 0, kCallback});
    RE2DJ_CHECK_EQ(context, seen.size(), std::size_t{2});

    RE2DJ_CHECK_EQ(context, call("IDirectDraw7::RestoreAllSurfaces", {direct_draw}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, call("IDirectDraw7::RestoreDisplayMode", {direct_draw}), dx::kDdOk);
}

// Drawing: a whole-target clear of the presented surface fills its pixels
// and clears the host; SetTexture holds a stage 0 texture; DrawPrimitive
// hands the decoded draw to the host with that texture; Flip presents; a
// released texture's host copy is discarded.
void CheckDrawing(re2dj::test::Context& context)
{
    namespace dx = re2dj::directx;
    const auto descriptor = modules::MakeDdrawModuleDescriptor();
    // Declared first: surfaces hand their textures back to it when the
    // process goes away.
    FakePresentation presentation;
    MemoryServices services;
    services.presentation = &presentation;
    const std::uint32_t direct_draw = CreateDirectDraw(context, services, descriptor);
    const auto call = [&](const char* name, std::initializer_list<std::uint32_t> arguments) {
        return CallModuleExport(context, services, descriptor, name, arguments).eax;
    };
    constexpr std::uint32_t kGuid = MemoryServices::kBase + 0x40;
    constexpr std::uint32_t kOut = MemoryServices::kBase + 0x70;
    constexpr std::uint32_t kData = MemoryServices::kBase + 0x100;
    const auto put_desc = [&](const dx::DdSurfaceDesc2& desc) {
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(&desc);
        for (std::uint32_t index = 0; index < sizeof(desc); ++index)
        {
            services.Byte(kData + index) = bytes[index];
        }
    };
    const auto put_guid = [&](const dx::Guid& guid) {
        for (std::uint32_t index = 0; index < 16; ++index)
        {
            services.Byte(kGuid + index) = guid[index];
        }
    };
    dx::DdSurfaceDesc2 primary;
    primary.size = sizeof(primary);
    primary.flags = dx::kDdsdCaps | dx::kDdsdBackBufferCount;
    primary.caps.caps = 0x00002218U;
    primary.back_buffer_count = 1;
    put_desc(primary);
    RE2DJ_CHECK_EQ(context, call("IDirectDraw7::CreateSurface", {direct_draw, kData, kOut, 0}), dx::kDdOk);
    const std::uint32_t front = services.U32(kOut);
    RE2DJ_CHECK(context, presentation.retains);
    services.PutU32(kData, dx::kDdsCapsBackBuffer);
    call("IDirectDrawSurface7::GetAttachedSurface", {front, kData, kOut});
    const std::uint32_t back = services.U32(kOut);
    dx::DdSurfaceDesc2 texture_desc;
    texture_desc.size = sizeof(texture_desc);
    texture_desc.flags = 0x00001007U;
    texture_desc.caps.caps = 0x10005000U;
    texture_desc.width = 4;
    texture_desc.height = 2;
    texture_desc.pixel_format = dx::Rgb565Format();
    put_desc(texture_desc);
    call("IDirectDraw7::CreateSurface", {direct_draw, kData, kOut, 0});
    const std::uint32_t texture = services.U32(kOut);
    put_guid(dx::kIidDirect3D7);
    call("IDirectDraw7::QueryInterface", {direct_draw, kGuid, kOut});
    const std::uint32_t direct3d = services.U32(kOut);
    put_guid(dx::kIidDirect3DHalDevice);
    RE2DJ_CHECK_EQ(context, call("IDirect3D7::CreateDevice", {direct3d, kGuid, back, kOut}), dx::kDdOk);
    const std::uint32_t device = services.U32(kOut);

    // A partial clear changes nothing drawn; a whole-target clear fills the
    // back buffer with the RGB565 color and clears the host to it.
    RE2DJ_CHECK_EQ(context,
                   call("IDirect3DDevice7::Clear", {device, 1, kData, dx::kD3dClearTarget, 0x00FF0000U, 0, 0}),
                   dx::kDdOk);
    RE2DJ_CHECK(context, presentation.clears.empty());
    RE2DJ_CHECK_EQ(context,
                   call("IDirect3DDevice7::Clear",
                        {device, 0, 0, dx::kD3dClearTarget | dx::kD3dClearZBuffer, 0x00FF0000U, 0x3F800000U, 0}),
                   dx::kDdOk);
    RE2DJ_CHECK(context, presentation.clears == std::vector<std::uint16_t>{0xF800});
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface7::GetDC", {back, kOut}), dx::kDdOk);
    const std::uint32_t back_dc = services.U32(kOut);
    const re2dj::hle::GuestDc* guest_dc = services.Process()->gdi().FindDc(back_dc);
    RE2DJ_CHECK(context, guest_dc != nullptr);
    if (guest_dc != nullptr)
    {
        const std::uint32_t pixels = services.Process()->gdi().FindBitmap(guest_dc->bitmap)->bits;
        RE2DJ_CHECK_EQ(context, services.U32(pixels), 0xF800F800U);
        RE2DJ_CHECK_EQ(context, services.U32(pixels + 1280 * 480 - 4), 0xF800F800U);
    }
    call("IDirectDrawSurface7::ReleaseDC", {back, back_dc});

    // Locking the surface the guest presents from reads the host's target
    // into the locked area; unlocking puts that area back. Other surfaces
    // are their own pixels only.
    constexpr std::uint32_t kLockDesc = MemoryServices::kBase + 0x3000;
    constexpr std::uint32_t kLockRect = MemoryServices::kBase + 0x3100;
    presentation.target_pixel = 0x1234;
    services.PutU32(kLockDesc, sizeof(dx::DdSurfaceDesc2));
    services.PutU32(kLockRect, 8);
    services.PutU32(kLockRect + 4, 4);
    services.PutU32(kLockRect + 8, 24);
    services.PutU32(kLockRect + 12, 10);
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface7::Lock", {back, kLockRect, kLockDesc, 1, 0}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, presentation.reads.size(), std::size_t{1});
    if (presentation.reads.size() == 1)
    {
        RE2DJ_CHECK_EQ(context, presentation.reads[0].x, 8U);
        RE2DJ_CHECK_EQ(context, presentation.reads[0].y, 4U);
        RE2DJ_CHECK_EQ(context, presentation.reads[0].width, 16U);
        RE2DJ_CHECK_EQ(context, presentation.reads[0].height, 6U);
    }
    const std::uint32_t locked = services.U32(kLockDesc + 36);
    RE2DJ_CHECK_EQ(context, services.U32(locked), 0x12341234U);
    RE2DJ_CHECK_EQ(context, services.U32(locked + 5 * 1280 + 28), 0x12341234U);
    // Outside the locked area the pixels stay the clear's.
    RE2DJ_CHECK_EQ(context, services.U32(locked + 32), 0xF800F800U);
    services.PutU32(locked, 0xABCDABCDU);
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface7::Unlock", {back, kLockRect}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, presentation.writes.size(), std::size_t{1});
    if (presentation.writes.size() == 1)
    {
        RE2DJ_CHECK_EQ(context, presentation.writes[0].x, 8U);
        RE2DJ_CHECK_EQ(context, presentation.writes[0].width, 16U);
        RE2DJ_CHECK_EQ(context, presentation.writes[0].height, 6U);
        RE2DJ_CHECK_EQ(context, presentation.writes[0].first, std::uint16_t{0xABCD});
    }
    // An unlock without a lock puts nothing back; a texture is not the target.
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface7::Unlock", {back, 0}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface7::Lock", {texture, 0, kLockDesc, 1, 0}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface7::Unlock", {texture, 0}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, presentation.reads.size(), std::size_t{1});
    RE2DJ_CHECK_EQ(context, presentation.writes.size(), std::size_t{1});

    // SetTexture: stage 0 only, and only a texture.
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice7::SetTexture", {device, 1, texture}), dx::kDdErrUnsupported);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice7::SetTexture", {device, 0, back}), dx::kDdErrInvalidObject);
    const std::uint32_t texture_references = services.Process()->com().Find(texture)->reference_count;
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice7::SetTexture", {device, 0, texture}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, services.Process()->com().Find(texture)->reference_count, texture_references + 1);

    // A transformed, lit triangle strip reaches the host with the texture.
    constexpr std::uint32_t kVertices = MemoryServices::kBase + 0x400;
    const auto put_float = [&](std::uint32_t address, float value) {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        services.PutU32(address, bits);
    };
    const float corners[4][2] = {{0.0f, 480.0f}, {0.0f, 0.0f}, {640.0f, 480.0f}, {640.0f, 0.0f}};
    for (std::uint32_t index = 0; index < 4; ++index)
    {
        const std::uint32_t vertex = kVertices + index * 32;
        put_float(vertex, corners[index][0]);
        put_float(vertex + 4, corners[index][1]);
        put_float(vertex + 8, 0.5f);
        put_float(vertex + 12, 1.0f);
        services.PutU32(vertex + 16, 0xFFFFFFFFU);
        services.PutU32(vertex + 20, 0);
        put_float(vertex + 24, corners[index][0] / 640.0f);
        put_float(vertex + 28, corners[index][1] / 480.0f);
    }
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice7::BeginScene", {device}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context,
                   call("IDirect3DDevice7::DrawPrimitive",
                        {device, dx::kD3dPtTriangleStrip, dx::kD3dFvfTlVertex, kVertices, 4, 0}),
                   dx::kDdOk);
    RE2DJ_CHECK_EQ(context, presentation.draws.size(), std::size_t{1});
    if (presentation.draws.size() == 1)
    {
        RE2DJ_CHECK_EQ(context, presentation.draws[0].vertices.size(), std::size_t{4});
        RE2DJ_CHECK(context, presentation.textured[0]);
    }
    // Outside the plan: a point list, and no vertices.
    RE2DJ_CHECK_EQ(context,
                   call("IDirect3DDevice7::DrawPrimitive", {device, 1, dx::kD3dFvfTlVertex, kVertices, 4, 0}),
                   dx::kDdErrUnsupported);
    RE2DJ_CHECK_EQ(context,
                   call("IDirect3DDevice7::DrawPrimitive",
                        {device, dx::kD3dPtTriangleStrip, dx::kD3dFvfTlVertex, 0, 4, 0}),
                   dx::kDdErrUnsupported);
    RE2DJ_CHECK_EQ(context, presentation.draws.size(), std::size_t{1});
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice7::EndScene", {device}), dx::kDdOk);

    // Flip presents from the primary; the back buffer does not flip.
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface7::Flip", {back, 0, 1}), dx::kDdErrNotFlippable);
    RE2DJ_CHECK_EQ(context, presentation.presents, 0);
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface7::Flip", {front, 0, 1}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, presentation.presents, 1);

    // The texture was drawn, so letting go of it discards the host's copy.
    call("IDirect3DDevice7::SetTexture", {device, 0, 0});
    RE2DJ_CHECK_EQ(context, services.Process()->com().Find(texture)->reference_count, texture_references);
    RE2DJ_CHECK(context, presentation.discarded.empty());
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface7::Release", {texture}), 0U);
    RE2DJ_CHECK_EQ(context, presentation.discarded.size(), std::size_t{1});
}

// Vertex buffers: CreateVertexBuffer under the core's rules, a lock handing
// the guest the whole buffer's memory, and DrawPrimitiveVB and
// DrawIndexedPrimitiveVB drawing what the guest wrote as DrawPrimitive would.
void CheckVertexBuffers(re2dj::test::Context& context)
{
    namespace dx = re2dj::directx;
    const auto descriptor = modules::MakeDdrawModuleDescriptor();
    FakePresentation presentation;
    MemoryServices services;
    services.presentation = &presentation;
    const std::uint32_t direct_draw = CreateDirectDraw(context, services, descriptor);
    const auto call = [&](const char* name, std::initializer_list<std::uint32_t> arguments) {
        return CallModuleExport(context, services, descriptor, name, arguments).eax;
    };
    constexpr std::uint32_t kGuid = MemoryServices::kBase + 0x40;
    constexpr std::uint32_t kOut = MemoryServices::kBase + 0x70;
    constexpr std::uint32_t kSize = MemoryServices::kBase + 0x74;
    constexpr std::uint32_t kData = MemoryServices::kBase + 0x100;
    constexpr std::uint32_t kIndices = MemoryServices::kBase + 0x300;
    const auto put_guid = [&](const dx::Guid& guid) {
        for (std::uint32_t index = 0; index < 16; ++index)
        {
            services.Byte(kGuid + index) = guid[index];
        }
    };
    dx::DdSurfaceDesc2 primary;
    primary.size = sizeof(primary);
    primary.flags = dx::kDdsdCaps | dx::kDdsdBackBufferCount;
    primary.caps.caps = 0x00002218U;
    primary.back_buffer_count = 1;
    const auto* primary_bytes = reinterpret_cast<const std::uint8_t*>(&primary);
    for (std::uint32_t index = 0; index < sizeof(primary); ++index)
    {
        services.Byte(kData + index) = primary_bytes[index];
    }
    call("IDirectDraw7::CreateSurface", {direct_draw, kData, kOut, 0});
    const std::uint32_t front = services.U32(kOut);
    services.PutU32(kData, dx::kDdsCapsBackBuffer);
    call("IDirectDrawSurface7::GetAttachedSurface", {front, kData, kOut});
    const std::uint32_t back = services.U32(kOut);
    put_guid(dx::kIidDirect3D7);
    call("IDirectDraw7::QueryInterface", {direct_draw, kGuid, kOut});
    const std::uint32_t direct3d = services.U32(kOut);
    put_guid(dx::kIidDirect3DHalDevice);
    call("IDirect3D7::CreateDevice", {direct3d, kGuid, back, kOut});
    const std::uint32_t device = services.U32(kOut);

    // Creation: the core's refusals, then a buffer of four TL vertices.
    const auto put_desc = [&](std::uint32_t size, std::uint32_t fvf, std::uint32_t count) {
        services.PutU32(kData, size);
        services.PutU32(kData + 4, 0);
        services.PutU32(kData + 8, fvf);
        services.PutU32(kData + 12, count);
    };
    put_desc(16, dx::kD3dFvfTlVertex, 4);
    RE2DJ_CHECK_EQ(context, call("IDirect3D7::CreateVertexBuffer", {direct3d, kData, 0, 0}), dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, call("IDirect3D7::CreateVertexBuffer", {direct3d, 0, kOut, 0}), dx::kDdErrInvalidParams);
    put_desc(12, dx::kD3dFvfTlVertex, 4);
    RE2DJ_CHECK_EQ(context, call("IDirect3D7::CreateVertexBuffer", {direct3d, kData, kOut, 0}),
                   dx::kDdErrInvalidParams);
    put_desc(16, 0, 4);
    RE2DJ_CHECK_EQ(context, call("IDirect3D7::CreateVertexBuffer", {direct3d, kData, kOut, 0}),
                   dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, services.U32(kOut), 0U);
    const std::uint32_t direct3d_references = services.Process()->com().Find(direct3d)->reference_count;
    put_desc(20, dx::kD3dFvfTlVertex, 4);
    RE2DJ_CHECK_EQ(context, call("IDirect3D7::CreateVertexBuffer", {direct3d, kData, kOut, 0}), dx::kDdOk);
    const std::uint32_t buffer = services.U32(kOut);
    RE2DJ_CHECK(context, buffer != 0);
    RE2DJ_CHECK_EQ(context, services.Process()->com().Find(direct3d)->reference_count, direct3d_references + 1);

    // Its description, with dwSize the structure's, and its interfaces.
    services.PutU32(kData, 12);
    RE2DJ_CHECK_EQ(context, call("IDirect3DVertexBuffer7::GetVertexBufferDesc", {buffer, kData}),
                   dx::kDdErrInvalidParams);
    services.PutU32(kData, 16);
    RE2DJ_CHECK_EQ(context, call("IDirect3DVertexBuffer7::GetVertexBufferDesc", {buffer, kData}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, services.U32(kData), 16U);
    RE2DJ_CHECK_EQ(context, services.U32(kData + 8), dx::kD3dFvfTlVertex);
    RE2DJ_CHECK_EQ(context, services.U32(kData + 12), 4U);
    put_guid(dx::kIidDirect3DVertexBuffer);
    RE2DJ_CHECK_EQ(context, call("IDirect3DVertexBuffer7::QueryInterface", {buffer, kGuid, kOut}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, services.U32(kOut), buffer);
    call("IDirect3DVertexBuffer7::Release", {buffer});
    put_guid(dx::kIidDirect3D7);
    RE2DJ_CHECK_EQ(context, call("IDirect3DVertexBuffer7::QueryInterface", {buffer, kGuid, kOut}), dx::kENoInterface);
    RE2DJ_CHECK_EQ(context, call("IDirect3DVertexBuffer7::Optimize", {buffer, device, 0}), dx::kDdErrUnsupported);

    // A lock hands out the whole buffer once; a draw of a locked buffer waits.
    RE2DJ_CHECK_EQ(context, call("IDirect3DVertexBuffer7::Lock", {buffer, 0, 0, kSize}), dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, call("IDirect3DVertexBuffer7::Lock", {buffer, 0, kOut, kSize}), dx::kDdOk);
    const std::uint32_t vertices = services.U32(kOut);
    RE2DJ_CHECK(context, vertices != 0);
    RE2DJ_CHECK_EQ(context, services.U32(kSize), 128U);
    RE2DJ_CHECK_EQ(context, call("IDirect3DVertexBuffer7::Lock", {buffer, 0, kOut, kSize}),
                   dx::kD3dErrVertexBufferLocked);
    RE2DJ_CHECK_EQ(context, services.U32(kOut), 0U);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice7::DrawPrimitiveVB", {device, dx::kD3dPtTriangleStrip, buffer, 0, 4, 0}),
                   dx::kD3dErrVertexBufferLocked);
    const float corners[4][2] = {{0.0f, 480.0f}, {0.0f, 0.0f}, {640.0f, 480.0f}, {640.0f, 0.0f}};
    for (std::uint32_t index = 0; index < 4; ++index)
    {
        const std::uint32_t vertex = vertices + index * 32;
        for (std::uint32_t axis = 0; axis < 2; ++axis)
        {
            std::uint32_t bits = 0;
            std::memcpy(&bits, &corners[index][axis], sizeof(bits));
            services.PutU32(vertex + axis * 4, bits);
        }
        services.PutU32(vertex + 8, 0x3F000000U);
        services.PutU32(vertex + 12, 0x3F800000U);
        services.PutU32(vertex + 16, 0xFFFFFFFFU);
    }
    RE2DJ_CHECK_EQ(context, call("IDirect3DVertexBuffer7::Unlock", {buffer}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, call("IDirect3DVertexBuffer7::Unlock", {buffer}), dx::kDdErrNotLocked);

    // DrawPrimitiveVB: the buffer's vertices, and the core's refusals.
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice7::DrawPrimitiveVB", {device, dx::kD3dPtTriangleStrip, buffer, 0, 4, 0}),
                   dx::kDdOk);
    RE2DJ_CHECK_EQ(context, presentation.draws.size(), std::size_t{1});
    if (presentation.draws.size() == 1)
    {
        RE2DJ_CHECK_EQ(context, presentation.draws[0].vertices.size(), std::size_t{4});
        RE2DJ_CHECK_EQ(context, presentation.draws[0].vertices[2].x, 640.0f);
    }
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice7::DrawPrimitiveVB", {device, dx::kD3dPtTriangleStrip, buffer, 1, 4, 0}),
                   dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice7::DrawPrimitiveVB", {device, dx::kD3dPtTriangleStrip, buffer, 0, 0, 0}),
                   dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, call("IDirect3DDevice7::DrawPrimitiveVB", {device, dx::kD3dPtTriangleStrip, device, 0, 4, 0}),
                   dx::kDdErrInvalidParams);

    // DrawIndexedPrimitiveVB: two triangles from four vertices.
    const std::uint16_t indices[] = {0, 1, 2, 2, 1, 3};
    for (std::uint32_t index = 0; index < 6; ++index)
    {
        services.Byte(kIndices + index * 2) = static_cast<std::uint8_t>(indices[index]);
        services.Byte(kIndices + index * 2 + 1) = 0;
    }
    RE2DJ_CHECK_EQ(context,
                   call("IDirect3DDevice7::DrawIndexedPrimitiveVB",
                        {device, dx::kD3dPtTriangleList, buffer, 0, 4, kIndices, 6, 0}),
                   dx::kDdOk);
    RE2DJ_CHECK_EQ(context, presentation.draws.size(), std::size_t{2});
    if (presentation.draws.size() == 2)
    {
        RE2DJ_CHECK_EQ(context, presentation.draws[1].vertices.size(), std::size_t{6});
        RE2DJ_CHECK_EQ(context, presentation.draws[1].vertices[5].x, 640.0f);
    }
    RE2DJ_CHECK_EQ(context,
                   call("IDirect3DDevice7::DrawIndexedPrimitiveVB",
                        {device, dx::kD3dPtTriangleList, buffer, 1, 4, kIndices, 6, 0}),
                   dx::kDdErrUnsupported);
    RE2DJ_CHECK_EQ(context,
                   call("IDirect3DDevice7::DrawIndexedPrimitiveVB",
                        {device, dx::kD3dPtTriangleList, buffer, 0, 4, 0, 6, 0}),
                   dx::kDdErrInvalidParams);
    services.Byte(kIndices + 10) = 9;
    RE2DJ_CHECK_EQ(context,
                   call("IDirect3DDevice7::DrawIndexedPrimitiveVB",
                        {device, dx::kD3dPtTriangleList, buffer, 0, 4, kIndices, 6, 0}),
                   dx::kDdErrInvalidParams);

    // Releasing the buffer lets go of its Direct3D object.
    RE2DJ_CHECK_EQ(context, call("IDirect3DVertexBuffer7::Release", {buffer}), 0U);
    RE2DJ_CHECK_EQ(context, services.Process()->com().Find(direct3d)->reference_count, direct3d_references);
}

// GDI drawing into a surface's DC as Windows 11 draws into a 5-6-5 DIB: a
// solid brush filling an ordered, clipped rectangle with its color narrowed
// channel by channel; the text color and background mode kept and answered;
// DrawTextA's offsets and Unifont glyphs; and DeleteObject.
void CheckSurfaceGdiDrawing(re2dj::test::Context& context)
{
    namespace dx = re2dj::directx;
    const auto descriptor = modules::MakeDdrawModuleDescriptor();
    const auto gdi32 = modules::MakeGdi32ModuleDescriptor();
    const auto user32 = modules::MakeUser32ModuleDescriptor();
    MemoryServices services;
    const std::uint32_t direct_draw = CreateDirectDraw(context, services, descriptor);
    const auto call = [&](const auto& module, const char* name, std::initializer_list<std::uint32_t> arguments) {
        return CallModuleExport(context, services, module, name, arguments).eax;
    };
    constexpr std::uint32_t kDesc = MemoryServices::kBase + 0x100;
    constexpr std::uint32_t kOut = MemoryServices::kBase + 0x40;
    constexpr std::uint32_t kRect = MemoryServices::kBase + 0x300;
    constexpr std::uint32_t kText = MemoryServices::kBase + 0x320;
    dx::DdSurfaceDesc2 request;
    request.size = sizeof(request);
    request.flags = 0x00001007U;
    request.caps.caps = 0x10005000U;
    request.width = 8;
    request.height = 4;
    request.pixel_format = dx::Rgb565Format();
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&request);
    for (std::uint32_t index = 0; index < sizeof(request); ++index)
    {
        services.Byte(kDesc + index) = bytes[index];
    }
    call(descriptor, "IDirectDraw7::CreateSurface", {direct_draw, kDesc, kOut, 0});
    const std::uint32_t texture = services.U32(kOut);
    call(descriptor, "IDirectDrawSurface7::GetDC", {texture, kOut});
    const std::uint32_t dc = services.U32(kOut);
    const re2dj::hle::GuestDc* guest_dc = services.Process()->gdi().FindDc(dc);
    RE2DJ_CHECK(context, guest_dc != nullptr);
    if (guest_dc == nullptr)
    {
        return;
    }
    const std::uint32_t pixels = services.Process()->gdi().FindBitmap(guest_dc->bitmap)->bits;
    const auto pixel = [&](std::uint32_t x, std::uint32_t y) {
        return static_cast<std::uint16_t>(services.Byte(pixels + y * 16 + x * 2) |
                                          (services.Byte(pixels + y * 16 + x * 2 + 1) << 8));
    };
    const auto put_rect = [&](std::int32_t left, std::int32_t top, std::int32_t right, std::int32_t bottom) {
        services.PutU32(kRect, static_cast<std::uint32_t>(left));
        services.PutU32(kRect + 4, static_cast<std::uint32_t>(top));
        services.PutU32(kRect + 8, static_cast<std::uint32_t>(right));
        services.PutU32(kRect + 12, static_cast<std::uint32_t>(bottom));
    };

    // A brush's color narrowed as measured: 0x7F blue is 0x000F, and 0x84
    // grey is 0x8430. The rectangle is ordered and clipped.
    services.SetLastError(1234);
    const std::uint32_t blue = call(gdi32, "CreateSolidBrush", {0x007F0000U});
    RE2DJ_CHECK(context, blue != 0);
    put_rect(10, 10, -2, -1);
    RE2DJ_CHECK_EQ(context, call(user32, "FillRect", {dc, kRect, blue}), 1U);
    RE2DJ_CHECK_EQ(context, pixel(0, 0), std::uint16_t{0x000F});
    RE2DJ_CHECK_EQ(context, pixel(7, 3), std::uint16_t{0x000F});
    const std::uint32_t grey = call(gdi32, "CreateSolidBrush", {0x00848484U});
    put_rect(1, 1, 3, 3);
    call(user32, "FillRect", {dc, kRect, grey});
    RE2DJ_CHECK_EQ(context, pixel(1, 1), std::uint16_t{0x8430});
    RE2DJ_CHECK_EQ(context, pixel(2, 2), std::uint16_t{0x8430});
    RE2DJ_CHECK_EQ(context, pixel(3, 3), std::uint16_t{0x000F});
    RE2DJ_CHECK_EQ(context, services.LastError(), 1234U);
    // An empty rectangle, and a handle that is no brush, fill nothing.
    put_rect(2, 1, 2, 3);
    RE2DJ_CHECK_EQ(context, call(user32, "FillRect", {dc, kRect, blue}), 1U);
    RE2DJ_CHECK_EQ(context, pixel(2, 1), std::uint16_t{0x8430});
    put_rect(0, 0, 8, 4);
    RE2DJ_CHECK_EQ(context, call(user32, "FillRect", {dc, kRect, 0x12345678U}), 1U);
    RE2DJ_CHECK_EQ(context, pixel(1, 1), std::uint16_t{0x8430});
    // A DC that is none.
    RE2DJ_CHECK_EQ(context, call(user32, "FillRect", {0x12345678U, kRect, blue}), 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), re2dj::hle::kWin32ErrorInvalidHandle);
    // Not modelled: a system color brush and a null rectangle.
    bool handled = true;
    CallModuleExport(context, services, user32, "FillRect", {dc, kRect, 6}, &handled);
    RE2DJ_CHECK(context, !handled);
    handled = true;
    CallModuleExport(context, services, user32, "FillRect", {dc, 0, blue}, &handled);
    RE2DJ_CHECK(context, !handled);

    // DeleteObject: brushes go, stock objects stay, unknown handles fail.
    services.SetLastError(1234);
    RE2DJ_CHECK_EQ(context, call(gdi32, "DeleteObject", {blue}), 1U);
    RE2DJ_CHECK_EQ(context, call(gdi32, "DeleteObject", {blue}), 0U);
    const std::uint32_t black = call(gdi32, "GetStockObject", {4});
    RE2DJ_CHECK_EQ(context, call(gdi32, "DeleteObject", {black}), 1U);
    RE2DJ_CHECK_EQ(context, call(gdi32, "DeleteObject", {0}), 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), 1234U);
    // A stock brush fills with its color.
    put_rect(0, 0, 1, 1);
    RE2DJ_CHECK_EQ(context, call(user32, "FillRect", {dc, kRect, black}), 1U);
    RE2DJ_CHECK_EQ(context, pixel(0, 0), std::uint16_t{0x0000});

    // Text color and background mode: the previous value back.
    RE2DJ_CHECK_EQ(context, call(gdi32, "SetTextColor", {dc, 0x00FFFFFFU}), 0U);
    RE2DJ_CHECK_EQ(context, call(gdi32, "SetTextColor", {dc, 0x01000003U}), 0x00FFFFFFU);
    RE2DJ_CHECK_EQ(context, call(gdi32, "SetBkMode", {dc, 1}), 2U);
    RE2DJ_CHECK_EQ(context, call(gdi32, "SetBkMode", {dc, 3}), 1U);
    RE2DJ_CHECK_EQ(context, call(gdi32, "SetTextColor", {0x12345678U, 0}), 0xFFFFFFFFU);
    RE2DJ_CHECK_EQ(context, services.LastError(), re2dj::hle::kWin32ErrorInvalidHandle);
    services.SetLastError(1234);
    RE2DJ_CHECK_EQ(context, call(gdi32, "SetBkMode", {0x12345678U, 1}), 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), re2dj::hle::kWin32ErrorInvalidHandle);
    // Background color: white at first, every value kept as given, and the
    // last error untouched; a handle that is no DC is CLR_INVALID with 6.
    services.SetLastError(1234);
    RE2DJ_CHECK_EQ(context, call(gdi32, "SetBkColor", {dc, 0x00123456U}), 0x00FFFFFFU);
    RE2DJ_CHECK_EQ(context, call(gdi32, "SetBkColor", {dc, 0x01000003U}), 0x00123456U);
    RE2DJ_CHECK_EQ(context, call(gdi32, "SetBkColor", {dc, 0xFFFFFFFFU}), 0x01000003U);
    RE2DJ_CHECK_EQ(context, call(gdi32, "SetBkColor", {dc, 0x00FFFFFFU}), 0xFFFFFFFFU);
    RE2DJ_CHECK_EQ(context, services.LastError(), 1234U);
    RE2DJ_CHECK_EQ(context, call(gdi32, "SetBkColor", {0x12345678U, 0}), 0xFFFFFFFFU);
    RE2DJ_CHECK_EQ(context, services.LastError(), re2dj::hle::kWin32ErrorInvalidHandle);
    RE2DJ_CHECK_EQ(context, call(gdi32, "SetBkColor", {0, 0}), 0xFFFFFFFFU);

    // DrawTextA's offsets for one line in a 64-pixel-high rectangle.
    RE2DJ_CHECK_EQ(context, call(gdi32, "SetTextColor", {dc, 0x000000FFU}), 0x01000003U);
    services.Put(kText, "temp");
    put_rect(0, 0, 128, 64);
    services.SetLastError(1234);
    RE2DJ_CHECK_EQ(context, call(user32, "DrawTextA", {dc, kText, 4, kRect, 0x25}), 40U);
    RE2DJ_CHECK_EQ(context, services.LastError(), 0U);
    RE2DJ_CHECK_EQ(context, call(user32, "DrawTextA", {dc, kText, 0xFFFFFFFFU, kRect, 0x20}), 16U);
    RE2DJ_CHECK_EQ(context, call(user32, "DrawTextA", {dc, kText, 4, kRect, 0x2A}), 64U);
    services.SetLastError(1234);
    RE2DJ_CHECK_EQ(context, call(user32, "DrawTextA", {dc, kText, 0, kRect, 0x25}), 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), 1234U);
    RE2DJ_CHECK_EQ(context, call(user32, "DrawTextA", {0x12345678U, kText, 4, kRect, 0x25}), 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), re2dj::hle::kWin32ErrorInvalidParameter);
    handled = true;
    CallModuleExport(context, services, user32, "DrawTextA", {dc, kText, 4, kRect, 0x425}, &handled);
    RE2DJ_CHECK(context, !handled);

    // The glyphs: Unifont's "t", rows 4 to 7 (0x10, 0x10, 0x10, 0x7C), drawn
    // unclipped from a rectangle 4 pixels above the bitmap. TRANSPARENT mode
    // leaves the other pixels as they were.
    put_rect(0, 0, 8, 4);
    call(user32, "FillRect", {dc, kRect, black});
    call(gdi32, "SetBkMode", {dc, 1});
    put_rect(0, -4, 8, 0);
    RE2DJ_CHECK_EQ(context, call(user32, "DrawTextA", {dc, kText, 1, kRect, 0x120}), 16U);
    RE2DJ_CHECK_EQ(context, pixel(3, 0), std::uint16_t{0xF800});
    RE2DJ_CHECK_EQ(context, pixel(2, 0), std::uint16_t{0x0000});
    RE2DJ_CHECK_EQ(context, pixel(3, 2), std::uint16_t{0xF800});
    RE2DJ_CHECK_EQ(context, pixel(1, 3), std::uint16_t{0xF800});
    RE2DJ_CHECK_EQ(context, pixel(5, 3), std::uint16_t{0xF800});
    RE2DJ_CHECK_EQ(context, pixel(6, 3), std::uint16_t{0x0000});
    // Clipped to the rectangle without DT_NOCLIP: nothing lands.
    put_rect(0, 0, 8, 4);
    call(user32, "FillRect", {dc, kRect, black});
    put_rect(0, -4, 8, 0);
    call(user32, "DrawTextA", {dc, kText, 1, kRect, 0x20});
    RE2DJ_CHECK_EQ(context, pixel(3, 0), std::uint16_t{0x0000});
    // OPAQUE mode paints the cell white: "e" rows 5 and 6 (0x00, 0x3C).
    call(gdi32, "SetBkMode", {dc, 2});
    services.Put(kText, "e");
    put_rect(0, -5, 8, -1);
    call(user32, "DrawTextA", {dc, kText, 1, kRect, 0x120});
    RE2DJ_CHECK_EQ(context, pixel(0, 0), std::uint16_t{0xFFFF});
    RE2DJ_CHECK_EQ(context, pixel(1, 1), std::uint16_t{0xFFFF});
    RE2DJ_CHECK_EQ(context, pixel(2, 1), std::uint16_t{0xF800});
    RE2DJ_CHECK_EQ(context, pixel(5, 1), std::uint16_t{0xF800});
    RE2DJ_CHECK_EQ(context, pixel(6, 1), std::uint16_t{0xFFFF});
    RE2DJ_CHECK_EQ(context, pixel(0, 2), std::uint16_t{0xFFFF});
    // Not modelled: a byte outside printable ASCII, and a palette color.
    services.Put(kText, "\xB0");
    handled = true;
    CallModuleExport(context, services, user32, "DrawTextA", {dc, kText, 1, kRect, 0x120}, &handled);
    RE2DJ_CHECK(context, !handled);
    services.Put(kText, "e");
    call(gdi32, "SetTextColor", {dc, 0x01000003U});
    handled = true;
    CallModuleExport(context, services, user32, "DrawTextA", {dc, kText, 1, kRect, 0x120}, &handled);
    RE2DJ_CHECK(context, !handled);
    call(descriptor, "IDirectDrawSurface7::ReleaseDC", {texture, dc});
}

// A texture's DC: GetDC gives a DC over the surface's pixels, which
// StretchDIBits fills as Windows 11 GDI does; a second GetDC, and releasing
// another DC, are refused; SetColorKey takes only a source blit key.
void CheckSurfaceDc(re2dj::test::Context& context)
{
    namespace dx = re2dj::directx;
    const auto descriptor = modules::MakeDdrawModuleDescriptor();
    const auto gdi32 = modules::MakeGdi32ModuleDescriptor();
    MemoryServices services;
    const std::uint32_t direct_draw = CreateDirectDraw(context, services, descriptor);
    const auto call = [&](const char* name, std::initializer_list<std::uint32_t> arguments) {
        return CallModuleExport(context, services, descriptor, name, arguments).eax;
    };
    constexpr std::uint32_t kDesc = MemoryServices::kBase + 0x100;
    constexpr std::uint32_t kOut = MemoryServices::kBase + 0x40;
    dx::DdSurfaceDesc2 request;
    request.size = sizeof(request);
    request.flags = 0x00001007U;
    request.caps.caps = 0x10005000U;
    request.width = 4;
    request.height = 2;
    request.pixel_format = dx::Rgb565Format();
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&request);
    for (std::uint32_t index = 0; index < sizeof(request); ++index)
    {
        services.Byte(kDesc + index) = bytes[index];
    }
    RE2DJ_CHECK_EQ(context, call("IDirectDraw7::CreateSurface", {direct_draw, kDesc, kOut, 0}), dx::kDdOk);
    const std::uint32_t texture = services.U32(kOut);

    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface7::GetDC", {texture, kOut}), dx::kDdOk);
    const std::uint32_t dc = services.U32(kOut);
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface7::GetDC", {texture, kOut}), dx::kDdErrDcAlreadyCreated);
    const re2dj::hle::GuestDc* guest_dc = services.Process()->gdi().FindDc(dc);
    RE2DJ_CHECK(context, guest_dc != nullptr);
    if (guest_dc == nullptr)
    {
        return;
    }
    const std::uint32_t pixels = services.Process()->gdi().FindBitmap(guest_dc->bitmap)->bits;

    // A bottom-up 4x2 5-5-5 DIB, as measured: the top destination row is the
    // DIB's second stored row.
    constexpr std::uint32_t kInfo = MemoryServices::kBase + 0x300;
    constexpr std::uint32_t kBits = MemoryServices::kBase + 0x340;
    services.PutU32(kInfo, 40);
    services.PutU32(kInfo + 4, 4);
    services.PutU32(kInfo + 8, 2);
    services.PutU32(kInfo + 12, 0x00100001U);
    services.PutU32(kInfo + 16, 0);
    const std::uint16_t source[8] = {0x0000, 0x7FFF, 0x001F, 0x03E0, 0x7C00, 0x0421, 0x4210, 0x3DEF};
    for (std::uint32_t index = 0; index < 8; ++index)
    {
        services.Byte(kBits + index * 2) = static_cast<std::uint8_t>(source[index]);
        services.Byte(kBits + index * 2 + 1) = static_cast<std::uint8_t>(source[index] >> 8);
    }
    RE2DJ_CHECK_EQ(context,
                   CallModuleExport(context, services, gdi32, "StretchDIBits",
                                    {dc, 0, 0, 4, 2, 0, 0, 4, 2, kBits, kInfo, 0, 0x00CC0020U}).eax,
                   2U);
    const std::uint16_t expected[8] = {0xF800, 0x0841, 0x8430, 0x7BCF, 0x0000, 0xFFFF, 0x001F, 0x07E0};
    for (std::uint32_t index = 0; index < 8; ++index)
    {
        const std::uint32_t pixel = services.Byte(pixels + (index / 4) * 8 + (index % 4) * 2) |
                                    (services.Byte(pixels + (index / 4) * 8 + (index % 4) * 2 + 1) << 8);
        RE2DJ_CHECK_EQ(context, pixel, static_cast<std::uint32_t>(expected[index]));
    }

    // A bottom-up 4x2 8-bit palettized DIB, as measured (design 426): the
    // palette converts as 24-bit colours do, biClrUsed 0 means 256 entries,
    // and an index past a shorter palette is black.
    constexpr std::uint32_t kInfo8 = MemoryServices::kBase + 0x4000;
    constexpr std::uint32_t kBits8 = MemoryServices::kBase + 0x4500;
    services.PutU32(kInfo8, 40);
    services.PutU32(kInfo8 + 4, 4);
    services.PutU32(kInfo8 + 8, 2);
    services.PutU32(kInfo8 + 12, 0x00080001U);
    services.PutU32(kInfo8 + 16, 0);
    services.PutU32(kInfo8 + 32, 0);
    const std::uint32_t colors[4] = {0x000000U, 0xFFFFFFU, 0x7F8081U, 0x070803U};
    for (std::uint32_t index = 0; index < 256; ++index)
    {
        const std::uint32_t rgb = index < 4 ? colors[index] : 0x204060U;
        services.PutU32(kInfo8 + 40 + index * 4, ((rgb >> 16) & 0xFF) << 16 | ((rgb >> 8) & 0xFF) << 8 | (rgb & 0xFF));
    }
    // Stored bottom row first: {200, 3, 3, 3}, then the top row {0, 1, 2, 3}.
    const std::uint8_t indices[8] = {200, 3, 3, 3, 0, 1, 2, 3};
    for (std::uint32_t index = 0; index < 8; ++index)
    {
        services.Byte(kBits8 + index) = indices[index];
    }
    const auto read_pixel = [&](std::uint32_t index) {
        return static_cast<std::uint32_t>(services.Byte(pixels + (index / 4) * 8 + (index % 4) * 2) |
                                          (services.Byte(pixels + (index / 4) * 8 + (index % 4) * 2 + 1) << 8));
    };
    RE2DJ_CHECK_EQ(context,
                   CallModuleExport(context, services, gdi32, "StretchDIBits",
                                    {dc, 0, 0, 4, 2, 0, 0, 4, 2, kBits8, kInfo8, 0, 0x00CC0020U}).eax,
                   2U);
    const std::uint16_t expected8[8] = {0x0000, 0xFFFF, 0x7C10, 0x0040, 0x220C, 0x0040, 0x0040, 0x0040};
    for (std::uint32_t index = 0; index < 8; ++index)
    {
        RE2DJ_CHECK_EQ(context, read_pixel(index), static_cast<std::uint32_t>(expected8[index]));
    }
    services.PutU32(kInfo8 + 32, 4);
    CallModuleExport(context, services, gdi32, "StretchDIBits",
                     {dc, 0, 0, 4, 2, 0, 0, 4, 2, kBits8, kInfo8, 0, 0x00CC0020U});
    RE2DJ_CHECK_EQ(context, read_pixel(4), 0U);
    // DIB_PAL_COLORS is not modelled.
    bool handled = true;
    CallModuleExport(context, services, gdi32, "StretchDIBits",
                     {dc, 0, 0, 4, 2, 0, 0, 4, 2, kBits8, kInfo8, 1, 0x00CC0020U}, &handled);
    RE2DJ_CHECK(context, !handled);

    // A top-down 4x2 32-bit BI_RGB DIB, with the pixels measured on Windows
    // 11 (task 431): each converts as its 24-bit colour does, the top byte
    // ignored.
    constexpr std::uint32_t kInfo32 = MemoryServices::kBase + 0x4800;
    constexpr std::uint32_t kBits32 = MemoryServices::kBase + 0x4900;
    services.PutU32(kInfo32, 40);
    services.PutU32(kInfo32 + 4, 4);
    services.PutU32(kInfo32 + 8, static_cast<std::uint32_t>(-2));
    services.PutU32(kInfo32 + 12, 0x00200001U);
    services.PutU32(kInfo32 + 16, 0);
    const std::uint32_t source32[8] = {0x00FF7F3FU, 0xFF123456U, 0x80FFFFFFU, 0x00070307U,
                                       0x00080408U, 0x12F8FCF8U, 0x00000000U, 0xFF0000FFU};
    for (std::uint32_t index = 0; index < 8; ++index)
    {
        services.PutU32(kBits32 + index * 4, source32[index]);
    }
    RE2DJ_CHECK_EQ(context,
                   CallModuleExport(context, services, gdi32, "StretchDIBits",
                                    {dc, 0, 0, 4, 2, 0, 0, 4, 2, kBits32, kInfo32, 0, 0x00CC0020U}).eax,
                   2U);
    const std::uint16_t expected32[8] = {0xFBE7, 0x11AA, 0xFFFF, 0x0000, 0x0821, 0xFFFF, 0x0000, 0x001F};
    for (std::uint32_t index = 0; index < 8; ++index)
    {
        RE2DJ_CHECK_EQ(context, read_pixel(index), static_cast<std::uint32_t>(expected32[index]));
    }

    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface7::ReleaseDC", {texture, dc + 4}), dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface7::ReleaseDC", {texture, dc}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface7::ReleaseDC", {texture, dc}), dx::kDdErrInvalidParams);

    constexpr std::uint32_t kKey = MemoryServices::kBase + 0x60;
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface7::SetColorKey", {texture, dx::kDdckeySrcBlt, kKey}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface7::SetColorKey", {texture, 2, kKey}), dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface7::SetColorKey", {texture, dx::kDdckeySrcBlt, 0}),
                   dx::kDdErrInvalidParams);

    // Releasing the texture forgets its DC.
    call("IDirectDrawSurface7::Release", {texture});
    RE2DJ_CHECK(context, services.Process()->gdi().FindDc(dc) == nullptr);
}

// Selects a colour depth for one test and puts the default back after it.
class ScopedColorDepth
{
public:
    explicit ScopedColorDepth(re2dj::graphics::ColorDepth depth) { re2dj::graphics::SelectColorDepth(depth); }
    ~ScopedColorDepth() { re2dj::graphics::SelectColorDepth(re2dj::graphics::ColorDepth::k16); }
    ScopedColorDepth(const ScopedColorDepth&) = delete;
    ScopedColorDepth& operator=(const ScopedColorDepth&) = delete;
};

// 32-bit colour (design 429): the guest's RGB565 pixels stay what 16-bit
// colour gives, while each surface's true-color plane keeps what GDI, clears,
// copies, and fills put there at 8 bits per channel, the pixels a guest lock
// changed are widened, and textures reach the host with their plane.
void CheckTrueColorSurfaces(re2dj::test::Context& context)
{
    namespace dx = re2dj::directx;
    namespace graphics = re2dj::graphics;
    const ScopedColorDepth depth(graphics::ColorDepth::k32);
    const auto descriptor = modules::MakeDdrawModuleDescriptor();
    const auto gdi32 = modules::MakeGdi32ModuleDescriptor();
    FakePresentation presentation;
    MemoryServices services;
    services.presentation = &presentation;
    const std::uint32_t direct_draw = CreateDirectDraw(context, services, descriptor);
    const auto call = [&](const char* name, std::initializer_list<std::uint32_t> arguments) {
        return CallModuleExport(context, services, descriptor, name, arguments).eax;
    };
    constexpr std::uint32_t kGuid = MemoryServices::kBase + 0x40;
    constexpr std::uint32_t kOut = MemoryServices::kBase + 0x70;
    constexpr std::uint32_t kData = MemoryServices::kBase + 0x100;
    const auto put_desc = [&](const dx::DdSurfaceDesc2& desc) {
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(&desc);
        for (std::uint32_t index = 0; index < sizeof(desc); ++index)
        {
            services.Byte(kData + index) = bytes[index];
        }
    };
    const auto put_guid = [&](const dx::Guid& guid) {
        for (std::uint32_t index = 0; index < 16; ++index)
        {
            services.Byte(kGuid + index) = guid[index];
        }
    };
    dx::DdSurfaceDesc2 primary;
    primary.size = sizeof(primary);
    primary.flags = dx::kDdsdCaps | dx::kDdsdBackBufferCount;
    primary.caps.caps = 0x00002218U;
    primary.back_buffer_count = 1;
    put_desc(primary);
    call("IDirectDraw7::CreateSurface", {direct_draw, kData, kOut, 0});
    const std::uint32_t front = services.U32(kOut);
    services.PutU32(kData, dx::kDdsCapsBackBuffer);
    call("IDirectDrawSurface7::GetAttachedSurface", {front, kData, kOut});
    const std::uint32_t back = services.U32(kOut);
    dx::DdSurfaceDesc2 texture_desc;
    texture_desc.size = sizeof(texture_desc);
    texture_desc.flags = 0x00001007U;
    texture_desc.caps.caps = 0x10005000U;
    texture_desc.width = 4;
    texture_desc.height = 2;
    texture_desc.pixel_format = dx::Rgb565Format();
    put_desc(texture_desc);
    call("IDirectDraw7::CreateSurface", {direct_draw, kData, kOut, 0});
    const std::uint32_t texture = services.U32(kOut);
    put_desc(texture_desc);
    call("IDirectDraw7::CreateSurface", {direct_draw, kData, kOut, 0});
    const std::uint32_t copy = services.U32(kOut);
    put_guid(dx::kIidDirect3D7);
    call("IDirectDraw7::QueryInterface", {direct_draw, kGuid, kOut});
    const std::uint32_t direct3d = services.U32(kOut);
    put_guid(dx::kIidDirect3DHalDevice);
    call("IDirect3D7::CreateDevice", {direct3d, kGuid, back, kOut});
    const std::uint32_t device = services.U32(kOut);

    // A surface's DC, its RGB565 pixels, and its plane.
    struct SurfaceDc
    {
        std::uint32_t dc = 0;
        std::uint32_t pixels = 0;
        std::uint32_t pitch = 0;
        const re2dj::hle::GuestBitmap* bitmap = nullptr;
    };
    const auto open_dc = [&](std::uint32_t surface) {
        SurfaceDc result;
        RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface7::GetDC", {surface, kOut}), dx::kDdOk);
        result.dc = services.U32(kOut);
        const re2dj::hle::GuestDc* guest_dc = services.Process()->gdi().FindDc(result.dc);
        result.bitmap = guest_dc == nullptr ? nullptr : services.Process()->gdi().FindBitmap(guest_dc->bitmap);
        if (result.bitmap != nullptr)
        {
            result.pixels = result.bitmap->bits;
            result.pitch = result.bitmap->pitch;
        }
        return result;
    };
    const auto rgb565 = [&](const SurfaceDc& surface, std::uint32_t x, std::uint32_t y) {
        return static_cast<std::uint32_t>(services.Byte(surface.pixels + y * surface.pitch + x * 2) |
                                          (services.Byte(surface.pixels + y * surface.pitch + x * 2 + 1) << 8));
    };
    const auto plane = [&](const SurfaceDc& surface, std::uint32_t x, std::uint32_t y) {
        return surface.bitmap == nullptr || surface.bitmap->true_color == nullptr
                   ? 0xFFFFFFFFU
                   : surface.bitmap->true_color->Row(y)[x];
    };

    // A whole-target clear keeps the guest's 8-bit channels on the host and in
    // the back buffer's plane, and gives the RGB565 pixels their narrowing.
    call("IDirect3DDevice7::Clear", {device, 0, 0, dx::kD3dClearTarget, 0x80123456U, 0, 0});
    RE2DJ_CHECK(context, presentation.clears.empty());
    RE2DJ_CHECK(context, presentation.true_color_clears == std::vector<std::uint32_t>{0x00123456U});
    const SurfaceDc back_dc = open_dc(back);
    RE2DJ_CHECK_EQ(context, rgb565(back_dc, 0, 0), 0x11AAU);
    RE2DJ_CHECK_EQ(context, plane(back_dc, 639, 479), 0x00123456U);
    call("IDirectDrawSurface7::ReleaseDC", {back, back_dc.dc});

    // A bottom-up 4x2 24-bit DIB through the texture's DC: RGB565 narrowed as
    // in 16-bit colour, the plane as given.
    constexpr std::uint32_t kInfo = MemoryServices::kBase + 0x4000;
    constexpr std::uint32_t kBits = MemoryServices::kBase + 0x4100;
    services.PutU32(kInfo, 40);
    services.PutU32(kInfo + 4, 4);
    services.PutU32(kInfo + 8, 2);
    services.PutU32(kInfo + 12, 0x00180001U);
    services.PutU32(kInfo + 16, 0);
    const std::uint32_t top[4] = {0x7F8081U, 0x070803U, 0xFFFFFFU, 0x000001U};
    const std::uint32_t bottom[4] = {0x123456U, 0xABCDEFU, 0x808080U, 0x010203U};
    for (std::uint32_t x = 0; x < 4; ++x)
    {
        // Stored bottom row first, each pixel blue, green, red.
        for (std::uint32_t channel = 0; channel < 3; ++channel)
        {
            services.Byte(kBits + x * 3 + channel) = static_cast<std::uint8_t>(bottom[x] >> (8 * channel));
            services.Byte(kBits + 12 + x * 3 + channel) = static_cast<std::uint8_t>(top[x] >> (8 * channel));
        }
    }
    const SurfaceDc texture_dc = open_dc(texture);
    RE2DJ_CHECK_EQ(context,
                   CallModuleExport(context, services, gdi32, "StretchDIBits",
                                    {texture_dc.dc, 0, 0, 4, 2, 0, 0, 4, 2, kBits, kInfo, 0, 0x00CC0020U}).eax,
                   2U);
    bool gdi_matches = true;
    for (std::uint32_t x = 0; x < 4; ++x)
    {
        gdi_matches &= plane(texture_dc, x, 0) == top[x] && plane(texture_dc, x, 1) == bottom[x];
        gdi_matches &= rgb565(texture_dc, x, 0) == graphics::NarrowToRgb565(top[x]);
        gdi_matches &= rgb565(texture_dc, x, 1) == graphics::NarrowToRgb565(bottom[x]);
    }
    RE2DJ_CHECK(context, gdi_matches);
    call("IDirectDrawSurface7::ReleaseDC", {texture, texture_dc.dc});

    // The texture reaches the host with its plane.
    call("IDirect3DDevice7::SetTexture", {device, 0, texture});
    constexpr std::uint32_t kVertices = MemoryServices::kBase + 0x400;
    for (std::uint32_t index = 0; index < 4; ++index)
    {
        const std::uint32_t vertex = kVertices + index * 32;
        const float corner[2] = {index < 2 ? 0.0f : 640.0f, index % 2 == 0 ? 480.0f : 0.0f};
        std::uint32_t bits = 0;
        std::memcpy(&bits, &corner[0], sizeof(bits));
        services.PutU32(vertex, bits);
        std::memcpy(&bits, &corner[1], sizeof(bits));
        services.PutU32(vertex + 4, bits);
        services.PutU32(vertex + 8, 0x3F000000U);
        services.PutU32(vertex + 12, 0x3F800000U);
        services.PutU32(vertex + 16, 0xFFFFFFFFU);
    }
    call("IDirect3DDevice7::DrawPrimitive", {device, dx::kD3dPtTriangleStrip, dx::kD3dFvfTlVertex, kVertices, 4, 0});
    RE2DJ_CHECK_EQ(context, presentation.true_color_rows.size(), std::size_t{1});
    if (presentation.true_color_rows.size() == 1)
    {
        RE2DJ_CHECK(context, presentation.true_color_rows[0] == std::vector<std::uint32_t>(top, top + 4));
    }

    // A lock: the pixel the guest changed is widened, the others keep theirs.
    constexpr std::uint32_t kLockDesc = MemoryServices::kBase + 0x3000;
    services.PutU32(kLockDesc, sizeof(dx::DdSurfaceDesc2));
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface7::Lock", {texture, 0, kLockDesc, 1, 0}), dx::kDdOk);
    const std::uint32_t locked = services.U32(kLockDesc + 36);
    services.Byte(locked + 2) = 0x1F;
    services.Byte(locked + 3) = 0x00;
    call("IDirectDrawSurface7::Unlock", {texture, 0});
    const SurfaceDc relocked = open_dc(texture);
    RE2DJ_CHECK_EQ(context, plane(relocked, 0, 0), top[0]);
    RE2DJ_CHECK_EQ(context, plane(relocked, 1, 0), graphics::WidenRgb565(0x001F));
    RE2DJ_CHECK_EQ(context, plane(relocked, 2, 0), top[2]);
    call("IDirectDrawSurface7::ReleaseDC", {texture, relocked.dc});

    // A keyed copy takes the source's plane where the source's RGB565 pixel
    // is not the key, and a partial colour fill widens its RGB565 colour.
    constexpr std::uint32_t kKey = MemoryServices::kBase + 0x60;
    const std::uint32_t key = graphics::NarrowToRgb565(top[2]);
    services.PutU32(kKey, key);
    services.PutU32(kKey + 4, key);
    call("IDirectDrawSurface7::SetColorKey", {texture, dx::kDdckeySrcBlt, kKey});
    RE2DJ_CHECK_EQ(context,
                   call("IDirectDrawSurface7::BltFast", {copy, 0, 0, texture, 0, dx::kDdBltFastSrcColorKey}),
                   dx::kDdOk);
    constexpr std::uint32_t kFx = MemoryServices::kBase + 0x3200;
    constexpr std::uint32_t kFillRect = MemoryServices::kBase + 0x3300;
    services.PutU32(kFx, dx::kDdBltFxSize);
    services.PutU32(kFx + dx::kDdBltFxFillColorOffset, 0x8410);
    services.PutU32(kFillRect, 3);
    services.PutU32(kFillRect + 4, 1);
    services.PutU32(kFillRect + 8, 4);
    services.PutU32(kFillRect + 12, 2);
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface7::Blt", {copy, kFillRect, 0, 0, dx::kDdBltColorFill, kFx}),
                   dx::kDdOk);
    const SurfaceDc copy_dc = open_dc(copy);
    RE2DJ_CHECK_EQ(context, plane(copy_dc, 0, 0), top[0]);
    RE2DJ_CHECK_EQ(context, plane(copy_dc, 2, 0), 0U);
    RE2DJ_CHECK_EQ(context, plane(copy_dc, 1, 1), bottom[1]);
    RE2DJ_CHECK_EQ(context, plane(copy_dc, 3, 1), graphics::WidenRgb565(0x8410));
    RE2DJ_CHECK_EQ(context, rgb565(copy_dc, 1, 1), graphics::NarrowToRgb565(bottom[1]));
    call("IDirectDrawSurface7::ReleaseDC", {copy, copy_dc.dc});

    // A surface made in 16-bit colour has no plane until 32-bit colour needs
    // one; its first texture view then widens its RGB565 pixels.
    graphics::SelectColorDepth(graphics::ColorDepth::k16);
    put_desc(texture_desc);
    call("IDirectDraw7::CreateSurface", {direct_draw, kData, kOut, 0});
    const std::uint32_t plain = services.U32(kOut);
    const SurfaceDc plain_dc = open_dc(plain);
    RE2DJ_CHECK(context, plain_dc.bitmap != nullptr && plain_dc.bitmap->true_color == nullptr);
    services.Byte(plain_dc.pixels) = 0x10;
    services.Byte(plain_dc.pixels + 1) = 0x84;
    call("IDirectDrawSurface7::ReleaseDC", {plain, plain_dc.dc});
    call("IDirect3DDevice7::SetTexture", {device, 0, plain});
    call("IDirect3DDevice7::DrawPrimitive", {device, dx::kD3dPtTriangleStrip, dx::kD3dFvfTlVertex, kVertices, 4, 0});
    graphics::SelectColorDepth(graphics::ColorDepth::k32);
    call("IDirect3DDevice7::DrawPrimitive", {device, dx::kD3dPtTriangleStrip, dx::kD3dFvfTlVertex, kVertices, 4, 0});
    RE2DJ_CHECK_EQ(context, presentation.true_color_rows.size(), std::size_t{3});
    if (presentation.true_color_rows.size() == 3)
    {
        RE2DJ_CHECK(context, presentation.true_color_rows[1].empty());
        RE2DJ_CHECK_EQ(context, presentation.true_color_rows[2].size(), std::size_t{4});
        RE2DJ_CHECK_EQ(context, presentation.true_color_rows[2][0], graphics::WidenRgb565(0x8410));
    }
    call("IDirect3DDevice7::SetTexture", {device, 0, 0});
}

}  // namespace

// A windowed title's clipper (#15): CreateClipper, SetHWnd and GetHWnd, and a
// surface holding it through SetClipper until it is detached.
void CheckClipper(re2dj::test::Context& context)
{
    namespace dx = re2dj::directx;
    const auto descriptor = modules::MakeDdrawModuleDescriptor();
    MemoryServices services;
    const std::uint32_t direct_draw = CreateDirectDraw(context, services, descriptor);
    const auto call = [&](const char* name, std::initializer_list<std::uint32_t> arguments) {
        return CallModuleExport(context, services, descriptor, name, arguments).eax;
    };
    constexpr std::uint32_t kOut = MemoryServices::kBase + 0x40;
    RE2DJ_CHECK_EQ(context, call("IDirectDraw7::CreateClipper", {direct_draw, 0, kOut, 0}), dx::kDdOk);
    const std::uint32_t clipper = services.U32(kOut);
    RE2DJ_CHECK(context, clipper != 0);
    RE2DJ_CHECK_EQ(context, call("IDirectDraw7::CreateClipper", {direct_draw, 0, 0, 0}), dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, call("IDirectDraw7::CreateClipper", {direct_draw, 0, kOut, 4}), dx::kClassENoAggregation);

    constexpr std::uint32_t kWindow = 0x00010014U;
    RE2DJ_CHECK_EQ(context, call("IDirectDrawClipper::SetHWnd", {clipper, 1, kWindow}), dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, call("IDirectDrawClipper::SetHWnd", {clipper, 0, kWindow}), dx::kDdOk);
    services.PutU32(kOut, 0);
    RE2DJ_CHECK_EQ(context, call("IDirectDrawClipper::GetHWnd", {clipper, kOut}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, services.U32(kOut), kWindow);

    // A primary without back buffers, as a windowed title makes it.
    constexpr std::uint32_t kDesc = MemoryServices::kBase + 0x100;
    dx::DdSurfaceDesc2 primary;
    primary.size = sizeof(primary);
    primary.flags = dx::kDdsdCaps;
    primary.caps.caps = dx::kDdsCapsPrimarySurface;
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&primary);
    for (std::uint32_t index = 0; index < sizeof(primary); ++index)
    {
        services.Byte(kDesc + index) = bytes[index];
    }
    RE2DJ_CHECK_EQ(context, call("IDirectDraw7::CreateSurface", {direct_draw, kDesc, kOut, 0}), dx::kDdOk);
    const std::uint32_t surface = services.U32(kOut);
    RE2DJ_CHECK(context, surface != 0);

    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface7::GetClipper", {surface, kOut}), dx::kDdErrNoClipperAttached);
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface7::SetClipper", {surface, direct_draw}), dx::kDdErrInvalidObject);
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface7::SetClipper", {surface, clipper}), dx::kDdOk);
    // The guest lets go of its own reference; the surface still holds one.
    RE2DJ_CHECK_EQ(context, call("IDirectDrawClipper::Release", {clipper}), 1U);
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface7::GetClipper", {surface, kOut}), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, services.U32(kOut), clipper);
    RE2DJ_CHECK_EQ(context, call("IDirectDrawClipper::Release", {clipper}), 1U);
    // Detaching drops the last reference and the clipper goes.
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface7::SetClipper", {surface, 0}), dx::kDdOk);
    RE2DJ_CHECK(context, services.Process()->com().Find(clipper) == nullptr);
    RE2DJ_CHECK_EQ(context, call("IDirectDrawSurface7::GetClipper", {surface, kOut}), dx::kDdErrNoClipperAttached);
}

void RunDdrawModuleTests(re2dj::test::Context& context)
{
    CheckClipper(context);
    CheckSurfaceGdiDrawing(context);
    CheckVertexBuffers(context);
    CheckEnumSurfaces(context);
    CheckDrawing(context);
    CheckSurfaceDc(context);
    CheckTrueColorSurfaces(context);
    CheckDevice(context);
    CheckSurfaces(context);
    CheckHostWindow(context);
    CheckCooperativeLevelAndMode(context);
    CheckEnumerate(context);
    CheckCreate(context);
    CheckCreateDirectX6(context);
    CheckDirectX6Device(context);
    CheckDirectDrawDescriptions(context);
    CheckDirect3D(context);
}
