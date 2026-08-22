#include "re2dj/runtime/pe_loader.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "synthetic_pe32.h"
#include "test_support.h"

namespace
{

constexpr std::uint32_t kPreferredBase = 0x00400000;

void PutString(std::vector<std::uint8_t>& bytes, std::size_t offset, const char* text)
{
    for (std::size_t index = 0;; ++index)
    {
        bytes[offset + index] = static_cast<std::uint8_t>(text[index]);
        if (text[index] == '\0')
        {
            return;
        }
    }
}

re2dj::exe::PeImageInfo MakeLoaderInfo()
{
    re2dj::exe::PeImageInfo info;
    info.machine = re2dj::exe::kMachineI386;
    info.magic = re2dj::exe::PeMagic::kPe32;
    info.image_base = kPreferredBase;
    info.entry_point_rva = 0x1000;
    info.size_of_image = 0x5000;
    info.size_of_headers = 0x100;

    re2dj::exe::PeSection text;
    text.name = ".text";
    text.virtual_address = 0x1000;
    text.virtual_size = 0x200;
    text.raw_offset = 0x100;
    text.raw_size = 0x100;
    info.sections.push_back(text);

    re2dj::exe::PeSection imports;
    imports.name = ".idata";
    imports.virtual_address = 0x2000;
    imports.virtual_size = 0x300;
    imports.raw_offset = 0x200;
    imports.raw_size = 0x300;
    info.sections.push_back(imports);

    re2dj::exe::PeSection relocations;
    relocations.name = ".reloc";
    relocations.virtual_address = 0x3000;
    relocations.virtual_size = 0x100;
    relocations.raw_offset = 0x500;
    relocations.raw_size = 0x100;
    info.sections.push_back(relocations);

    info.data_directories.resize(16);
    info.data_directories[static_cast<std::size_t>(re2dj::exe::PeDirectoryIndex::kImport)] =
        {0x2000, 40};
    info.data_directories[
        static_cast<std::size_t>(re2dj::exe::PeDirectoryIndex::kBaseRelocation)] =
        {0x3000, 12};
    info.data_directories[static_cast<std::size_t>(re2dj::exe::PeDirectoryIndex::kTls)] =
        {0x2200, 24};
    return info;
}

std::vector<std::uint8_t> MakeLoaderFile()
{
    std::vector<std::uint8_t> bytes(0x600, 0);

    // Relocation target in .text.
    re2dj::test::PutU32(bytes, 0x100, kPreferredBase + 0x1234);

    // One import descriptor followed by the zero descriptor already present.
    re2dj::test::PutU32(bytes, 0x200, 0x2080);  // OriginalFirstThunk
    re2dj::test::PutU32(bytes, 0x20C, 0x2050);  // Name
    re2dj::test::PutU32(bytes, 0x210, 0x20A0);  // FirstThunk
    PutString(bytes, 0x250, "KERNEL32.dll");
    re2dj::test::PutU32(bytes, 0x280, 0x20C0);
    re2dj::test::PutU32(bytes, 0x284, 0x80000001);
    re2dj::test::PutU16(bytes, 0x2C0, 0);
    PutString(bytes, 0x2C2, "CreateFileA");

    // A HIGHLOW entry for RVA 0x1000 and one ABSOLUTE padding entry.
    re2dj::test::PutU32(bytes, 0x500, 0x1000);
    re2dj::test::PutU32(bytes, 0x504, 12);
    re2dj::test::PutU16(bytes, 0x508, 0x3000);
    re2dj::test::PutU16(bytes, 0x50A, 0x0000);
    return bytes;
}

}  // namespace

void RunPeLoaderTests(re2dj::test::Context& context)
{
    using re2dj::runtime::AddressSpace;
    using re2dj::runtime::GuestAddress;
    using re2dj::runtime::ImportGateTable;
    using re2dj::runtime::LoadedPeImage;

    const re2dj::exe::PeImageInfo info = MakeLoaderInfo();
    const std::vector<std::uint8_t> file = MakeLoaderFile();
    AddressSpace space;
    ImportGateTable gates;
    LoadedPeImage loaded;
    std::string error;
    RE2DJ_CHECK(context,
                re2dj::runtime::LoadPe32Image(file.data(),
                                              file.size(),
                                              info,
                                              GuestAddress(0x00500000),
                                              &space,
                                              &gates,
                                              &loaded,
                                              &error));
    RE2DJ_CHECK_EQ(context, loaded.load_base.value(), std::uint32_t{0x00500000});
    RE2DJ_CHECK_EQ(context, loaded.entry_point.value(), std::uint32_t{0x00501000});
    RE2DJ_CHECK_EQ(context, loaded.tls_directory_rva, std::uint32_t{0x2200});
    RE2DJ_CHECK_EQ(context, loaded.tls_directory_size, std::uint32_t{24});

    std::uint32_t relocated = 0;
    RE2DJ_CHECK(context, space.Read32(GuestAddress(0x00501000), &relocated));
    RE2DJ_CHECK_EQ(context, relocated, std::uint32_t{0x00501234});
    std::uint8_t zero_fill = 1;
    RE2DJ_CHECK(context, space.Read8(GuestAddress(0x00501180), &zero_fill));
    RE2DJ_CHECK_EQ(context, zero_fill, std::uint8_t{0});

    std::uint32_t named_gate = 0;
    std::uint32_t ordinal_gate = 0;
    RE2DJ_CHECK(context, space.Read32(GuestAddress(0x005020A0), &named_gate));
    RE2DJ_CHECK(context, space.Read32(GuestAddress(0x005020A4), &ordinal_gate));
    RE2DJ_CHECK_EQ(context, named_gate, re2dj::runtime::kDefaultImportGateBase);
    RE2DJ_CHECK_EQ(context,
                   ordinal_gate,
                   re2dj::runtime::kDefaultImportGateBase +
                       re2dj::runtime::kDefaultImportGateStride);
    RE2DJ_CHECK_EQ(context, loaded.imports.size(), std::size_t{2});
    if (loaded.imports.size() == 2)
    {
        RE2DJ_CHECK_EQ(context, loaded.imports[0].module, std::string("kernel32.dll"));
        RE2DJ_CHECK_EQ(context, loaded.imports[0].name, std::string("CreateFileA"));
        RE2DJ_CHECK(context, !loaded.imports[0].by_ordinal);
        RE2DJ_CHECK(context, loaded.imports[1].by_ordinal);
        RE2DJ_CHECK_EQ(context, loaded.imports[1].ordinal, std::uint16_t{1});
    }

    // A malformed relocation must leave both caller-owned objects unchanged.
    std::vector<std::uint8_t> malformed = file;
    re2dj::test::PutU16(malformed, 0x508, 0x7000);
    AddressSpace untouched_space;
    ImportGateTable untouched_gates;
    LoadedPeImage unused;
    RE2DJ_CHECK(context,
                !re2dj::runtime::LoadPe32Image(malformed.data(),
                                               malformed.size(),
                                               info,
                                               GuestAddress(0x00600000),
                                               &untouched_space,
                                               &untouched_gates,
                                               &unused,
                                               &error));
    RE2DJ_CHECK_EQ(context, untouched_gates.gates().size(), std::size_t{0});
}
