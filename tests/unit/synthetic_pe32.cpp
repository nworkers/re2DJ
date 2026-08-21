#include "synthetic_pe32.h"

#include "re2dj/exe/pe_image.h"

namespace re2dj::test
{

namespace
{

constexpr std::size_t kImageSize = 0x400;

void PutSectionName(std::vector<std::uint8_t>& bytes, std::size_t offset, const char* name)
{
    for (std::size_t index = 0; index < 8 && name[index] != '\0'; ++index)
    {
        bytes[offset + index] = static_cast<std::uint8_t>(name[index]);
    }
}

}  // namespace

void PutU16(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint16_t value)
{
    bytes[offset + 0] = static_cast<std::uint8_t>(value & 0xFF);
    bytes[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
}

void PutU32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value)
{
    for (std::size_t index = 0; index < 4; ++index)
    {
        bytes[offset + index] = static_cast<std::uint8_t>((value >> (index * 8)) & 0xFF);
    }
}

std::vector<std::uint8_t> MakeSyntheticPe32Image()
{
    std::vector<std::uint8_t> bytes(kImageSize, 0);

    bytes[0] = 'M';
    bytes[1] = 'Z';
    PutU32(bytes, 0x3C, static_cast<std::uint32_t>(kSyntheticPeOffset));

    bytes[kSyntheticPeOffset + 0] = 'P';
    bytes[kSyntheticPeOffset + 1] = 'E';

    PutU16(bytes, kSyntheticFileHeaderOffset + 0, exe::kMachineI386);
    PutU16(bytes, kSyntheticFileHeaderOffset + 2, 2);           // NumberOfSections
    PutU32(bytes, kSyntheticFileHeaderOffset + 4, 0x3A5F1000);  // TimeDateStamp
    PutU16(bytes, kSyntheticFileHeaderOffset + 16, kSyntheticOptionalSize);
    PutU16(bytes, kSyntheticFileHeaderOffset + 18, 0x010E);     // EXECUTABLE | 32BIT

    PutU16(bytes, kSyntheticOptionalOffset + 0,
           static_cast<std::uint16_t>(exe::PeMagic::kPe32));
    PutU32(bytes, kSyntheticOptionalOffset + 16, 0x00001000);   // AddressOfEntryPoint
    PutU32(bytes, kSyntheticOptionalOffset + 28, 0x00400000);   // ImageBase
    PutU32(bytes, kSyntheticOptionalOffset + 32, 0x00001000);   // SectionAlignment
    PutU32(bytes, kSyntheticOptionalOffset + 36, 0x00000200);   // FileAlignment
    PutU16(bytes, kSyntheticOptionalOffset + 48, 4);            // MajorSubsystemVersion
    PutU16(bytes, kSyntheticOptionalOffset + 50, 0);            // MinorSubsystemVersion
    PutU32(bytes, kSyntheticOptionalOffset + 56, 0x00008000);   // SizeOfImage
    PutU32(bytes, kSyntheticOptionalOffset + 60, 0x00000400);   // SizeOfHeaders
    PutU16(bytes, kSyntheticOptionalOffset + 68, exe::kSubsystemWindowsGui);
    PutU32(bytes, kSyntheticOptionalOffset + 92, 16);           // NumberOfRvaAndSizes

    const std::size_t import_directory = kSyntheticOptionalOffset + 96 + 8;
    PutU32(bytes, import_directory + 0, 0x00002000);
    PutU32(bytes, import_directory + 4, 0x00000100);

    PutSectionName(bytes, kSyntheticSectionTableOffset, ".text");
    PutU32(bytes, kSyntheticSectionTableOffset + 8, 0x00000900);
    PutU32(bytes, kSyntheticSectionTableOffset + 12, 0x00001000);
    PutU32(bytes, kSyntheticSectionTableOffset + 16, 0x00000A00);
    PutU32(bytes, kSyntheticSectionTableOffset + 20, 0x00000400);
    PutU32(bytes, kSyntheticSectionTableOffset + 36, 0x60000020);

    PutSectionName(bytes, kSyntheticSectionTableOffset + 40, ".data");
    PutU32(bytes, kSyntheticSectionTableOffset + 40 + 8, 0x00000400);
    PutU32(bytes, kSyntheticSectionTableOffset + 40 + 12, 0x00002000);
    PutU32(bytes, kSyntheticSectionTableOffset + 40 + 16, 0x00000200);
    PutU32(bytes, kSyntheticSectionTableOffset + 40 + 20, 0x00000E00);
    PutU32(bytes, kSyntheticSectionTableOffset + 40 + 36, 0xC0000040);

    return bytes;
}

}  // namespace re2dj::test
