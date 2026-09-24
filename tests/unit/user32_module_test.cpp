#include "re2dj/hle/modules/user32_module.h"

#include <array>
#include <cstdint>
#include <span>
#include <string>

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
    RE2DJ_CHECK_EQ(context, descriptor.exports.size(), std::size_t{2});
    if (descriptor.exports.size() != 2)
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
    // No window-creating export exists, so no guest window can be active.
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

}  // namespace

void RunUser32ModuleTests(re2dj::test::Context& context)
{
    CheckDescriptor(context);
    CheckMessageBox(context);
}
