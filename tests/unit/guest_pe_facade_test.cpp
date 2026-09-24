#include "re2dj/hle/modules/guest_pe_facade.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "re2dj/exe/pe_image.h"
#include "test_support.h"

namespace
{

using re2dj::hle::CallingConvention;
using re2dj::hle::ImportCall;
using re2dj::hle::ImportReturn;
using re2dj::hle::modules::GuestExportDescriptor;
using re2dj::hle::modules::GuestModuleDescriptor;
using re2dj::hle::modules::GuestPeFacadeBuildOptions;
using re2dj::hle::modules::GuestPeFacadeBuilder;
using re2dj::hle::modules::GuestPeFacadeImage;
using re2dj::runtime::GuestAddress;

bool StubHandler(const ImportCall&, ImportReturn*, std::string*)
{
    return true;
}

GuestExportDescriptor MakeExport(std::string name,
                                 std::optional<std::uint16_t> ordinal)
{
    GuestExportDescriptor descriptor;
    descriptor.name = std::move(name);
    descriptor.ordinal = ordinal;
    descriptor.calling_convention = CallingConvention::kStdcall;
    descriptor.handler = &StubHandler;
    return descriptor;
}

GuestModuleDescriptor MakeDescriptor()
{
    GuestModuleDescriptor descriptor;
    descriptor.name = "kernel32.dll";
    descriptor.aliases = {"kernel32"};
    descriptor.exports.push_back(MakeExport("Zulu", std::uint16_t{5}));
    descriptor.exports.push_back(MakeExport("Alpha", std::nullopt));
    descriptor.exports.push_back(MakeExport("", std::uint16_t{10}));
    descriptor.exports.push_back(MakeExport("Middle", std::nullopt));
    return descriptor;
}

std::uint16_t ReadU16(const std::vector<std::uint8_t>& bytes, std::size_t offset)
{
    return static_cast<std::uint16_t>(bytes[offset]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1]) << 8);
}

std::uint32_t ReadU32(const std::vector<std::uint8_t>& bytes, std::size_t offset)
{
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

std::size_t RvaToOffset(const std::vector<std::uint8_t>& bytes, std::uint32_t rva)
{
    const std::uint32_t pe_offset = ReadU32(bytes, 0x3C);
    const std::uint16_t section_count = ReadU16(bytes, pe_offset + 6);
    const std::uint16_t optional_size = ReadU16(bytes, pe_offset + 20);
    const std::size_t optional_offset = pe_offset + 24;
    const std::uint32_t header_size = ReadU32(bytes, optional_offset + 60);
    if (rva < header_size)
    {
        return rva;
    }
    const std::size_t section_table = optional_offset + optional_size;
    for (std::uint16_t index = 0; index < section_count; ++index)
    {
        const std::size_t section = section_table + static_cast<std::size_t>(index) * 40;
        const std::uint32_t virtual_size = ReadU32(bytes, section + 8);
        const std::uint32_t virtual_address = ReadU32(bytes, section + 12);
        const std::uint32_t raw_size = ReadU32(bytes, section + 16);
        const std::uint32_t raw_offset = ReadU32(bytes, section + 20);
        const std::uint32_t span = (std::max)(virtual_size, raw_size);
        if (rva >= virtual_address && rva - virtual_address < span)
        {
            return raw_offset + (rva - virtual_address);
        }
    }
    return bytes.size();
}

std::string ReadString(const std::vector<std::uint8_t>& bytes, std::uint32_t rva)
{
    std::string value;
    std::size_t offset = RvaToOffset(bytes, rva);
    while (offset < bytes.size() && bytes[offset] != 0)
    {
        value.push_back(static_cast<char>(bytes[offset]));
        ++offset;
    }
    return value;
}

void CheckThunk(re2dj::test::Context& context,
                const GuestPeFacadeImage& image,
                std::size_t export_index,
                GuestAddress gate,
                GuestAddress bridge,
                GuestAddress cleanup)
{
    const std::uint32_t thunk_rva = image.export_thunk_rvas[export_index];
    const std::size_t thunk = RvaToOffset(image.file_bytes, thunk_rva);
    RE2DJ_CHECK(context, thunk + 19 <= image.file_bytes.size());
    if (thunk + 19 > image.file_bytes.size())
    {
        return;
    }
    RE2DJ_CHECK_EQ(context, image.file_bytes[thunk], std::uint8_t{0x68});
    RE2DJ_CHECK_EQ(context, ReadU32(image.file_bytes, thunk + 1), gate.value());
    RE2DJ_CHECK_EQ(context, image.file_bytes[thunk + 5], std::uint8_t{0xE8});
    const std::uint32_t call_target = image.preferred_base.value() + thunk_rva + 10U +
                                      ReadU32(image.file_bytes, thunk + 6);
    RE2DJ_CHECK_EQ(context, call_target, bridge.value());
    RE2DJ_CHECK_EQ(context, image.file_bytes[thunk + 10], std::uint8_t{0x59});
    RE2DJ_CHECK_EQ(context, image.file_bytes[thunk + 11], std::uint8_t{0x03});
    RE2DJ_CHECK_EQ(context, image.file_bytes[thunk + 12], std::uint8_t{0x25});
    RE2DJ_CHECK_EQ(context, ReadU32(image.file_bytes, thunk + 13), cleanup.value());
    RE2DJ_CHECK_EQ(context, image.file_bytes[thunk + 17], std::uint8_t{0xFF});
    RE2DJ_CHECK_EQ(context, image.file_bytes[thunk + 18], std::uint8_t{0xE1});
}

void CheckValidFacade(re2dj::test::Context& context)
{
    const GuestModuleDescriptor descriptor = MakeDescriptor();
    const std::array<GuestAddress, 4> gates = {
        GuestAddress(0x6F000010U),
        GuestAddress(0x6F000020U),
        GuestAddress(0x6F000030U),
        GuestAddress(0x6F000040U),
    };
    const GuestPeFacadeBuildOptions options = {
        GuestAddress(0x70000000U),
        GuestAddress(0x10002000U),
        GuestAddress(0x10003000U),
        std::span<const GuestAddress>(gates),
    };
    GuestPeFacadeImage image;
    std::string error = "stale";
    RE2DJ_CHECK(context, GuestPeFacadeBuilder::Build(descriptor, options, &image, &error));
    RE2DJ_CHECK(context, error.empty());
    RE2DJ_CHECK_EQ(context, image.preferred_base, options.image_base);
    RE2DJ_CHECK_EQ(context, image.export_thunk_rvas.size(), std::size_t{4});
    RE2DJ_CHECK_EQ(context, image.export_ordinals.size(), std::size_t{4});
    if (image.export_ordinals.size() == 4)
    {
        RE2DJ_CHECK_EQ(context, image.export_ordinals[0], std::uint16_t{5});
        RE2DJ_CHECK_EQ(context, image.export_ordinals[1], std::uint16_t{1});
        RE2DJ_CHECK_EQ(context, image.export_ordinals[2], std::uint16_t{10});
        RE2DJ_CHECK_EQ(context, image.export_ordinals[3], std::uint16_t{2});
    }

    re2dj::exe::PeImageInfo info;
    RE2DJ_CHECK(context,
                re2dj::exe::ReadPeImageInfo(image.file_bytes.data(),
                                            image.file_bytes.size(),
                                            &info,
                                            &error));
    RE2DJ_CHECK(context, info.magic == re2dj::exe::PeMagic::kPe32);
    RE2DJ_CHECK_EQ(context, info.machine, re2dj::exe::kMachineI386);
    RE2DJ_CHECK(context, info.is_dll);
    RE2DJ_CHECK_EQ(context, info.image_base, std::uint64_t{0x70000000U});
    RE2DJ_CHECK_EQ(context, info.entry_point_rva, std::uint32_t{0});
    RE2DJ_CHECK_EQ(context, info.section_alignment, std::uint32_t{0x1000});
    RE2DJ_CHECK_EQ(context, info.file_alignment, std::uint32_t{0x200});
    RE2DJ_CHECK_EQ(context, info.size_of_image, image.image_size);
    RE2DJ_CHECK_EQ(context, info.sections.size(), std::size_t{2});
    if (info.sections.size() == 2)
    {
        RE2DJ_CHECK_EQ(context, info.sections[0].name, std::string(".edata"));
        RE2DJ_CHECK_EQ(context, info.sections[0].characteristics, std::uint32_t{0x40000040});
        RE2DJ_CHECK_EQ(context, info.sections[1].name, std::string(".text"));
        RE2DJ_CHECK_EQ(context, info.sections[1].characteristics, std::uint32_t{0x60000020});
    }

    const std::uint32_t pe_offset = ReadU32(image.file_bytes, 0x3C);
    const std::size_t optional = pe_offset + 24;
    const std::uint32_t export_rva = ReadU32(image.file_bytes, optional + 96);
    const std::uint32_t export_size = ReadU32(image.file_bytes, optional + 100);
    RE2DJ_CHECK(context, export_rva != 0);
    RE2DJ_CHECK(context, export_size >= 40);
    const std::size_t export_directory = RvaToOffset(image.file_bytes, export_rva);
    RE2DJ_CHECK_EQ(context,
                   ReadString(image.file_bytes,
                              ReadU32(image.file_bytes, export_directory + 12)),
                   std::string("kernel32.dll"));
    RE2DJ_CHECK_EQ(context, ReadU32(image.file_bytes, export_directory + 16), std::uint32_t{1});
    RE2DJ_CHECK_EQ(context, ReadU32(image.file_bytes, export_directory + 20), std::uint32_t{10});
    RE2DJ_CHECK_EQ(context, ReadU32(image.file_bytes, export_directory + 24), std::uint32_t{3});

    const std::uint32_t eat_rva = ReadU32(image.file_bytes, export_directory + 28);
    const std::uint32_t names_rva = ReadU32(image.file_bytes, export_directory + 32);
    const std::uint32_t ordinals_rva = ReadU32(image.file_bytes, export_directory + 36);
    const std::size_t eat = RvaToOffset(image.file_bytes, eat_rva);
    const std::size_t names = RvaToOffset(image.file_bytes, names_rva);
    const std::size_t ordinals = RvaToOffset(image.file_bytes, ordinals_rva);
    RE2DJ_CHECK_EQ(context, ReadU32(image.file_bytes, eat + 0), image.export_thunk_rvas[1]);
    RE2DJ_CHECK_EQ(context, ReadU32(image.file_bytes, eat + 4), image.export_thunk_rvas[3]);
    RE2DJ_CHECK_EQ(context, ReadU32(image.file_bytes, eat + 8), std::uint32_t{0});
    RE2DJ_CHECK_EQ(context, ReadU32(image.file_bytes, eat + 16), image.export_thunk_rvas[0]);
    RE2DJ_CHECK_EQ(context, ReadU32(image.file_bytes, eat + 36), image.export_thunk_rvas[2]);

    const std::array<std::string_view, 3> expected_names = {"Alpha", "Middle", "Zulu"};
    const std::array<std::uint16_t, 3> expected_ordinal_indices = {0, 1, 4};
    for (std::size_t index = 0; index < expected_names.size(); ++index)
    {
        RE2DJ_CHECK_EQ(context,
                       ReadString(image.file_bytes,
                                  ReadU32(image.file_bytes, names + index * 4)),
                       std::string(expected_names[index]));
        RE2DJ_CHECK_EQ(context,
                       ReadU16(image.file_bytes, ordinals + index * 2),
                       expected_ordinal_indices[index]);
    }

    for (std::size_t index = 0; index < gates.size(); ++index)
    {
        CheckThunk(context,
                   image,
                   index,
                   gates[index],
                   options.bridge_address,
                   options.cleanup_address);
    }
}

void CheckRejections(re2dj::test::Context& context)
{
    GuestModuleDescriptor descriptor = MakeDescriptor();
    std::array<GuestAddress, 4> gates = {
        GuestAddress(0x6F000010U),
        GuestAddress(0x6F000020U),
        GuestAddress(0x6F000030U),
        GuestAddress(0x6F000040U),
    };
    GuestPeFacadeBuildOptions options = {
        GuestAddress(0x70000000U),
        GuestAddress(0x10002000U),
        GuestAddress(0x10003000U),
        std::span<const GuestAddress>(gates),
    };
    GuestPeFacadeImage image;
    image.file_bytes = {1, 2, 3};
    image.preferred_base = GuestAddress(0x12340000U);
    image.image_size = 7;

    const auto check_unchanged = [&context, &image]()
    {
        RE2DJ_CHECK_EQ(context, image.file_bytes.size(), std::size_t{3});
        RE2DJ_CHECK_EQ(context, image.preferred_base, GuestAddress(0x12340000U));
        RE2DJ_CHECK_EQ(context, image.image_size, std::uint32_t{7});
    };

    std::string error;
    GuestModuleDescriptor no_exports = descriptor;
    no_exports.exports.clear();
    RE2DJ_CHECK(context,
                !GuestPeFacadeBuilder::Build(no_exports, options, &image, &error));
    RE2DJ_CHECK(context, !error.empty());
    check_unchanged();

    GuestModuleDescriptor embedded_null = descriptor;
    embedded_null.exports[0].name = std::string("Bad\0Name", 8);
    RE2DJ_CHECK(context,
                !GuestPeFacadeBuilder::Build(embedded_null, options, &image, &error));
    check_unchanged();

    GuestModuleDescriptor duplicate_ordinal = descriptor;
    duplicate_ordinal.exports[1].ordinal = std::uint16_t{5};
    RE2DJ_CHECK(context,
                !GuestPeFacadeBuilder::Build(duplicate_ordinal, options, &image, &error));
    check_unchanged();

    GuestPeFacadeBuildOptions unaligned = options;
    unaligned.image_base = GuestAddress(0x70001000U);
    RE2DJ_CHECK(context,
                !GuestPeFacadeBuilder::Build(descriptor, unaligned, &image, &error));
    check_unchanged();

    GuestPeFacadeBuildOptions missing_gate = options;
    missing_gate.export_gates = std::span<const GuestAddress>(gates.data(), 3);
    RE2DJ_CHECK(context,
                !GuestPeFacadeBuilder::Build(descriptor, missing_gate, &image, &error));
    check_unchanged();

    gates[2] = GuestAddress();
    RE2DJ_CHECK(context,
                !GuestPeFacadeBuilder::Build(descriptor, options, &image, &error));
    check_unchanged();
    gates[2] = GuestAddress(0x6F000030U);

    GuestPeFacadeBuildOptions zero_bridge = options;
    zero_bridge.bridge_address = GuestAddress();
    RE2DJ_CHECK(context,
                !GuestPeFacadeBuilder::Build(descriptor, zero_bridge, &image, &error));
    check_unchanged();

    RE2DJ_CHECK(context,
                !GuestPeFacadeBuilder::Build(descriptor, options, nullptr, &error));
}

}  // namespace

void RunGuestPeFacadeTests(re2dj::test::Context& context)
{
    CheckValidFacade(context);
    CheckRejections(context);
}
