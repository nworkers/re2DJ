#include "re2dj/hle/modules/kernel32_module.h"

#include <array>
#include <cstdint>
#include <limits>
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

    const auto descriptor = re2dj::hle::modules::MakeKernel32ModuleDescriptor();
    std::string error = "stale";
    RE2DJ_CHECK(context,
                re2dj::hle::modules::ValidateGuestModuleDescriptor(descriptor, &error));
    RE2DJ_CHECK(context, error.empty());
    RE2DJ_CHECK_EQ(context, descriptor.name, std::string("kernel32.dll"));
    RE2DJ_CHECK_EQ(context, descriptor.aliases.size(), std::size_t{1});
    if (descriptor.aliases.size() == 1)
    {
        RE2DJ_CHECK_EQ(context, descriptor.aliases[0], std::string("kernel32"));
    }
    RE2DJ_CHECK_EQ(context, descriptor.exports.size(), std::size_t{5});
    if (descriptor.exports.size() != 5)
    {
        return;
    }

    const std::array<std::string, 5> names = {
        "GetModuleHandleA", "GetProcAddress", "GetVersion", "CreateFileA", "ExitProcess"};
    const std::array<std::uint32_t, 5> argument_counts = {1, 2, 0, 7, 1};
    for (std::size_t index = 0; index < descriptor.exports.size(); ++index)
    {
        const auto& export_descriptor = descriptor.exports[index];
        RE2DJ_CHECK_EQ(context, export_descriptor.name, names[index]);
        RE2DJ_CHECK(context, !export_descriptor.ordinal.has_value());
        RE2DJ_CHECK(context,
                    export_descriptor.calling_convention == CallingConvention::kStdcall);
        RE2DJ_CHECK_EQ(context, export_descriptor.argument_count, argument_counts[index]);
        RE2DJ_CHECK(context, export_descriptor.handler != nullptr);
    }

    ImportGate gate;
    gate.module = descriptor.name;
    gate.address = GuestAddress(0xF1000000U);
    const std::array<std::uint32_t, 7> arguments = {};
    ImportReturn result;

    for (std::size_t index = 0; index < descriptor.exports.size(); ++index)
    {
        gate.name = descriptor.exports[index].name;
        const ImportCall call{gate,
                              std::span<const std::uint32_t>(
                                  arguments.data(), descriptor.exports[index].argument_count)};
        result = {0x12345678U, 0x87654321U};
        RE2DJ_CHECK(context,
                    descriptor.exports[index].handler(call, &result, &error));
        RE2DJ_CHECK(context, error.empty());
        const std::uint32_t expected =
            index == 3   ? (std::numeric_limits<std::uint32_t>::max)()
            : index == 2 ? re2dj::hle::modules::kKernel32GuestVersion
                         : 0U;
        RE2DJ_CHECK_EQ(context, result.eax, expected);
        RE2DJ_CHECK_EQ(context, result.edx, std::uint32_t{0});
        // Only ExitProcess ends the guest process.
        RE2DJ_CHECK_EQ(context, result.exit_process, index == 4);
    }
}

void CheckExitProcess(re2dj::test::Context& context)
{
    using re2dj::hle::ImportCall;
    using re2dj::hle::ImportReturn;

    const auto descriptor = re2dj::hle::modules::MakeKernel32ModuleDescriptor();
    const re2dj::hle::modules::GuestExportDescriptor* exit_process = nullptr;
    for (const auto& export_descriptor : descriptor.exports)
    {
        if (export_descriptor.name == "ExitProcess")
        {
            exit_process = &export_descriptor;
        }
    }
    RE2DJ_CHECK(context, exit_process != nullptr);
    if (exit_process == nullptr)
    {
        return;
    }

    re2dj::runtime::ImportGate gate;
    gate.module = descriptor.name;
    gate.name = exit_process->name;
    const std::array<std::uint32_t, 1> code = {7};
    ImportReturn result;
    std::string error = "stale";
    RE2DJ_CHECK(context,
                exit_process->handler(ImportCall{gate, code}, &result, &error));
    RE2DJ_CHECK(context, error.empty());
    RE2DJ_CHECK(context, result.exit_process);
    RE2DJ_CHECK_EQ(context, result.exit_code, std::uint32_t{7});

    // The exit code is the only argument; any other shape is rejected.
    const std::array<std::uint32_t, 2> too_many = {7, 8};
    RE2DJ_CHECK(context,
                !exit_process->handler(ImportCall{gate, too_many}, &result, &error));
    RE2DJ_CHECK(context, !error.empty());
}

void CheckGuestVersionEncoding(re2dj::test::Context& context)
{
    // GetVersion packs major/minor in the low word and the build number in the
    // high word, with bit 31 clear on the NT platform.
    constexpr std::uint32_t version = re2dj::hle::modules::kKernel32GuestVersion;
    RE2DJ_CHECK_EQ(context, version & 0xFFU, std::uint32_t{6});
    RE2DJ_CHECK_EQ(context, (version >> 8) & 0xFFU, std::uint32_t{2});
    RE2DJ_CHECK_EQ(context, version >> 16, std::uint32_t{9200});
    RE2DJ_CHECK_EQ(context, version & 0x80000000U, std::uint32_t{0});
}

}  // namespace

void RunKernel32ModuleTests(re2dj::test::Context& context)
{
    CheckDescriptor(context);
    CheckGuestVersionEncoding(context);
    CheckExitProcess(context);
}
