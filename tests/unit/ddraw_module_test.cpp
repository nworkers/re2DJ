#include "re2dj/hle/modules/ddraw_module.h"

#include <cstdint>
#include <string>
#include <vector>

#include "memory_services.h"
#include "re2dj/directx/direct3d_description.h"
#include "re2dj/directx/directdraw_description.h"
#include "re2dj/hle/guest_process.h"
#include "re2dj/hle/guest_user.h"
#include "re2dj/hle/host_presentation.h"
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
    // IDirectDraw7's 30 methods and IDirect3D7's 8.
    RE2DJ_CHECK_EQ(context, method_count, std::size_t{38});

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

}  // namespace

void RunDdrawModuleTests(re2dj::test::Context& context)
{
    CheckHostWindow(context);
    CheckCooperativeLevelAndMode(context);
    CheckEnumerate(context);
    CheckCreate(context);
    CheckDirectDrawDescriptions(context);
    CheckDirect3D(context);
}
