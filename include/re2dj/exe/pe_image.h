#ifndef RE2DJ_EXE_PE_IMAGE_H_
#define RE2DJ_EXE_PE_IMAGE_H_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace re2dj::exe
{

// COFF machine values. Only i386 matters for this project; the others are here
// so a scan can explain why an executable was rejected.
inline constexpr std::uint16_t kMachineI386 = 0x014C;
inline constexpr std::uint16_t kMachineAmd64 = 0x8664;
inline constexpr std::uint16_t kMachineArm64 = 0xAA64;

// Optional-header subsystem values relevant to a desktop game.
inline constexpr std::uint16_t kSubsystemWindowsGui = 2;
inline constexpr std::uint16_t kSubsystemWindowsCui = 3;

enum class PeMagic : std::uint16_t
{
    kUnknown = 0,
    kPe32 = 0x010B,
    kPe32Plus = 0x020B,
};

// Index into PeImageInfo::data_directories.
enum class PeDirectoryIndex : std::size_t
{
    kExport = 0,
    kImport = 1,
    kResource = 2,
    kException = 3,
    kSecurity = 4,
    kBaseRelocation = 5,
    kDebug = 6,
    kTls = 9,
    kBoundImport = 11,
    kImportAddressTable = 12,
    kDelayImport = 13,
};

struct PeSection
{
    std::string name;
    std::uint32_t virtual_size = 0;
    std::uint32_t virtual_address = 0;
    std::uint32_t raw_size = 0;
    std::uint32_t raw_offset = 0;
    std::uint32_t characteristics = 0;
};

struct PeDataDirectory
{
    std::uint32_t virtual_address = 0;
    std::uint32_t size = 0;
};

// Header-level description of a PE image. This is a read-only report: nothing
// here maps, relocates, or binds anything. Loading belongs to the runtime layer.
struct PeImageInfo
{
    std::uint16_t machine = 0;
    std::uint16_t section_count = 0;
    std::uint32_t timestamp = 0;
    std::uint16_t characteristics = 0;

    PeMagic magic = PeMagic::kUnknown;
    std::uint64_t image_base = 0;
    std::uint32_t entry_point_rva = 0;
    std::uint32_t size_of_image = 0;
    std::uint32_t size_of_headers = 0;
    std::uint32_t section_alignment = 0;
    std::uint32_t file_alignment = 0;
    std::uint16_t subsystem = 0;
    std::uint16_t dll_characteristics = 0;
    std::uint16_t major_subsystem_version = 0;
    std::uint16_t minor_subsystem_version = 0;

    std::vector<PeSection> sections;
    std::vector<PeDataDirectory> data_directories;

    // True when the file header sets IMAGE_FILE_DLL.
    bool is_dll = false;

    const PeDataDirectory* Directory(PeDirectoryIndex index) const;
};

// Reads PE headers from `bytes`, which must contain at least the DOS header,
// the PE header, and the section table. `error` receives a message on failure.
bool ReadPeImageInfo(const std::uint8_t* bytes,
                     std::size_t size,
                     PeImageInfo* out,
                     std::string* error);

// Reads only the leading header region of the file, so scanning a directory
// full of executables does not pull whole images into memory.
bool ReadPeImageInfo(const std::filesystem::path& path,
                     PeImageInfo* out,
                     std::string* error);

// True for the format this project targets: a 32-bit x86 PE32 executable that
// is not a DLL.
bool IsGuestExecutable(const PeImageInfo& info);

// Name of the section containing the entry point, or an empty view when the
// entry point falls outside every section.
std::string_view EntryPointSectionName(const PeImageInfo& info);

// True when the entry point does not lie in a section named ".text".
//
// This is an observation, not a verdict. A packer or protector typically adds
// its stub as a new section and points the entry there, so this is the cheapest
// signal that an image will start by rewriting itself. It is still only a
// signal: a legitimate program may place its entry point elsewhere, and a
// protected one may leave its entry point inside .text.
bool HasEntryPointOutsideTextSection(const PeImageInfo& info);

std::string_view MachineName(std::uint16_t machine);
std::string_view SubsystemName(std::uint16_t subsystem);
std::string_view MagicName(PeMagic magic);

}  // namespace re2dj::exe

#endif  // RE2DJ_EXE_PE_IMAGE_H_
