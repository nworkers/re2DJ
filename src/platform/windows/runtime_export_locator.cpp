#include "runtime_export_locator.h"

#include <windows.h>

#include <cstring>
#include <fstream>
#include <limits>
#include <vector>

namespace re2dj::platform::windows
{
namespace
{

bool ReadFile(const std::filesystem::path& path,
              std::vector<std::uint8_t>* bytes,
              std::string* error)
{
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream)
    {
        *error = "cannot open runtime DLL";
        return false;
    }
    const std::streamoff length = stream.tellg();
    if (length <= 0 || static_cast<std::uintmax_t>(length) >
                           (std::numeric_limits<std::size_t>::max)())
    {
        *error = "invalid runtime DLL size";
        return false;
    }
    bytes->resize(static_cast<std::size_t>(length));
    stream.seekg(0);
    stream.read(reinterpret_cast<char*>(bytes->data()),
                static_cast<std::streamsize>(bytes->size()));
    if (!stream)
    {
        *error = "cannot read runtime DLL";
        return false;
    }
    return true;
}

const std::uint8_t* RvaPointer(const std::vector<std::uint8_t>& bytes,
                               const IMAGE_NT_HEADERS32& headers,
                               std::uint32_t rva,
                               std::size_t size)
{
    if (rva < headers.OptionalHeader.SizeOfHeaders)
    {
        return rva <= bytes.size() && size <= bytes.size() - rva ? bytes.data() + rva : nullptr;
    }
    const IMAGE_SECTION_HEADER* sections = IMAGE_FIRST_SECTION(&headers);
    for (WORD index = 0; index < headers.FileHeader.NumberOfSections; ++index)
    {
        const IMAGE_SECTION_HEADER& section = sections[index];
        const DWORD span = section.Misc.VirtualSize > section.SizeOfRawData
                               ? section.Misc.VirtualSize
                               : section.SizeOfRawData;
        if (rva < section.VirtualAddress || rva - section.VirtualAddress > span ||
            size > span - (rva - section.VirtualAddress))
        {
            continue;
        }
        const std::uint64_t offset = static_cast<std::uint64_t>(section.PointerToRawData) +
                                     (rva - section.VirtualAddress);
        return offset <= bytes.size() && size <= bytes.size() - offset
                   ? bytes.data() + offset
                   : nullptr;
    }
    return nullptr;
}

}  // namespace

bool FindPe32ExportRva(const std::filesystem::path& path,
                       const char* name,
                       std::uint32_t* rva,
                       std::string* error)
{
    std::vector<std::uint8_t> bytes;
    if (name == nullptr || rva == nullptr || error == nullptr || !ReadFile(path, &bytes, error) ||
        bytes.size() < sizeof(IMAGE_DOS_HEADER))
    {
        return false;
    }
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes.data());
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0 ||
        static_cast<std::size_t>(dos->e_lfanew) > bytes.size() - sizeof(IMAGE_NT_HEADERS32))
    {
        *error = "runtime DLL has invalid PE32 headers";
        return false;
    }
    const auto* headers = reinterpret_cast<const IMAGE_NT_HEADERS32*>(bytes.data() + dos->e_lfanew);
    if (headers->Signature != IMAGE_NT_SIGNATURE || headers->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC)
    {
        *error = "runtime DLL is not PE32";
        return false;
    }
    const IMAGE_DATA_DIRECTORY directory =
        headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    const auto* exports = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(
        RvaPointer(bytes, *headers, directory.VirtualAddress, sizeof(IMAGE_EXPORT_DIRECTORY)));
    if (directory.VirtualAddress == 0 || exports == nullptr)
    {
        *error = "runtime DLL has no export directory";
        return false;
    }
    const auto* names = reinterpret_cast<const DWORD*>(
        RvaPointer(bytes, *headers, exports->AddressOfNames, exports->NumberOfNames * sizeof(DWORD)));
    const auto* ordinals = reinterpret_cast<const WORD*>(
        RvaPointer(bytes, *headers, exports->AddressOfNameOrdinals, exports->NumberOfNames * sizeof(WORD)));
    const auto* functions = reinterpret_cast<const DWORD*>(
        RvaPointer(bytes, *headers, exports->AddressOfFunctions, exports->NumberOfFunctions * sizeof(DWORD)));
    if (names == nullptr || ordinals == nullptr || functions == nullptr)
    {
        *error = "runtime DLL export table is malformed";
        return false;
    }
    for (DWORD index = 0; index < exports->NumberOfNames; ++index)
    {
        const char* candidate = reinterpret_cast<const char*>(
            RvaPointer(bytes, *headers, names[index], 1));
        if (candidate != nullptr && std::strcmp(candidate, name) == 0 &&
            ordinals[index] < exports->NumberOfFunctions)
        {
            *rva = functions[ordinals[index]];
            return *rva != 0;
        }
    }
    *error = "runtime DLL export was not found";
    return false;
}

}  // namespace re2dj::platform::windows
