#include "re2dj/exe/pe_image.h"

#include <algorithm>
#include <fstream>
#include <utility>

namespace re2dj::exe
{

namespace
{

// PE headers are little-endian regardless of host byte order, so every field is
// assembled byte by byte rather than reinterpreted from memory.
std::uint16_t ReadU16(const std::uint8_t* bytes)
{
    return static_cast<std::uint16_t>(bytes[0]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[1]) << 8);
}

std::uint32_t ReadU32(const std::uint8_t* bytes)
{
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8) |
           (static_cast<std::uint32_t>(bytes[2]) << 16) |
           (static_cast<std::uint32_t>(bytes[3]) << 24);
}

std::uint64_t ReadU64(const std::uint8_t* bytes)
{
    return static_cast<std::uint64_t>(ReadU32(bytes)) |
           (static_cast<std::uint64_t>(ReadU32(bytes + 4)) << 32);
}

constexpr std::uint16_t kDosSignature = 0x5A4D;      // "MZ"
constexpr std::uint32_t kPeSignature = 0x00004550;   // "PE\0\0"
constexpr std::uint16_t kFileDll = 0x2000;           // IMAGE_FILE_DLL
constexpr std::size_t kDosHeaderSize = 0x40;
constexpr std::size_t kFileHeaderSize = 20;
constexpr std::size_t kSectionHeaderSize = 40;

// Enough for the DOS stub, the PE header, and a section table far larger than
// any real game executable carries.
constexpr std::size_t kHeaderReadSize = 64 * 1024;

bool Fail(std::string* error, std::string message)
{
    if (error != nullptr)
    {
        *error = std::move(message);
    }
    return false;
}

}  // namespace

const PeDataDirectory* PeImageInfo::Directory(PeDirectoryIndex index) const
{
    const std::size_t position = static_cast<std::size_t>(index);
    if (position >= data_directories.size())
    {
        return nullptr;
    }
    return &data_directories[position];
}

bool ReadPeImageInfo(const std::uint8_t* bytes,
                     std::size_t size,
                     PeImageInfo* out,
                     std::string* error)
{
    if (out == nullptr)
    {
        return Fail(error, "internal error: output pointer is null");
    }
    if (bytes == nullptr || size < kDosHeaderSize)
    {
        return Fail(error, "file is too small to hold a DOS header");
    }
    if (ReadU16(bytes) != kDosSignature)
    {
        return Fail(error, "missing MZ signature");
    }

    const std::uint32_t pe_offset = ReadU32(bytes + 0x3C);
    if (pe_offset > size || size - pe_offset < 4 + kFileHeaderSize)
    {
        return Fail(error, "PE header offset lies outside the file");
    }
    if (ReadU32(bytes + pe_offset) != kPeSignature)
    {
        return Fail(error, "missing PE signature");
    }

    PeImageInfo info;

    const std::uint8_t* file_header = bytes + pe_offset + 4;
    info.machine = ReadU16(file_header + 0);
    info.section_count = ReadU16(file_header + 2);
    info.timestamp = ReadU32(file_header + 4);
    const std::uint16_t optional_header_size = ReadU16(file_header + 16);
    info.characteristics = ReadU16(file_header + 18);
    info.is_dll = (info.characteristics & kFileDll) != 0;

    const std::size_t optional_offset = pe_offset + 4 + kFileHeaderSize;
    if (optional_header_size == 0)
    {
        return Fail(error, "object file has no optional header");
    }
    if (optional_offset > size || size - optional_offset < optional_header_size)
    {
        return Fail(error, "optional header lies outside the file");
    }

    const std::uint8_t* optional = bytes + optional_offset;
    const std::uint16_t magic_value = ReadU16(optional);
    if (magic_value == static_cast<std::uint16_t>(PeMagic::kPe32))
    {
        info.magic = PeMagic::kPe32;
    }
    else if (magic_value == static_cast<std::uint16_t>(PeMagic::kPe32Plus))
    {
        info.magic = PeMagic::kPe32Plus;
    }
    else
    {
        return Fail(error, "unknown optional header magic");
    }

    const bool is_plus = info.magic == PeMagic::kPe32Plus;
    // The two layouts agree up to BaseOfCode; from ImageBase on, PE32+ widens
    // ImageBase to eight bytes and drops BaseOfData.
    const std::size_t minimum_optional = is_plus ? 112 : 96;
    if (optional_header_size < minimum_optional)
    {
        return Fail(error, "optional header is truncated");
    }

    info.entry_point_rva = ReadU32(optional + 16);
    if (is_plus)
    {
        info.image_base = ReadU64(optional + 24);
    }
    else
    {
        info.image_base = ReadU32(optional + 28);
    }
    info.section_alignment = ReadU32(optional + 32);
    info.file_alignment = ReadU32(optional + 36);
    info.major_subsystem_version = ReadU16(optional + 48);
    info.minor_subsystem_version = ReadU16(optional + 50);
    info.size_of_image = ReadU32(optional + 56);
    info.size_of_headers = ReadU32(optional + 60);
    info.subsystem = ReadU16(optional + 68);
    info.dll_characteristics = ReadU16(optional + 70);

    const std::size_t directory_count_offset = is_plus ? 108 : 92;
    const std::uint32_t declared_directories = ReadU32(optional + directory_count_offset);
    const std::size_t directory_table_offset = directory_count_offset + 4;
    const std::size_t available_directory_bytes =
        optional_header_size > directory_table_offset
            ? optional_header_size - directory_table_offset
            : 0;
    const std::size_t directory_count =
        std::min<std::size_t>(declared_directories, available_directory_bytes / 8);

    info.data_directories.reserve(directory_count);
    for (std::size_t index = 0; index < directory_count; ++index)
    {
        const std::uint8_t* entry = optional + directory_table_offset + index * 8;
        PeDataDirectory directory;
        directory.virtual_address = ReadU32(entry + 0);
        directory.size = ReadU32(entry + 4);
        info.data_directories.push_back(directory);
    }

    const std::size_t section_table_offset = optional_offset + optional_header_size;
    const std::size_t section_table_bytes =
        static_cast<std::size_t>(info.section_count) * kSectionHeaderSize;
    if (section_table_offset > size || size - section_table_offset < section_table_bytes)
    {
        return Fail(error, "section table lies outside the read header region");
    }

    info.sections.reserve(info.section_count);
    for (std::size_t index = 0; index < info.section_count; ++index)
    {
        const std::uint8_t* entry = bytes + section_table_offset + index * kSectionHeaderSize;
        PeSection section;
        // The name field is eight bytes and is NUL-padded, not NUL-terminated.
        std::size_t name_length = 0;
        while (name_length < 8 && entry[name_length] != 0)
        {
            ++name_length;
        }
        section.name.assign(reinterpret_cast<const char*>(entry), name_length);
        section.virtual_size = ReadU32(entry + 8);
        section.virtual_address = ReadU32(entry + 12);
        section.raw_size = ReadU32(entry + 16);
        section.raw_offset = ReadU32(entry + 20);
        section.characteristics = ReadU32(entry + 36);
        info.sections.push_back(std::move(section));
    }

    *out = std::move(info);
    return true;
}

bool ReadPeImageInfo(const std::filesystem::path& path,
                     PeImageInfo* out,
                     std::string* error)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        return Fail(error, "cannot open file: " + path.string());
    }

    std::vector<std::uint8_t> buffer(kHeaderReadSize);
    stream.read(reinterpret_cast<char*>(buffer.data()),
                static_cast<std::streamsize>(buffer.size()));
    const std::streamsize read_bytes = stream.gcount();
    if (read_bytes <= 0)
    {
        return Fail(error, "file is empty: " + path.string());
    }
    buffer.resize(static_cast<std::size_t>(read_bytes));

    return ReadPeImageInfo(buffer.data(), buffer.size(), out, error);
}

bool IsGuestExecutable(const PeImageInfo& info)
{
    return info.machine == kMachineI386 && info.magic == PeMagic::kPe32 && !info.is_dll;
}

std::string_view EntryPointSectionName(const PeImageInfo& info)
{
    for (const PeSection& section : info.sections)
    {
        // A section occupies whichever of its two sizes is larger: raw data can
        // be padded up to FileAlignment, and virtual size can exceed raw size
        // when the tail is zero-filled.
        const std::uint32_t span = std::max(section.virtual_size, section.raw_size);
        if (info.entry_point_rva >= section.virtual_address &&
            info.entry_point_rva < section.virtual_address + span)
        {
            return section.name;
        }
    }
    return {};
}

bool HasEntryPointOutsideTextSection(const PeImageInfo& info)
{
    return EntryPointSectionName(info) != ".text";
}

std::string_view MachineName(std::uint16_t machine)
{
    switch (machine)
    {
    case kMachineI386:
        return "i386";
    case kMachineAmd64:
        return "amd64";
    case kMachineArm64:
        return "arm64";
    default:
        return "unknown";
    }
}

std::string_view SubsystemName(std::uint16_t subsystem)
{
    switch (subsystem)
    {
    case kSubsystemWindowsGui:
        return "windows-gui";
    case kSubsystemWindowsCui:
        return "windows-cui";
    default:
        return "other";
    }
}

std::string_view MagicName(PeMagic magic)
{
    switch (magic)
    {
    case PeMagic::kPe32:
        return "PE32";
    case PeMagic::kPe32Plus:
        return "PE32+";
    case PeMagic::kUnknown:
    default:
        return "unknown";
    }
}

}  // namespace re2dj::exe
