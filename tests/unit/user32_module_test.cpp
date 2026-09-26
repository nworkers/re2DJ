#include "re2dj/hle/modules/user32_module.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <utility>

#include "memory_services.h"
#include "re2dj/hle/guest_user.h"
#include "re2dj/hle/modules/gdi32_module.h"
#include "re2dj/hle/win32_errors.h"
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
    // Thirteen implemented exports from GetActiveWindow to GetWindowLongA,
    // then 24 resolve-only exports.
    RE2DJ_CHECK_EQ(context, descriptor.exports.size(), std::size_t{37});
    if (descriptor.exports.size() != 37)
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
        const auto& export_descriptor = descriptor.exports[13 + index];
        RE2DJ_CHECK_EQ(context, export_descriptor.name, cursors[index].first);
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
    RE2DJ_CHECK_EQ(context, descriptor.exports.size(), std::size_t{12});
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
    CheckStockObjects(context);
    CheckShowCursor(context);
    CheckSetRect(context);
}
