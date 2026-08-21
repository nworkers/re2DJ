#include "re2dj/exe/pe_image.h"

#include <cstdint>
#include <string>
#include <vector>

#include "synthetic_pe32.h"
#include "test_support.h"

using re2dj::exe::IsGuestExecutable;
using re2dj::exe::PeDirectoryIndex;
using re2dj::exe::PeImageInfo;
using re2dj::exe::PeMagic;
using re2dj::exe::ReadPeImageInfo;
using re2dj::test::kSyntheticFileHeaderOffset;
using re2dj::test::kSyntheticPeOffset;
using re2dj::test::MakeSyntheticPe32Image;
using re2dj::test::PutU16;
using re2dj::test::PutU32;

void RunPeImageTests(re2dj::test::Context& context)
{
    std::vector<std::uint8_t> image = MakeSyntheticPe32Image();
    PeImageInfo info;
    std::string error;

    RE2DJ_CHECK(context, ReadPeImageInfo(image.data(), image.size(), &info, &error));
    RE2DJ_CHECK(context, info.magic == PeMagic::kPe32);
    RE2DJ_CHECK_EQ(context, info.machine, re2dj::exe::kMachineI386);
    RE2DJ_CHECK_EQ(context, info.image_base, std::uint64_t{0x00400000});
    RE2DJ_CHECK_EQ(context, info.entry_point_rva, std::uint32_t{0x00001000});
    RE2DJ_CHECK_EQ(context, info.section_alignment, std::uint32_t{0x00001000});
    RE2DJ_CHECK_EQ(context, info.file_alignment, std::uint32_t{0x00000200});
    RE2DJ_CHECK_EQ(context, info.size_of_image, std::uint32_t{0x00008000});
    RE2DJ_CHECK_EQ(context, info.size_of_headers, std::uint32_t{0x00000400});
    RE2DJ_CHECK_EQ(context, info.subsystem, re2dj::exe::kSubsystemWindowsGui);
    RE2DJ_CHECK_EQ(context, info.major_subsystem_version, std::uint16_t{4});
    RE2DJ_CHECK(context, !info.is_dll);
    RE2DJ_CHECK(context, IsGuestExecutable(info));

    RE2DJ_CHECK_EQ(context, info.sections.size(), std::size_t{2});
    if (info.sections.size() == 2)
    {
        RE2DJ_CHECK_EQ(context, info.sections[0].name, std::string(".text"));
        RE2DJ_CHECK_EQ(context, info.sections[0].virtual_address, std::uint32_t{0x1000});
        RE2DJ_CHECK_EQ(context, info.sections[0].raw_offset, std::uint32_t{0x400});
        RE2DJ_CHECK_EQ(context, info.sections[0].characteristics, std::uint32_t{0x60000020});
        RE2DJ_CHECK_EQ(context, info.sections[1].name, std::string(".data"));
        RE2DJ_CHECK_EQ(context, info.sections[1].virtual_address, std::uint32_t{0x2000});
    }

    RE2DJ_CHECK_EQ(context, info.data_directories.size(), std::size_t{16});
    const re2dj::exe::PeDataDirectory* import_directory =
        info.Directory(PeDirectoryIndex::kImport);
    RE2DJ_CHECK(context, import_directory != nullptr);
    if (import_directory != nullptr)
    {
        RE2DJ_CHECK_EQ(context, import_directory->virtual_address, std::uint32_t{0x2000});
        RE2DJ_CHECK_EQ(context, import_directory->size, std::uint32_t{0x100});
    }

    // The entry point of a normal build lies in .text.
    RE2DJ_CHECK_EQ(context,
                   std::string(re2dj::exe::EntryPointSectionName(info)),
                   std::string(".text"));
    RE2DJ_CHECK(context, !re2dj::exe::HasEntryPointOutsideTextSection(info));

    // A protector typically appends its stub as a new section and points the
    // entry there, which is what both protected EZ2DJ builds do (.gtide and
    // .protect). Moving the entry into .data reproduces that shape.
    std::vector<std::uint8_t> protected_image = MakeSyntheticPe32Image();
    PutU32(protected_image, re2dj::test::kSyntheticOptionalOffset + 16, 0x00002100);
    RE2DJ_CHECK(context,
                ReadPeImageInfo(protected_image.data(), protected_image.size(), &info, &error));
    RE2DJ_CHECK_EQ(context,
                   std::string(re2dj::exe::EntryPointSectionName(info)),
                   std::string(".data"));
    RE2DJ_CHECK(context, re2dj::exe::HasEntryPointOutsideTextSection(info));

    // An entry point in no section at all reports an empty name rather than
    // guessing, and still counts as outside .text.
    std::vector<std::uint8_t> stray_image = MakeSyntheticPe32Image();
    PutU32(stray_image, re2dj::test::kSyntheticOptionalOffset + 16, 0x00700000);
    RE2DJ_CHECK(context,
                ReadPeImageInfo(stray_image.data(), stray_image.size(), &info, &error));
    RE2DJ_CHECK(context, re2dj::exe::EntryPointSectionName(info).empty());
    RE2DJ_CHECK(context, re2dj::exe::HasEntryPointOutsideTextSection(info));

    // A DLL is not a launch target even when everything else matches.
    std::vector<std::uint8_t> dll_image = MakeSyntheticPe32Image();
    PutU16(dll_image, kSyntheticFileHeaderOffset + 18, 0x210E);
    RE2DJ_CHECK(context, ReadPeImageInfo(dll_image.data(), dll_image.size(), &info, &error));
    RE2DJ_CHECK(context, info.is_dll);
    RE2DJ_CHECK(context, !IsGuestExecutable(info));

    // A 64-bit image parses but is not the guest format this project targets.
    std::vector<std::uint8_t> amd64_image = MakeSyntheticPe32Image();
    PutU16(amd64_image, kSyntheticFileHeaderOffset + 0, re2dj::exe::kMachineAmd64);
    RE2DJ_CHECK(context, ReadPeImageInfo(amd64_image.data(), amd64_image.size(), &info, &error));
    RE2DJ_CHECK(context, !IsGuestExecutable(info));

    // Malformed inputs must fail rather than report garbage.
    std::vector<std::uint8_t> no_mz = MakeSyntheticPe32Image();
    no_mz[0] = 'X';
    RE2DJ_CHECK(context, !ReadPeImageInfo(no_mz.data(), no_mz.size(), &info, &error));

    std::vector<std::uint8_t> bad_offset = MakeSyntheticPe32Image();
    PutU32(bad_offset, 0x3C, 0x00FFFFFF);
    RE2DJ_CHECK(context, !ReadPeImageInfo(bad_offset.data(), bad_offset.size(), &info, &error));

    std::vector<std::uint8_t> no_pe = MakeSyntheticPe32Image();
    no_pe[kSyntheticPeOffset] = 'Q';
    RE2DJ_CHECK(context, !ReadPeImageInfo(no_pe.data(), no_pe.size(), &info, &error));

    // An optional header shorter than the fields the loader needs is rejected
    // rather than read past its end.
    std::vector<std::uint8_t> short_optional = MakeSyntheticPe32Image();
    PutU16(short_optional, kSyntheticFileHeaderOffset + 16, 32);
    RE2DJ_CHECK(context,
                !ReadPeImageInfo(short_optional.data(), short_optional.size(), &info, &error));

    RE2DJ_CHECK(context, !ReadPeImageInfo(image.data(), 8, &info, &error));
    RE2DJ_CHECK(context, !ReadPeImageInfo(nullptr, 0, &info, &error));
}
