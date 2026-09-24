#include "re2dj/hle/modules/guest_module_registry.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "test_support.h"

namespace
{

using re2dj::hle::CallingConvention;
using re2dj::hle::ImportCall;
using re2dj::hle::ImportReturn;
using re2dj::hle::modules::GuestExportDescriptor;
using re2dj::hle::modules::GuestModuleDescriptor;
using re2dj::hle::modules::GuestModuleMapping;
using re2dj::hle::modules::GuestModuleRegistry;
using re2dj::runtime::GuestAddress;

bool StubHandler(const ImportCall&, ImportReturn*, std::string*)
{
    return true;
}

GuestExportDescriptor MakeExport(std::string name,
                                 std::optional<std::uint16_t> ordinal,
                                 std::uint32_t argument_count = 0)
{
    GuestExportDescriptor descriptor;
    descriptor.name = std::move(name);
    descriptor.ordinal = ordinal;
    descriptor.calling_convention = CallingConvention::kStdcall;
    descriptor.argument_count = argument_count;
    descriptor.handler = &StubHandler;
    return descriptor;
}

GuestModuleDescriptor MakeKernel32Descriptor()
{
    GuestModuleDescriptor descriptor;
    descriptor.name = "kernel32.dll";
    descriptor.aliases = {"kernel32"};
    descriptor.exports.push_back(MakeExport("GetVersion", std::uint16_t{1}));
    descriptor.exports.push_back(MakeExport("CreateFileA", std::uint16_t{2}, 7));
    descriptor.exports.push_back(MakeExport("NameOnly", std::nullopt));
    descriptor.exports.push_back(MakeExport("", std::uint16_t{4}));
    return descriptor;
}

GuestModuleMapping MakeMapping(std::uint32_t base = 0x70000000U)
{
    GuestModuleMapping mapping;
    mapping.base = GuestAddress(base);
    mapping.image_size = 0x2000U;
    mapping.export_thunks = {
        GuestAddress(base + 0x1000U),
        GuestAddress(base + 0x1010U),
        GuestAddress(base + 0x1020U),
        GuestAddress(base + 0x1030U),
    };
    return mapping;
}

void CheckLookup(re2dj::test::Context& context)
{
    GuestModuleRegistry registry;
    std::string error = "stale";
    RE2DJ_CHECK(context,
                registry.Register(MakeKernel32Descriptor(), MakeMapping(), &error));
    RE2DJ_CHECK(context, error.empty());
    RE2DJ_CHECK_EQ(context, registry.module_count(), std::size_t{1});

    const auto* canonical = registry.FindModule("kernel32.dll");
    const auto* alias = registry.FindModule("KERNEL32");
    const auto* mixed_case = registry.FindModule("KeRnEl32.DlL");
    const auto* handle = registry.FindModule(GuestAddress(0x70000000U));
    RE2DJ_CHECK(context, canonical != nullptr);
    RE2DJ_CHECK(context, canonical == alias);
    RE2DJ_CHECK(context, canonical == mixed_case);
    RE2DJ_CHECK(context, canonical == handle);
    RE2DJ_CHECK(context, registry.FindModule("kernelbase") == nullptr);
    RE2DJ_CHECK(context, registry.FindModule(GuestAddress()) == nullptr);
    RE2DJ_CHECK(context, registry.FindModule(GuestAddress(0x70000001U)) == nullptr);

    const auto* by_name = registry.FindExport(GuestAddress(0x70000000U), "CreateFileA");
    const auto* by_ordinal = registry.FindExport(GuestAddress(0x70000000U), 2);
    RE2DJ_CHECK(context, by_name != nullptr);
    RE2DJ_CHECK(context, by_name == by_ordinal);
    RE2DJ_CHECK_EQ(context, by_name->thunk_address, GuestAddress(0x70001010U));
    RE2DJ_CHECK_EQ(context, by_name->descriptor.argument_count, std::uint32_t{7});
    RE2DJ_CHECK(context, by_name->descriptor.handler == &StubHandler);
    RE2DJ_CHECK(context,
                registry.FindExport(GuestAddress(0x70000000U), "createfilea") == nullptr);
    RE2DJ_CHECK(context,
                registry.FindExport(GuestAddress(0x70000000U), "NameOnly") != nullptr);
    RE2DJ_CHECK(context, registry.FindExport(GuestAddress(0x70000000U), 4) != nullptr);
    RE2DJ_CHECK(context, registry.FindExport(GuestAddress(0x70000000U), 3) == nullptr);
    RE2DJ_CHECK(context, registry.FindExport(GuestAddress(0x70000000U), 0) == nullptr);
    RE2DJ_CHECK(context,
                registry.FindExport(GuestAddress(0x71000000U), "CreateFileA") == nullptr);

    re2dj::runtime::ImportGate static_gate;
    static_gate.module = "KERNEL32.dll";
    static_gate.name = "CreateFileA";
    const auto* static_export = registry.FindExport(static_gate);
    RE2DJ_CHECK(context, static_export == by_name);
    if (static_export != nullptr && by_name != nullptr)
    {
        RE2DJ_CHECK_EQ(context, static_export->thunk_address, by_name->thunk_address);
    }

    const auto* stable = canonical;
    GuestModuleDescriptor user32;
    user32.name = "user32.dll";
    user32.aliases = {"user32"};
    user32.exports.push_back(MakeExport("MessageBoxA", std::uint16_t{1}, 4));
    GuestModuleMapping user_mapping;
    user_mapping.base = GuestAddress(0x71000000U);
    user_mapping.image_size = 0x2000U;
    user_mapping.export_thunks = {GuestAddress(0x71001000U)};
    RE2DJ_CHECK(context, registry.Register(std::move(user32), std::move(user_mapping), &error));
    RE2DJ_CHECK(context, stable == registry.FindModule("kernel32"));
    RE2DJ_CHECK_EQ(context, stable->name, std::string("kernel32.dll"));
}

void CheckDescriptorRejections(re2dj::test::Context& context)
{
    const auto expect_rejected = [&context](GuestModuleDescriptor descriptor)
    {
        GuestModuleRegistry registry;
        std::string error;
        RE2DJ_CHECK(context, !registry.Register(std::move(descriptor), MakeMapping(), &error));
        RE2DJ_CHECK(context, !error.empty());
        RE2DJ_CHECK_EQ(context, registry.module_count(), std::size_t{0});
    };

    GuestModuleDescriptor empty_name = MakeKernel32Descriptor();
    empty_name.name.clear();
    expect_rejected(std::move(empty_name));

    GuestModuleDescriptor no_exports = MakeKernel32Descriptor();
    no_exports.exports.clear();
    expect_rejected(std::move(no_exports));

    GuestModuleDescriptor empty_alias = MakeKernel32Descriptor();
    empty_alias.aliases.push_back("");
    expect_rejected(std::move(empty_alias));

    GuestModuleDescriptor duplicate_alias = MakeKernel32Descriptor();
    duplicate_alias.aliases.push_back("KERNEL32.DLL");
    expect_rejected(std::move(duplicate_alias));

    GuestModuleDescriptor nameless_export = MakeKernel32Descriptor();
    nameless_export.exports.front().name.clear();
    nameless_export.exports.front().ordinal.reset();
    expect_rejected(std::move(nameless_export));

    GuestModuleDescriptor zero_ordinal = MakeKernel32Descriptor();
    zero_ordinal.exports.front().ordinal = std::uint16_t{0};
    expect_rejected(std::move(zero_ordinal));

    GuestModuleDescriptor duplicate_name = MakeKernel32Descriptor();
    duplicate_name.exports.push_back(MakeExport("GetVersion", std::uint16_t{20}));
    expect_rejected(std::move(duplicate_name));

    GuestModuleDescriptor duplicate_ordinal = MakeKernel32Descriptor();
    duplicate_ordinal.exports.push_back(MakeExport("CloseHandle", std::uint16_t{2}));
    expect_rejected(std::move(duplicate_ordinal));

    GuestModuleDescriptor null_handler = MakeKernel32Descriptor();
    null_handler.exports.front().handler = nullptr;
    expect_rejected(std::move(null_handler));

    GuestModuleDescriptor excessive_arguments = MakeKernel32Descriptor();
    excessive_arguments.exports.front().argument_count =
        re2dj::hle::ImportDispatcher::kMaximumArgumentCount + 1U;
    expect_rejected(std::move(excessive_arguments));

    GuestModuleDescriptor invalid_convention = MakeKernel32Descriptor();
    invalid_convention.exports.front().calling_convention =
        static_cast<CallingConvention>(0xffU);
    expect_rejected(std::move(invalid_convention));
}

void CheckMappingAndRegistryRejections(re2dj::test::Context& context)
{
    const auto expect_mapping_rejected = [&context](GuestModuleMapping mapping)
    {
        GuestModuleRegistry registry;
        std::string error;
        RE2DJ_CHECK(context,
                    !registry.Register(MakeKernel32Descriptor(), std::move(mapping), &error));
        RE2DJ_CHECK(context, !error.empty());
        RE2DJ_CHECK_EQ(context, registry.module_count(), std::size_t{0});
    };

    GuestModuleMapping zero_base = MakeMapping();
    zero_base.base = GuestAddress();
    expect_mapping_rejected(std::move(zero_base));

    GuestModuleMapping zero_size = MakeMapping();
    zero_size.image_size = 0;
    expect_mapping_rejected(std::move(zero_size));

    GuestModuleMapping wrong_count = MakeMapping();
    wrong_count.export_thunks.pop_back();
    expect_mapping_rejected(std::move(wrong_count));

    GuestModuleMapping below_image = MakeMapping();
    below_image.export_thunks.front() = GuestAddress(0x6fffffffU);
    expect_mapping_rejected(std::move(below_image));

    GuestModuleMapping at_image_end = MakeMapping();
    at_image_end.export_thunks.front() = GuestAddress(0x70002000U);
    expect_mapping_rejected(std::move(at_image_end));

    GuestModuleMapping wrapping = MakeMapping(0xfffff000U);
    wrapping.image_size = 0x2000U;
    expect_mapping_rejected(std::move(wrapping));

    GuestModuleRegistry registry;
    std::string error;
    RE2DJ_CHECK(context, registry.Register(MakeKernel32Descriptor(), MakeMapping(), &error));
    const auto* original = registry.FindModule("kernel32");

    GuestModuleDescriptor duplicate_module = MakeKernel32Descriptor();
    duplicate_module.name = "KERNEL32";
    duplicate_module.aliases = {"other-kernel"};
    RE2DJ_CHECK(context,
                !registry.Register(std::move(duplicate_module),
                                   MakeMapping(0x72000000U),
                                   &error));

    GuestModuleDescriptor alias_collision = MakeKernel32Descriptor();
    alias_collision.name = "kernelbase.dll";
    alias_collision.aliases = {"KERNEL32"};
    RE2DJ_CHECK(context,
                !registry.Register(std::move(alias_collision),
                                   MakeMapping(0x72000000U),
                                   &error));

    GuestModuleDescriptor overlap = MakeKernel32Descriptor();
    overlap.name = "advapi32.dll";
    overlap.aliases = {"advapi32"};
    RE2DJ_CHECK(context,
                !registry.Register(std::move(overlap),
                                   MakeMapping(0x70001000U),
                                   &error));

    RE2DJ_CHECK_EQ(context, registry.module_count(), std::size_t{1});
    RE2DJ_CHECK(context, registry.FindModule("kernel32") == original);
}

}  // namespace

void RunGuestModuleRegistryTests(re2dj::test::Context& context)
{
    CheckLookup(context);
    CheckDescriptorRejections(context);
    CheckMappingAndRegistryRejections(context);
}
