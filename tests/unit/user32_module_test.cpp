#include "re2dj/hle/modules/user32_module.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "memory_services.h"
#include "re2dj/hle/guest_user.h"
#include "re2dj/hle/modules/gdi32_module.h"
#include "re2dj/hle/win32_errors.h"
#include "re2dj/hle/wsprintf.h"
#include "test_support.h"

namespace
{

void CheckDescriptor(re2dj::test::Context& context)
{
    using re2dj::hle::CallingConvention;
    using re2dj::hle::ImportCall;
    using re2dj::hle::ImportReturn;
    using re2dj::runtime::GuestAddress;
    using re2dj::runtime::ImportGate;

    const auto descriptor = re2dj::hle::modules::MakeUser32ModuleDescriptor();
    std::string error = "stale";
    RE2DJ_CHECK(context,
                re2dj::hle::modules::ValidateGuestModuleDescriptor(descriptor, &error));
    RE2DJ_CHECK(context, error.empty());
    RE2DJ_CHECK_EQ(context, descriptor.name, std::string("user32.dll"));
    RE2DJ_CHECK_EQ(context, descriptor.aliases.size(), std::size_t{1});
    if (descriptor.aliases.size() == 1)
    {
        RE2DJ_CHECK_EQ(context, descriptor.aliases[0], std::string("user32"));
    }
    // The implemented exports from GetActiveWindow to wsprintfA, then 15
    // resolve-only exports.
    RE2DJ_CHECK_EQ(context, descriptor.exports.size(), std::size_t{42});
    if (descriptor.exports.size() != 42)
    {
        return;
    }

    const auto& get_active_window = descriptor.exports[0];
    RE2DJ_CHECK_EQ(context, get_active_window.name, std::string("GetActiveWindow"));
    RE2DJ_CHECK(context, !get_active_window.ordinal.has_value());
    RE2DJ_CHECK(context, get_active_window.calling_convention == CallingConvention::kStdcall);
    RE2DJ_CHECK_EQ(context, get_active_window.argument_count, std::uint32_t{0});
    RE2DJ_CHECK(context, get_active_window.handler != nullptr);
    if (get_active_window.handler == nullptr)
    {
        return;
    }

    ImportGate gate;
    gate.module = descriptor.name;
    gate.name = get_active_window.name;
    gate.address = GuestAddress(0xF1000000U);
    const ImportCall call{gate, std::span<const std::uint32_t>()};
    ImportReturn result{0x12345678U, 0x87654321U};
    error = "stale";
    RE2DJ_CHECK(context, get_active_window.handler(call, &result, &error));
    RE2DJ_CHECK(context, error.empty());
    // Without process services no window can be active.
    RE2DJ_CHECK_EQ(context, result.eax, std::uint32_t{0});
    RE2DJ_CHECK_EQ(context, result.edx, std::uint32_t{0});
    RE2DJ_CHECK(context, !get_active_window.handler(call, nullptr, &error));
}

void CheckMessageBox(re2dj::test::Context& context)
{
    using re2dj::hle::CallingConvention;
    using re2dj::hle::ImportCall;
    using re2dj::hle::ImportReturn;
    namespace modules = re2dj::hle::modules;

    const auto descriptor = modules::MakeUser32ModuleDescriptor();
    const modules::GuestExportDescriptor* message_box = nullptr;
    for (const auto& export_descriptor : descriptor.exports)
    {
        if (export_descriptor.name == "MessageBoxA")
        {
            message_box = &export_descriptor;
        }
    }
    RE2DJ_CHECK(context, message_box != nullptr);
    if (message_box == nullptr)
    {
        return;
    }
    RE2DJ_CHECK(context, message_box->calling_convention == CallingConvention::kStdcall);
    RE2DJ_CHECK_EQ(context, message_box->argument_count, std::uint32_t{4});

    // Default buttons by MB_TYPEMASK, then MB_DEFBUTTON2/3 and fallbacks.
    RE2DJ_CHECK_EQ(context, modules::MessageBoxDefaultButton(0x0), modules::kIdOk);
    RE2DJ_CHECK_EQ(context, modules::MessageBoxDefaultButton(0x1), modules::kIdOk);
    RE2DJ_CHECK_EQ(context, modules::MessageBoxDefaultButton(0x2), modules::kIdAbort);
    RE2DJ_CHECK_EQ(context, modules::MessageBoxDefaultButton(0x3), modules::kIdYes);
    RE2DJ_CHECK_EQ(context, modules::MessageBoxDefaultButton(0x4), modules::kIdYes);
    RE2DJ_CHECK_EQ(context, modules::MessageBoxDefaultButton(0x5), modules::kIdRetry);
    RE2DJ_CHECK_EQ(context, modules::MessageBoxDefaultButton(0x6), modules::kIdCancel);
    RE2DJ_CHECK_EQ(context, modules::MessageBoxDefaultButton(0x104), modules::kIdNo);
    RE2DJ_CHECK_EQ(context, modules::MessageBoxDefaultButton(0x203), modules::kIdCancel);
    RE2DJ_CHECK_EQ(context, modules::MessageBoxDefaultButton(0x206), modules::kIdContinue);
    // MB_ICONHAND and other high bits do not change the button.
    RE2DJ_CHECK_EQ(context, modules::MessageBoxDefaultButton(0x10), modules::kIdOk);
    // A third position on a two-button box, and an unknown set, fall back.
    RE2DJ_CHECK_EQ(context, modules::MessageBoxDefaultButton(0x204), modules::kIdYes);
    RE2DJ_CHECK_EQ(context, modules::MessageBoxDefaultButton(0x7), modules::kIdOk);

    re2dj::runtime::ImportGate gate;
    gate.module = descriptor.name;
    gate.name = message_box->name;
    const std::array<std::uint32_t, 4> arguments = {0, 0x1000, 0x2000, 0x4};
    ImportReturn result{0x12345678U, 0x87654321U};
    std::string error = "stale";
    RE2DJ_CHECK(context, message_box->handler(ImportCall{gate, arguments}, &result, &error));
    RE2DJ_CHECK(context, error.empty());
    RE2DJ_CHECK_EQ(context, result.eax, modules::kIdYes);
    RE2DJ_CHECK_EQ(context, result.edx, std::uint32_t{0});
    RE2DJ_CHECK(context, !result.exit_process);

    const std::array<std::uint32_t, 3> too_few = {0, 0x1000, 0x2000};
    RE2DJ_CHECK(context, !message_box->handler(ImportCall{gate, too_few}, &result, &error));
    RE2DJ_CHECK(context, !error.empty());
}

// The cursor exports the Hardlock API resolves are addressable but fail when
// called, naming the export.
void CheckResolveOnlyCursors(re2dj::test::Context& context)
{
    using re2dj::hle::ImportCall;
    using re2dj::hle::ImportReturn;

    const auto descriptor = re2dj::hle::modules::MakeUser32ModuleDescriptor();
    const std::array<std::pair<std::string, std::uint32_t>, 3> cursors = {
        {{"CreateCursor", 7}, {"DestroyCursor", 1}, {"SetCursor", 1}}};
    for (std::size_t index = 0; index < cursors.size(); ++index)
    {
        const auto found = std::find_if(descriptor.exports.begin(), descriptor.exports.end(),
                                        [&](const auto& entry) { return entry.name == cursors[index].first; });
        RE2DJ_CHECK(context, found != descriptor.exports.end());
        if (found == descriptor.exports.end())
        {
            continue;
        }
        const auto& export_descriptor = *found;
        RE2DJ_CHECK_EQ(context, export_descriptor.argument_count, cursors[index].second);
        re2dj::runtime::ImportGate gate;
        gate.module = descriptor.name;
        gate.name = export_descriptor.name;
        const std::array<std::uint32_t, 7> arguments = {};
        ImportReturn result;
        std::string error;
        RE2DJ_CHECK(context,
                    !export_descriptor.handler(
                        ImportCall{gate,
                                   std::span<const std::uint32_t>(
                                       arguments.data(), export_descriptor.argument_count)},
                        &result,
                        &error));
        RE2DJ_CHECK(context, error.find("user32.dll!" + cursors[index].first) != std::string::npos);
    }
}

// SetTimer without a window records a thread timer: a new ID, the same ID
// when replacing it, and the elapse clamped to USER_TIMER_MINIMUM.
void CheckThreadTimer(re2dj::test::Context& context)
{
    using re2dj::test::MemoryServices;
    const auto descriptor = re2dj::hle::modules::MakeUser32ModuleDescriptor();
    MemoryServices services;
    re2dj::hle::GuestClockReading clock;
    clock.tick_ms = 1000;
    services.SetClock(clock);
    const std::uint32_t first = re2dj::test::CallModuleExport(
        context, services, descriptor, "SetTimer", {0, 0, 0x8000, 0x00aeaddbU}).eax;
    RE2DJ_CHECK(context, first != 0);
    const std::uint32_t second = re2dj::test::CallModuleExport(
        context, services, descriptor, "SetTimer", {0, 0, 1, 0x00401000U}).eax;
    RE2DJ_CHECK(context, second != first);
    RE2DJ_CHECK_EQ(context,
                   re2dj::test::CallModuleExport(
                       context, services, descriptor, "SetTimer", {0, first, 50, 0x00401000U}).eax,
                   first);
    const auto& timers = services.Process()->timers();
    RE2DJ_CHECK_EQ(context, timers.size(), std::size_t{2});
    if (timers.size() == 2)
    {
        RE2DJ_CHECK_EQ(context, timers[0].elapse_ms, 50U);
        RE2DJ_CHECK_EQ(context, timers[1].elapse_ms, 10U);
    }
    // No window exists, so a window timer is not answered.
    bool handled = true;
    std::string error;
    re2dj::test::CallModuleExport(
        context, services, descriptor, "SetTimer", {0x10000, 1, 50, 0}, &handled, &error);
    RE2DJ_CHECK(context, !handled);

    // The message loop, as measured: nothing is due at first and the MSG is
    // left alone; a due timer's WM_TIMER leaves the queue and restarts its
    // interval; PM_NOREMOVE shows it without taking it; DispatchMessageA
    // calls a live timer's procedure with the tick, and nothing else.
    const auto call = [&](const char* name, std::initializer_list<std::uint32_t> arguments) {
        return re2dj::test::CallModuleExport(context, services, descriptor, name, arguments).eax;
    };
    constexpr std::uint32_t kMsg = MemoryServices::kBase + 0x200;
    services.PutU32(kMsg + 4, 0xAAAAAAAAU);
    clock.tick_ms = 1005;
    services.SetClock(clock);
    RE2DJ_CHECK_EQ(context, call("PeekMessageA", {kMsg, 0, 0, 0, 1}), 0U);
    RE2DJ_CHECK_EQ(context, services.U32(kMsg + 4), 0xAAAAAAAAU);
    clock.tick_ms = 1010;
    services.SetClock(clock);
    RE2DJ_CHECK_EQ(context, call("PeekMessageA", {kMsg, 0, 0, 0, 1}), 1U);
    RE2DJ_CHECK_EQ(context, services.U32(kMsg + 4), 0x0113U);
    RE2DJ_CHECK_EQ(context, services.U32(kMsg + 8), second);
    RE2DJ_CHECK_EQ(context, services.U32(kMsg + 16), 1010U);
    RE2DJ_CHECK_EQ(context, call("PeekMessageA", {kMsg, 0, 0, 0, 1}), 0U);
    clock.tick_ms = 1050;
    services.SetClock(clock);
    RE2DJ_CHECK_EQ(context, call("PeekMessageA", {kMsg, 0, 0, 0, 0}), 1U);
    RE2DJ_CHECK_EQ(context, services.U32(kMsg + 8), first);
    RE2DJ_CHECK_EQ(context, call("PeekMessageA", {kMsg, 0, 0, 0, 0}), 1U);
    RE2DJ_CHECK_EQ(context, services.U32(kMsg + 8), first);
    RE2DJ_CHECK_EQ(context, call("TranslateMessage", {kMsg}), 0U);

    // PostQuitMessage: WM_QUIT with the exit code comes before a due timer,
    // stays with PM_NOREMOVE, and goes once taken (design 439).
    services.SetLastError(1234);
    RE2DJ_CHECK_EQ(context, call("PostQuitMessage", {0x105}), 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), 1234U);
    RE2DJ_CHECK_EQ(context, call("PeekMessageA", {kMsg, 0, 0, 0, 0}), 1U);
    RE2DJ_CHECK_EQ(context, services.U32(kMsg), 0U);
    RE2DJ_CHECK_EQ(context, services.U32(kMsg + 4), 0x0012U);
    RE2DJ_CHECK_EQ(context, services.U32(kMsg + 8), 0x105U);
    RE2DJ_CHECK_EQ(context, call("PeekMessageA", {kMsg, 0, 0, 0, 1}), 1U);
    RE2DJ_CHECK_EQ(context, services.U32(kMsg + 4), 0x0012U);
    RE2DJ_CHECK_EQ(context, call("PeekMessageA", {kMsg, 0, 0, 0, 0}), 1U);
    RE2DJ_CHECK_EQ(context, services.U32(kMsg + 4), 0x0113U);

    services.guest_function = [](const std::vector<std::uint32_t>&) { return 7U; };
    services.guest_calls.clear();
    clock.tick_ms = 1060;
    services.SetClock(clock);
    RE2DJ_CHECK_EQ(context, call("DispatchMessageA", {kMsg}), 7U);
    RE2DJ_CHECK_EQ(context, services.guest_calls.size(), std::size_t{1});
    if (services.guest_calls.size() == 1)
    {
        const std::vector<std::uint32_t> expected = {0, 0x0113U, first, 1060U};
        RE2DJ_CHECK(context, services.guest_calls[0] == expected);
    }
    services.PutU32(kMsg + 12, 0x12345678U);
    RE2DJ_CHECK_EQ(context, call("DispatchMessageA", {kMsg}), 0U);
    RE2DJ_CHECK_EQ(context, services.guest_calls.size(), std::size_t{1});
}

// LoadIconA and LoadCursorA serve the system set by ID with one shared
// handle, and answer names or unknown IDs with NULL and the error Windows 11
// gives; module resources are not modelled.
void CheckSystemImages(re2dj::test::Context& context)
{
    using re2dj::test::CallModuleExport;
    using re2dj::test::MemoryServices;
    const auto descriptor = re2dj::hle::modules::MakeUser32ModuleDescriptor();
    MemoryServices services;
    services.Put(MemoryServices::kBase + 0x40, "EZ2DJ");
    services.SetLastError(0x1234);
    const std::uint32_t arrow = CallModuleExport(context, services, descriptor, "LoadCursorA", {0, 32512}).eax;
    RE2DJ_CHECK(context, arrow != 0);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "LoadCursorA", {0, 32512}).eax, arrow);
    RE2DJ_CHECK_EQ(context, services.LastError(), 0x1234U);
    const std::uint32_t application = CallModuleExport(context, services, descriptor, "LoadIconA", {0, 32512}).eax;
    RE2DJ_CHECK(context, application != 0 && application != arrow);
    RE2DJ_CHECK(context, services.Process()->user().IsIcon(application));
    RE2DJ_CHECK(context, services.Process()->user().IsCursor(arrow));

    RE2DJ_CHECK_EQ(context,
                   CallModuleExport(context, services, descriptor, "LoadIconA", {0, MemoryServices::kBase + 0x40}).eax,
                   0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), 1813U);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "LoadCursorA", {0, 1}).eax, 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), 1814U);

    bool handled = true;
    CallModuleExport(context, services, descriptor, "LoadIconA", {0x00400000U, 1}, &handled);
    RE2DJ_CHECK(context, !handled);
}

// RegisterClassA hands out atoms from 0xC000, refuses a second class of the
// same name with ERROR_CLASS_ALREADY_EXISTS, and files a NULL hInstance under
// the main image.
void CheckRegisterClass(re2dj::test::Context& context)
{
    using re2dj::test::CallModuleExport;
    using re2dj::test::MemoryServices;
    const auto descriptor = re2dj::hle::modules::MakeUser32ModuleDescriptor();
    MemoryServices services;
    services.Process()->SetMainImage(0x00400000U, "D:\\ez2dj\\EZ2DJ.exe");
    constexpr std::uint32_t kName = MemoryServices::kBase + 0x40;
    constexpr std::uint32_t kClass = MemoryServices::kBase + 0x60;
    services.Put(kName, "EZ2DJ");
    const std::uint32_t words[10] = {0x23, 0x00406BAAU, 0, 8, 0, 0, 0x10010, 0x00900011U, 0, kName};
    for (std::uint32_t index = 0; index < 10; ++index)
    {
        services.PutU32(kClass + index * 4, words[index]);
    }
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "RegisterClassA", {kClass}).eax, 0xC000U);
    services.Put(kName, "ez2dj");
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "RegisterClassA", {kClass}).eax, 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), 1410U);
    const re2dj::hle::GuestWindowClass* window_class =
        services.Process()->user().FindClass("EZ2DJ", 0x00400000U);
    RE2DJ_CHECK(context, window_class != nullptr);
    if (window_class != nullptr)
    {
        RE2DJ_CHECK_EQ(context, window_class->window_procedure, 0x00406BAAU);
        RE2DJ_CHECK_EQ(context, window_class->window_extra, 8U);
        RE2DJ_CHECK_EQ(context, window_class->background, 0x00900011U);
    }
}

// ShowWindow(SW_SHOW) of a window created hidden, as 1st shows its window:
// the messages a WS_VISIBLE creation sends, 0, and the last error 0, as
// measured on Windows 11; an unknown window is ERROR_INVALID_WINDOW_HANDLE,
// and another show command stops.
void CheckShowWindow(re2dj::test::Context& context)
{
    using re2dj::test::CallModuleExport;
    using re2dj::test::MemoryServices;
    const auto descriptor = re2dj::hle::modules::MakeUser32ModuleDescriptor();
    MemoryServices services;
    services.Process()->SetMainImage(0x00400000U, "D:\\ez2dj\\Ez2DJ.exe");
    constexpr std::uint32_t kName = MemoryServices::kBase + 0x40;
    constexpr std::uint32_t kClass = MemoryServices::kBase + 0x60;
    services.Put(kName, "EZ2DJ");
    const std::uint32_t words[10] = {0x23, 0x00406BAAU, 0, 0, 0, 0, 0, 0x00900011U, 0, kName};
    for (std::uint32_t index = 0; index < 10; ++index)
    {
        services.PutU32(kClass + index * 4, words[index]);
    }
    CallModuleExport(context, services, descriptor, "RegisterClassA", {kClass});
    services.guest_function = [&](const std::vector<std::uint32_t>& arguments) {
        return CallModuleExport(context, services, descriptor, "DefWindowProcA",
                                {arguments[0], arguments[1], arguments[2], arguments[3]})
            .eax;
    };
    const std::uint32_t hidden = CallModuleExport(context, services, descriptor, "CreateWindowExA",
                                                  {0x40000, kName, kName, 0x80000000U, 0, 0, 640, 480, 0, 0,
                                                   0x00400000U, 0})
                                     .eax;
    RE2DJ_CHECK(context, hidden != 0);
    services.guest_calls.clear();
    services.SetLastError(12345);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "ShowWindow", {hidden, 5}).eax, 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), 0U);
    const std::vector<std::uint32_t> expected = {0x18, 0x46, 0x46, 0x1C, 0x86, 0x06, 0x07, 0x85, 0x14, 0x47};
    std::vector<std::uint32_t> messages;
    for (const auto& call : services.guest_calls)
    {
        messages.push_back(call[1]);
    }
    RE2DJ_CHECK(context, messages == expected);
    RE2DJ_CHECK_EQ(context, services.Process()->user().active_window(), hidden);
    // WM_DESTROY passed on by a window procedure does nothing and gives 0.
    services.SetLastError(12345);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "DefWindowProcA", {hidden, 2, 0, 0}).eax,
                   0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), 12345U);
    RE2DJ_CHECK_EQ(context, services.Process()->user().focus_window(), hidden);
    services.SetLastError(12345);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "ShowWindow", {hidden + 4, 5}).eax, 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), re2dj::hle::kWin32ErrorInvalidWindowHandle);
    bool handled = true;
    CallModuleExport(context, services, descriptor, "ShowWindow", {hidden, 1}, &handled);
    RE2DJ_CHECK(context, !handled);
}

// EnumDisplaySettingsA(NULL, ENUM_CURRENT_SETTINGS) writes the host desktop's
// mode into the DEVMODEA bytes Windows 11 writes, leaving the rest alone.
void CheckEnumDisplaySettings(re2dj::test::Context& context)
{
    using re2dj::test::CallModuleExport;
    using re2dj::test::MemoryServices;
    const auto descriptor = re2dj::hle::modules::MakeUser32ModuleDescriptor();
    MemoryServices services;
    re2dj::test::InputPresentation host;
    constexpr std::uint32_t kDevMode = MemoryServices::kBase + 0x100;
    for (std::uint32_t offset = 0; offset < 156; ++offset)
    {
        services.Byte(kDevMode + offset) = 0xCC;
    }
    bool handled = true;
    CallModuleExport(context, services, descriptor, "EnumDisplaySettingsA", {0, 0xFFFFFFFFU, kDevMode}, &handled);
    RE2DJ_CHECK(context, !handled);
    host.desktop = {3840, 2160, 32, 60};
    services.presentation = &host;
    services.SetLastError(12345);
    RE2DJ_CHECK_EQ(context,
                   CallModuleExport(context, services, descriptor, "EnumDisplaySettingsA", {0, 0xFFFFFFFFU, kDevMode}).eax,
                   1U);
    RE2DJ_CHECK_EQ(context, services.LastError(), 12345U);
    RE2DJ_CHECK_EQ(context, services.U32(kDevMode), 0x00444443U);
    RE2DJ_CHECK_EQ(context, services.Byte(kDevMode + 4), std::uint8_t{0xCC});
    RE2DJ_CHECK_EQ(context, services.U32(kDevMode + 32), 0x04010401U);
    RE2DJ_CHECK_EQ(context, services.U32(kDevMode + 36), 124U);
    RE2DJ_CHECK_EQ(context, services.U32(kDevMode + 40), 0x207C00A0U);
    RE2DJ_CHECK_EQ(context, services.U32(kDevMode + 44), 0U);
    RE2DJ_CHECK_EQ(context, services.Byte(kDevMode + 70), std::uint8_t{0});
    RE2DJ_CHECK_EQ(context, services.Byte(kDevMode + 71), std::uint8_t{0xCC});
    RE2DJ_CHECK_EQ(context, services.U32(kDevMode + 104), 32U);
    RE2DJ_CHECK_EQ(context, services.U32(kDevMode + 108), 3840U);
    RE2DJ_CHECK_EQ(context, services.U32(kDevMode + 112), 2160U);
    RE2DJ_CHECK_EQ(context, services.U32(kDevMode + 120), 60U);
    RE2DJ_CHECK_EQ(context, services.Byte(kDevMode + 124), std::uint8_t{0xCC});
    // Enumeration by index is not modelled.
    handled = true;
    CallModuleExport(context, services, descriptor, "EnumDisplaySettingsA", {0, 0, kDevMode}, &handled);
    RE2DJ_CHECK(context, !handled);

    // ChangeDisplaySettingsExA is absorbed as on the Windows product: success,
    // the last error 0, the desktop unchanged.
    services.SetLastError(12345);
    RE2DJ_CHECK_EQ(context,
                   CallModuleExport(context, services, descriptor, "ChangeDisplaySettingsExA", {0, kDevMode, 0, 1, 0}).eax,
                   0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), 0U);
    RE2DJ_CHECK_EQ(context, services.U32(kDevMode + 108), 3840U);
}

// CreateWindowExA sends 4th's WS_POPUP | WS_VISIBLE window the messages
// Windows 11 sends, with a window procedure that passes everything to
// DefWindowProcA, as 4th's does for these messages.
void CheckCreateWindow(re2dj::test::Context& context)
{
    using re2dj::test::CallModuleExport;
    using re2dj::test::MemoryServices;
    const auto descriptor = re2dj::hle::modules::MakeUser32ModuleDescriptor();
    MemoryServices services;
    services.Process()->SetMainImage(0x00400000U, "D:\\ez2dj\\EZ2DJ.exe");
    constexpr std::uint32_t kName = MemoryServices::kBase + 0x40;
    constexpr std::uint32_t kClass = MemoryServices::kBase + 0x60;
    services.Put(kName, "EZ2DJ");
    const std::uint32_t words[10] = {0x23, 0x00406BAAU, 0, 0, 0, 0, 0, 0x00900011U, 0, kName};
    for (std::uint32_t index = 0; index < 10; ++index)
    {
        services.PutU32(kClass + index * 4, words[index]);
    }
    CallModuleExport(context, services, descriptor, "RegisterClassA", {kClass});
    // Each message's DefWindowProcA result, and WINDOWPOS flags as sent.
    std::vector<std::uint32_t> answers;
    std::vector<std::uint32_t> position_flags;
    services.guest_function = [&](const std::vector<std::uint32_t>& arguments) {
        if (arguments[1] == 0x46 || arguments[1] == 0x47)
        {
            position_flags.push_back(services.U32(arguments[3] + 24));
        }
        const std::uint32_t answer = CallModuleExport(context, services, descriptor, "DefWindowProcA",
                                                      {arguments[0], arguments[1], arguments[2], arguments[3]})
                                         .eax;
        answers.push_back(answer);
        return answer;
    };
    const std::uint32_t window = CallModuleExport(context, services, descriptor, "CreateWindowExA",
                                                  {0x40000, kName, kName, 0x90000000U, 0, 0, 640, 480, 0, 0,
                                                   0x00400000U, 0})
                                     .eax;
    RE2DJ_CHECK(context, window != 0);
    // WM_SETFOCUS comes from DefWindowProcA's WM_ACTIVATE, inside it.
    const std::vector<std::uint32_t> expected = {0x81, 0x83, 0x01, 0x05, 0x03, 0x18, 0x46, 0x46,
                                                 0x1C, 0x86, 0x06, 0x07, 0x85, 0x14, 0x47};
    std::vector<std::uint32_t> messages;
    for (const auto& call : services.guest_calls)
    {
        RE2DJ_CHECK_EQ(context, call[0], window);
        messages.push_back(call[1]);
    }
    RE2DJ_CHECK(context, messages == expected);
    if (services.guest_calls.size() == expected.size())
    {
        // WM_SIZE and WM_MOVE carry the client size and origin; WM_ERASEBKGND
        // an HDC.
        RE2DJ_CHECK_EQ(context, services.guest_calls[3][3], 0x01E00280U);
        RE2DJ_CHECK_EQ(context, services.guest_calls[4][3], 0U);
        RE2DJ_CHECK(context, services.guest_calls[13][2] != 0);
    }
    const std::vector<std::uint32_t> expected_flags = {0x43, 0x03, 0x10001843U};
    RE2DJ_CHECK(context, position_flags == expected_flags);
    // WM_NCCREATE, WM_NCACTIVATE, and WM_ERASEBKGND (with a class brush) are
    // answered 1; everything else 0.
    RE2DJ_CHECK_EQ(context, std::count(answers.begin(), answers.end(), 1U), std::ptrdiff_t{3});

    re2dj::hle::GuestUser& user = services.Process()->user();
    const re2dj::hle::GuestWindow* created = user.LookupWindow(window);
    RE2DJ_CHECK(context, created != nullptr);
    if (created != nullptr)
    {
        RE2DJ_CHECK_EQ(context, created->style, 0x94000000U);
        RE2DJ_CHECK_EQ(context, created->client_right, 640);
        RE2DJ_CHECK_EQ(context, created->client_bottom, 480);
        RE2DJ_CHECK_EQ(context, created->title, std::string("EZ2DJ"));
    }
    RE2DJ_CHECK_EQ(context, user.active_window(), window);
    RE2DJ_CHECK_EQ(context, user.focus_window(), window);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "GetActiveWindow", {}).eax, window);
    // The guest's one thread is the foreground thread.
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "GetForegroundWindow", {}).eax, window);

    // GetWindowLongA reads the window's fields and leaves the last error
    // alone; an unknown index or window is 0 with the error Windows 11 sets.
    services.SetLastError(12345);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "GetWindowLongA", {window, 0xFFFFFFF0U}).eax,
                   0x94000000U);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "GetWindowLongA", {window, 0xFFFFFFFAU}).eax,
                   created == nullptr ? 0U : created->instance);
    RE2DJ_CHECK_EQ(context, services.LastError(), 12345U);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "GetWindowLongA", {window, 0xFFFFFFF9U}).eax,
                   0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), re2dj::hle::kWin32ErrorInvalidIndex);
    CallModuleExport(context, services, descriptor, "GetWindowLongA", {window + 4, 0xFFFFFFFAU});
    RE2DJ_CHECK_EQ(context, services.LastError(), re2dj::hle::kWin32ErrorInvalidWindowHandle);

    // The cursor stays at the screen origin; ScreenToClient takes off the
    // window's client origin; a null point is ERROR_NOACCESS.
    constexpr std::uint32_t kPoint = MemoryServices::kBase + 0x280;
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "GetCursorPos", {kPoint}).eax, 1U);
    RE2DJ_CHECK_EQ(context, services.U32(kPoint), 0U);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "ScreenToClient", {window, kPoint}).eax, 1U);
    if (created != nullptr)
    {
        RE2DJ_CHECK_EQ(context, services.U32(kPoint),
                       static_cast<std::uint32_t>(-(created->x + created->client_left)));
    }
    services.SetLastError(12345);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "GetCursorPos", {0}).eax, 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), re2dj::hle::kWin32ErrorNoAccess);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "ScreenToClient", {window + 4, kPoint}).eax,
                   0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), re2dj::hle::kWin32ErrorInvalidWindowHandle);

    // ClientToScreen undoes ScreenToClient; GetClientRect is the client area
    // from its own origin (#15).
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "ClientToScreen", {window, kPoint}).eax, 1U);
    RE2DJ_CHECK_EQ(context, services.U32(kPoint), 0U);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "ClientToScreen", {window + 4, kPoint}).eax,
                   0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), re2dj::hle::kWin32ErrorInvalidWindowHandle);
    constexpr std::uint32_t kRect = MemoryServices::kBase + 0x2A0;
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "GetClientRect", {window, kRect}).eax, 1U);
    RE2DJ_CHECK_EQ(context, services.U32(kRect), 0U);
    RE2DJ_CHECK_EQ(context, services.U32(kRect + 4), 0U);
    RE2DJ_CHECK_EQ(context, services.U32(kRect + 8), 640U);
    RE2DJ_CHECK_EQ(context, services.U32(kRect + 12), 480U);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "GetClientRect", {window + 4, kRect}).eax,
                   0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), re2dj::hle::kWin32ErrorInvalidWindowHandle);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "GetClientRect", {window, 0}).eax, 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), re2dj::hle::kWin32ErrorNoAccess);

    // GetAsyncKeyState: every key up, the last error untouched inside 0..255
    // and ERROR_INVALID_PARAMETER outside it.
    services.SetLastError(12345);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "GetAsyncKeyState", {9}).eax, 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), 12345U);
    CallModuleExport(context, services, descriptor, "GetAsyncKeyState", {0x10009U});
    RE2DJ_CHECK_EQ(context, services.LastError(), re2dj::hle::kWin32ErrorInvalidParameter);

    // With a host, a held key reads 0x8000 and the pointer over the guest
    // window is its client position on the screen.
    re2dj::test::InputPresentation host;
    services.presentation = &host;
    host.input.virtual_keys.set(9);
    services.SetLastError(12345);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "GetAsyncKeyState", {9}).eax, 0x8000U);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "GetAsyncKeyState", {10}).eax, 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), 12345U);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "GetAsyncKeyState", {0x10009U}).eax, 0U);
    // GetKeyState: a held key is 0x0000FF80 in EAX, as Windows 11 returns it
    // (task 431); a key up, or a code above 0xFF, is 0 with the last error
    // left alone.
    services.SetLastError(12345);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "GetKeyState", {9}).eax, 0x0000FF80U);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "GetKeyState", {0x91}).eax, 0U);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "GetKeyState", {0x1FF}).eax, 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), 12345U);
    host.input.cursor_window = window;
    host.input.cursor_x = 100;
    host.input.cursor_y = 50;
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "GetCursorPos", {kPoint}).eax, 1U);
    if (created != nullptr)
    {
        RE2DJ_CHECK_EQ(context, services.U32(kPoint), static_cast<std::uint32_t>(created->x + created->client_left + 100));
        RE2DJ_CHECK_EQ(context, services.U32(kPoint + 4), static_cast<std::uint32_t>(created->y + created->client_top + 50));
    }
    services.presentation = nullptr;

    // UpdateWindow sends WM_PAINT for the update region once; DefWindowProcA
    // validates it, so the second call sends nothing.
    services.guest_calls.clear();
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "UpdateWindow", {window}).eax, 1U);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "UpdateWindow", {window}).eax, 1U);
    RE2DJ_CHECK_EQ(context, services.guest_calls.size(), std::size_t{1});
    if (services.guest_calls.size() == 1)
    {
        RE2DJ_CHECK_EQ(context, services.guest_calls[0][1], 0x0FU);
    }

    // Showing an already visible window again sends nothing and gives 24.
    services.guest_calls.clear();
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "ShowWindow", {window, 5}).eax, 24U);
    RE2DJ_CHECK(context, services.guest_calls.empty());

    // Shapes outside the model stop instead of guessing: a caption, a child,
    // and an unregistered class.
    bool handled = true;
    CallModuleExport(context, services, descriptor, "CreateWindowExA",
                     {0, kName, kName, 0x00CF0000U, 0, 0, 640, 480, 0, 0, 0, 0}, &handled);
    RE2DJ_CHECK(context, !handled);
    CallModuleExport(context, services, descriptor, "CreateWindowExA",
                     {0, kName, kName, 0xC0000000U, 0, 0, 640, 480, window, 0, 0, 0}, &handled);
    RE2DJ_CHECK(context, !handled);
    services.Put(kName, "OTHER");
    CallModuleExport(context, services, descriptor, "CreateWindowExA",
                     {0, kName, kName, 0x80000000U, 0, 0, 640, 480, 0, 0, 0, 0}, &handled);
    RE2DJ_CHECK(context, !handled);
    // DefWindowProcA stops on a message it has no model of.
    CallModuleExport(context, services, descriptor, "DefWindowProcA", {window, 0x0010, 0, 0}, &handled);
    RE2DJ_CHECK(context, !handled);
}

// EZ2Dancer 2nd MOVE's WS_POPUP | WS_VISIBLE | WS_BORDER window gets the same
// messages as 4th's, with the client area one pixel in on each side, as
// measured on Windows 11 (design 427).
void CheckCreateBorderedWindow(re2dj::test::Context& context)
{
    using re2dj::test::CallModuleExport;
    using re2dj::test::MemoryServices;
    const auto descriptor = re2dj::hle::modules::MakeUser32ModuleDescriptor();
    MemoryServices services;
    services.Process()->SetMainImage(0x00400000U, "D:\\ez2dancer\\EZ2Dancer.exe");
    constexpr std::uint32_t kName = MemoryServices::kBase + 0x40;
    constexpr std::uint32_t kClass = MemoryServices::kBase + 0x60;
    services.Put(kName, "EZ2Dancer");
    const std::uint32_t words[10] = {0x23, 0x00406BAAU, 0, 0, 0, 0, 0, 0x00900011U, 0, kName};
    for (std::uint32_t index = 0; index < 10; ++index)
    {
        services.PutU32(kClass + index * 4, words[index]);
    }
    CallModuleExport(context, services, descriptor, "RegisterClassA", {kClass});
    services.guest_function = [&](const std::vector<std::uint32_t>& arguments) {
        return CallModuleExport(context, services, descriptor, "DefWindowProcA",
                                {arguments[0], arguments[1], arguments[2], arguments[3]})
            .eax;
    };
    const std::uint32_t window = CallModuleExport(context, services, descriptor, "CreateWindowExA",
                                                  {0x40000, kName, kName, 0x90800000U, 0, 0, 640, 480, 0, 0,
                                                   0x00400000U, 0})
                                     .eax;
    RE2DJ_CHECK(context, window != 0);
    const std::vector<std::uint32_t> expected = {0x81, 0x83, 0x01, 0x05, 0x03, 0x18, 0x46, 0x46,
                                                 0x1C, 0x86, 0x06, 0x07, 0x85, 0x14, 0x47};
    std::vector<std::uint32_t> messages;
    for (const auto& call : services.guest_calls)
    {
        messages.push_back(call[1]);
    }
    RE2DJ_CHECK(context, messages == expected);
    if (services.guest_calls.size() == expected.size())
    {
        RE2DJ_CHECK_EQ(context, services.guest_calls[3][3], 0x01DE027EU);
        RE2DJ_CHECK_EQ(context, services.guest_calls[4][3], 0x00010001U);
    }
    const re2dj::hle::GuestWindow* created = services.Process()->user().LookupWindow(window);
    RE2DJ_CHECK(context, created != nullptr);
    if (created != nullptr)
    {
        RE2DJ_CHECK_EQ(context, created->style, 0x94800000U);
        RE2DJ_CHECK_EQ(context, created->client_left, 1);
        RE2DJ_CHECK_EQ(context, created->client_top, 1);
        RE2DJ_CHECK_EQ(context, created->client_right, 639);
        RE2DJ_CHECK_EQ(context, created->client_bottom, 479);
    }
    // WS_DLGFRAME, and so a caption, is still outside the model.
    bool handled = true;
    CallModuleExport(context, services, descriptor, "CreateWindowExA",
                     {0x40000, kName, kName, 0x90400000U, 0, 0, 640, 480, 0, 0, 0x00400000U, 0}, &handled);
    RE2DJ_CHECK(context, !handled);
}

// GetStockObject gives the handles Windows 11 gives a 32-bit process, and
// NULL for the unused index 9 and indices past DC_PEN.
void CheckStockObjects(re2dj::test::Context& context)
{
    using re2dj::test::CallModuleExport;
    const auto descriptor = re2dj::hle::modules::MakeGdi32ModuleDescriptor();
    re2dj::test::MemoryServices services;
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "GetStockObject", {4}).eax, 0x00900011U);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "GetStockObject", {0}).eax, 0x00900010U);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "GetStockObject", {9}).eax, 0U);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "GetStockObject", {20}).eax, 0U);
    RE2DJ_CHECK_EQ(context, descriptor.exports.size(), std::size_t{18});
}

// ShowCursor counts from 0 as Windows 11 does with a mouse installed.
void CheckShowCursor(re2dj::test::Context& context)
{
    using re2dj::test::CallModuleExport;
    const auto descriptor = re2dj::hle::modules::MakeUser32ModuleDescriptor();
    re2dj::test::MemoryServices services;
    std::vector<std::uint32_t> counts;
    for (const std::uint32_t show : {0U, 0U, 0U, 1U})
    {
        counts.push_back(CallModuleExport(context, services, descriptor, "ShowCursor", {show}).eax);
    }
    const std::vector<std::uint32_t> expected = {0xFFFFFFFFU, 0xFFFFFFFEU, 0xFFFFFFFDU, 0xFFFFFFFEU};
    RE2DJ_CHECK(context, counts == expected);
}

// SetRect stores the coordinates as given, reversed or not, and answers
// FALSE for NULL (measured on Windows 11).
void CheckSetRect(re2dj::test::Context& context)
{
    using re2dj::test::CallModuleExport;
    using re2dj::test::MemoryServices;
    const auto descriptor = re2dj::hle::modules::MakeUser32ModuleDescriptor();
    MemoryServices services;
    constexpr std::uint32_t kRect = MemoryServices::kBase + 0x40;
    services.SetLastError(0x1234);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "SetRect", {kRect, 0, 0, 640, 480}).eax,
                   1U);
    RE2DJ_CHECK_EQ(context, services.U32(kRect + 8), 640U);
    RE2DJ_CHECK_EQ(context, services.U32(kRect + 12), 480U);
    CallModuleExport(context, services, descriptor, "SetRect", {kRect, 5, 6, 1, 2});
    RE2DJ_CHECK_EQ(context, services.U32(kRect), 5U);
    RE2DJ_CHECK_EQ(context, services.U32(kRect + 8), 1U);
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "SetRect", {0, 1, 2, 3, 4}).eax, 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), 0x1234U);
}

// Formats with the shared wsprintfA core from words and strings (address 1
// is "abc", 2 "ab", 3 "xyz", 4 "narrow").
std::string Wsprintf(std::string_view format, std::initializer_list<std::uint32_t> words, bool* formatted = nullptr)
{
    const std::vector<std::uint32_t> values(words);
    std::size_t next = 0;
    const re2dj::hle::WsprintfWordReader next_word = [&](std::uint32_t* word) {
        if (next >= values.size())
        {
            return false;
        }
        *word = values[next++];
        return true;
    };
    const re2dj::hle::WsprintfStringReader read_string = [](std::uint32_t address, std::string* text) {
        static const std::array<const char*, 5> texts = {"", "abc", "ab", "xyz", "narrow"};
        if (address >= texts.size())
        {
            return false;
        }
        *text = texts[address];
        return true;
    };
    std::string output;
    std::string error;
    const bool ok = re2dj::hle::FormatWsprintf(format, next_word, read_string, &output, &error);
    if (formatted != nullptr)
    {
        *formatted = ok;
    }
    return ok ? output : "<" + error + ">";
}

// The shared wsprintfA core against the Windows 11 measurements (design 419).
void CheckWsprintfFormat(re2dj::test::Context& context)
{
    const auto check = [&](std::string_view format, std::initializer_list<std::uint32_t> words,
                           std::string_view expected) {
        RE2DJ_CHECK_EQ(context, Wsprintf(format, words), std::string(expected));
    };
    check("%d|%d|%d", {7, 0xFFFFFFF9U, 0}, "7|-7|0");
    check("%02d|%02d|%02d|%02d", {5, 123, 0xFFFFFFFBU, 0}, "05|123|-5|00");
    check("%6d|%6d|%-6d|%06d", {42, 0xFFFFFFD6U, 42, 0xFFFFFFD6U}, "    42|   -42|42    |-00042");
    check("%s|%s|[%s]", {1, 0, 0}, "abc||[]");
    check("%5s|%-5s|%.2s", {2, 2, 1}, "   ab|ab   |ab");
    check("%u|%x|%X|%#x|%#X", {0xFFFFFFFFU, 255, 255, 255, 255}, "4294967295|ff|FF|0xff|0XFF");
    check("%c%c|%5c|%-3c|%03c|", {'A', 'b', 'x', 'y', 'q'}, "Ab|    x|y  |00q|");
    check("100%%|%-3%|%3%|", {}, "100%|%|%|");
    check("%ld|%lu|%lx|%hd|%hi", {0xFFFFFFFDU, 3, 0xab, 0x12345, 0x18000}, "-3|3|ab|9029|-32768");
    check("%hu|%hx", {0x12345, 0x12345}, "74565|12345");
    check("%.3d|%5.3d|%.3d|%6.3d|%-6.3d|%06.3d|", {7, 7, 0xFFFFFFF9U, 0xFFFFFFF9U, 0xFFFFFFF9U, 7},
          "007|  007|-007|  -007|-007  |   007|");
    check("[%.0d]|%.d|[%.s]", {0, 7, 2}, "[0]|7|[]");
    // Not a flag, not a type: the character itself, taking no argument.
    check("%+d|% d|a%qb|%q%d|%*d%d|%5q", {5, 5}, "+d| d|aqb|q5|*d5|q");
    check("%0#x|%#05x|%-#6x|%#-6x|%-06x|%#6x|%#x|%#.3x", {255, 255, 255, 255, 255, 0, 5},
          "#x|0x000ff|0xff    |0xff    |ff    |    0xff|0x0|0x005");
    check("%-0 3d|%0-3d|%lld|%--5d|%#-#5x|", {7, 255}, " 3d|-3d|ld|7    |0xff   |");
    check("%p|%10p|%.2p|%#p|%p", {0x1234, 0xab, 0xab, 0xab, 0xabcd},
          "00001234|  000000AB|AB|0X000000AB|0000ABCD");
    check("%05s|%-05d|%04.1s|%.10s|%3.1s", {2, 7, 3, 1, 3}, "000ab|7    |000x|abc|  x");
    check("%u|%x|%05u|%.2x", {0, 0, 12, 1}, "0|0|00012|01");
    check("%I64d|%d", {5, 0, 9}, "5|9");
    check("%I64u|%I64x|%I64X|%I64d", {0xFFFFFFFFU, 0xFF, 0xBCDEF12U, 1, 0xBCDEF12U, 1, 0xFFFFFFFEU, 0xFFFFFFFFU},
          "1099511627775|10bcdef12|10BCDEF12|-2");
    check("%#d|%#o|%o|%d", {5, 3}, "5|o|o|3");
    check("%hs|%hc|%lu", {4, 'z', 4}, "narrow|z|4");
    check("abc%", {}, "abc");
    check("%.d|%.s|%5", {7, 2}, "7||");
    // The output stops at 1024 characters.
    const std::string long_format(1999, 'a');
    RE2DJ_CHECK_EQ(context, Wsprintf(long_format, {}).size(), std::size_t{1024});
    // Wide text is not modelled; running out of arguments fails too.
    bool formatted = true;
    for (const char* format : {"%ls", "%lc", "%ws", "%S", "%C", "%I64s"})
    {
        Wsprintf(format, {1}, &formatted);
        RE2DJ_CHECK(context, !formatted);
    }
    Wsprintf("%d%d", {1}, &formatted);
    RE2DJ_CHECK(context, !formatted);
}

// wsprintfA reads its variadic arguments from the guest stack past lpOut and
// lpFmt, writes the text and terminator, and returns the count.
void CheckWsprintfA(re2dj::test::Context& context)
{
    using re2dj::test::CallModuleExport;
    using re2dj::test::MemoryServices;
    const auto descriptor = re2dj::hle::modules::MakeUser32ModuleDescriptor();
    MemoryServices services;
    constexpr std::uint32_t kArguments = MemoryServices::kBase + 0x2000;
    constexpr std::uint32_t kOut = MemoryServices::kBase + 0x2100;
    constexpr std::uint32_t kFormat = MemoryServices::kBase + 0x2200;
    constexpr std::uint32_t kName = MemoryServices::kBase + 0x2300;
    services.Put(kFormat, std::string_view("Songs\\%s\\ez|%02d", 17));
    services.Put(kName, std::string_view("tr01", 5));
    services.PutU32(kArguments, kOut);
    services.PutU32(kArguments + 4, kFormat);
    services.PutU32(kArguments + 8, kName);
    services.PutU32(kArguments + 12, 3);
    services.Byte(kOut + 17) = 0xEE;
    services.SetLastError(0x1234);
    bool handled = true;
    // Without the stack arguments the call cannot be answered.
    CallModuleExport(context, services, descriptor, "wsprintfA", {kOut, kFormat}, &handled);
    RE2DJ_CHECK(context, !handled);
    services.arguments_address = kArguments;
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "wsprintfA", {kOut, kFormat}).eax, 16U);
    std::string text;
    for (std::uint32_t at = kOut; services.Byte(at) != 0; ++at)
    {
        text.push_back(static_cast<char>(services.Byte(at)));
    }
    RE2DJ_CHECK_EQ(context, text, std::string("Songs\\tr01\\ez|03"));
    RE2DJ_CHECK_EQ(context, services.Byte(kOut + 17), std::uint8_t{0xEE});
    RE2DJ_CHECK_EQ(context, services.LastError(), 0x1234U);
    handled = true;
    CallModuleExport(context, services, descriptor, "wsprintfA", {0, kFormat}, &handled);
    RE2DJ_CHECK(context, !handled);
}

}  // namespace

void RunUser32ModuleTests(re2dj::test::Context& context)
{
    CheckDescriptor(context);
    CheckMessageBox(context);
    CheckResolveOnlyCursors(context);
    CheckThreadTimer(context);
    CheckSystemImages(context);
    CheckRegisterClass(context);
    CheckCreateWindow(context);
    CheckCreateBorderedWindow(context);
    CheckShowWindow(context);
    CheckEnumDisplaySettings(context);
    CheckStockObjects(context);
    CheckShowCursor(context);
    CheckSetRect(context);
    CheckWsprintfFormat(context);
    CheckWsprintfA(context);
}
