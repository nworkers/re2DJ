#include "remote_module_exports.h"

#include <algorithm>
#include <cstring>
#include <utility>
#include <vector>

namespace re2dj::tools::windows_x86_launcher_probe
{
namespace
{

constexpr std::uint16_t kPe32Magic = 0x010b;
constexpr std::size_t kDataDirectoryOffsetInOptionalHeader = 96;
constexpr std::size_t kExportDirectoryIndex = 0;
constexpr std::size_t kImageExportDirectorySize = 40;
constexpr std::uint32_t kMaxExportCount = 128 * 1024;
constexpr std::size_t kMaxExportBlobSize = 4 * 1024 * 1024;

// IMAGE_EXPORT_DIRECTORY fields this helper consumes.
struct ExportDirectoryInfo
{
    std::uint32_t directory_rva = 0;
    std::uint32_t directory_size = 0;
    std::uint32_t name_rva = 0;
    std::uint32_t number_of_functions = 0;
    std::uint32_t number_of_names = 0;
    std::uint32_t functions_rva = 0;
    std::uint32_t names_rva = 0;
    std::uint32_t ordinals_rva = 0;
};

bool ReadRemoteBytes(HANDLE process, std::uintptr_t address, void* buffer, SIZE_T size)
{
    SIZE_T copied = 0;
    return ReadProcessMemory(process,
                             reinterpret_cast<const void*>(address),
                             buffer,
                             size,
                             &copied) != FALSE &&
           copied == size;
}

std::uint16_t ReadU16(const std::uint8_t* bytes)
{
    std::uint16_t value = 0;
    std::memcpy(&value, bytes, sizeof(value));
    return value;
}

std::uint32_t ReadU32(const std::uint8_t* bytes)
{
    std::uint32_t value = 0;
    std::memcpy(&value, bytes, sizeof(value));
    return value;
}

bool ReadExportDirectoryInfo(HANDLE process,
                             std::uintptr_t module_base,
                             ExportDirectoryInfo* info,
                             std::string* error)
{
    // The DOS header, PE headers, and optional header all live in the first
    // page of a mapped image; reading one bounded block keeps this simple.
    std::uint8_t headers[0x400] = {};
    if (!ReadRemoteBytes(process, module_base, headers, sizeof(headers)))
    {
        *error = "cannot read remote module headers";
        return false;
    }
    if (headers[0] != 'M' || headers[1] != 'Z')
    {
        *error = "remote module lacks MZ signature";
        return false;
    }
    const std::uint32_t pe_offset = ReadU32(headers + 0x3c);
    const std::size_t file_header_offset = pe_offset + 4;
    if (pe_offset == 0 ||
        file_header_offset + 20 + kDataDirectoryOffsetInOptionalHeader + 8 > sizeof(headers) ||
        headers[pe_offset] != 'P' || headers[pe_offset + 1] != 'E')
    {
        *error = "remote module lacks PE signature";
        return false;
    }
    const std::size_t optional_header_offset = file_header_offset + 20;
    if (ReadU16(headers + optional_header_offset) != kPe32Magic)
    {
        *error = "remote module is not PE32";
        return false;
    }
    const std::size_t export_directory_offset =
        optional_header_offset + kDataDirectoryOffsetInOptionalHeader +
        kExportDirectoryIndex * sizeof(std::uint32_t) * 2;
    info->directory_rva = ReadU32(headers + export_directory_offset);
    info->directory_size = ReadU32(headers + export_directory_offset + 4);
    if (info->directory_rva == 0 || info->directory_size < kImageExportDirectorySize)
    {
        *error = "remote module has no usable export directory";
        return false;
    }

    std::uint8_t directory[kImageExportDirectorySize] = {};
    if (!ReadRemoteBytes(process, module_base + info->directory_rva, directory, sizeof(directory)))
    {
        *error = "cannot read remote export directory";
        return false;
    }
    info->name_rva = ReadU32(directory + 0x0c);
    info->number_of_functions = ReadU32(directory + 0x14);
    info->number_of_names = ReadU32(directory + 0x18);
    info->functions_rva = ReadU32(directory + 0x1c);
    info->names_rva = ReadU32(directory + 0x20);
    info->ordinals_rva = ReadU32(directory + 0x24);
    if (info->number_of_names == 0 || info->number_of_names > kMaxExportCount ||
        info->number_of_functions == 0 || info->number_of_functions > kMaxExportCount)
    {
        *error = "remote export table size is out of range";
        return false;
    }
    return true;
}

// One bulk copy of the export-directory blob lets most name strings be
// compared without per-name round trips to the child. Falls back to a direct
// read when a name lies outside the blob.
class ExportNameReader
{
public:
    ExportNameReader(HANDLE process,
                     std::uintptr_t module_base,
                     std::uint32_t directory_rva,
                     std::uint32_t directory_size)
        : process_(process),
          module_base_(module_base),
          directory_rva_(directory_rva)
    {
        blob_.resize((std::min)(directory_size, static_cast<std::uint32_t>(kMaxExportBlobSize)));
        have_blob_ = ReadRemoteBytes(process,
                                     module_base + directory_rva,
                                     blob_.data(),
                                     blob_.size());
    }

    void Read(std::uint32_t name_rva, char (&buffer)[128]) const
    {
        buffer[0] = '\0';
        if (have_blob_ && name_rva >= directory_rva_ &&
            static_cast<std::size_t>(name_rva - directory_rva_) < blob_.size())
        {
            const std::size_t offset = name_rva - directory_rva_;
            const std::size_t length = (std::min)(blob_.size() - offset, sizeof(buffer) - 1);
            std::memcpy(buffer, blob_.data() + offset, length);
            return;
        }
        SIZE_T copied = 0;
        if (ReadProcessMemory(process_,
                              reinterpret_cast<const void*>(module_base_ + name_rva),
                              buffer,
                              sizeof(buffer) - 1,
                              &copied) != FALSE &&
            copied > 0)
        {
            buffer[copied] = '\0';
        }
    }

private:
    HANDLE process_ = nullptr;
    std::uintptr_t module_base_ = 0;
    std::uint32_t directory_rva_ = 0;
    std::vector<std::uint8_t> blob_;
    bool have_blob_ = false;
};

}  // namespace

bool ResolveRemotePe32Export(HANDLE process,
                             std::uintptr_t module_base,
                             const char* name,
                             RemoteExportResolution* resolution,
                             std::string* error)
{
    ExportDirectoryInfo info;
    if (!ReadExportDirectoryInfo(process, module_base, &info, error))
    {
        return false;
    }

    std::vector<std::uint32_t> name_addresses(info.number_of_names);
    if (!ReadRemoteBytes(process,
                         module_base + info.names_rva,
                         name_addresses.data(),
                         name_addresses.size() * sizeof(std::uint32_t)))
    {
        *error = "cannot read remote export name table";
        return false;
    }
    std::vector<std::uint16_t> ordinals(info.number_of_names);
    if (!ReadRemoteBytes(process,
                         module_base + info.ordinals_rva,
                         ordinals.data(),
                         ordinals.size() * sizeof(std::uint16_t)))
    {
        *error = "cannot read remote export ordinal table";
        return false;
    }

    const ExportNameReader name_reader(process,
                                       module_base,
                                       info.directory_rva,
                                       info.directory_size);
    for (std::uint32_t index = 0; index < info.number_of_names; ++index)
    {
        char candidate[128] = {};
        name_reader.Read(name_addresses[index], candidate);
        if (std::strncmp(candidate, name, sizeof(candidate)) != 0)
        {
            continue;
        }
        if (ordinals[index] >= info.number_of_functions)
        {
            *error = "remote export ordinal is out of range";
            return false;
        }
        std::uint32_t function_rva = 0;
        if (!ReadRemoteBytes(process,
                             module_base + info.functions_rva +
                                 static_cast<std::uintptr_t>(ordinals[index]) *
                                     sizeof(std::uint32_t),
                             &function_rva,
                             sizeof(function_rva)))
        {
            *error = "cannot read remote export function table";
            return false;
        }
        resolution->forwarded =
            function_rva >= info.directory_rva &&
            static_cast<std::size_t>(function_rva - info.directory_rva) < info.directory_size;
        resolution->address = resolution->forwarded ? 0 : module_base + function_rva;
        return true;
    }
    *error = "remote module does not export the requested name";
    return false;
}

bool FindRemotePe32NearestExport(HANDLE process,
                                 std::uintptr_t module_base,
                                 std::uintptr_t address,
                                 RemoteNearestExport* result,
                                 std::string* error)
{
    ExportDirectoryInfo info;
    if (!ReadExportDirectoryInfo(process, module_base, &info, error))
    {
        return false;
    }

    std::vector<std::uint32_t> functions(info.number_of_functions);
    if (!ReadRemoteBytes(process,
                         module_base + info.functions_rva,
                         functions.data(),
                         functions.size() * sizeof(std::uint32_t)))
    {
        *error = "cannot read remote export function table";
        return false;
    }
    std::vector<std::uint32_t> name_addresses(info.number_of_names);
    if (!ReadRemoteBytes(process,
                         module_base + info.names_rva,
                         name_addresses.data(),
                         name_addresses.size() * sizeof(std::uint32_t)))
    {
        *error = "cannot read remote export name table";
        return false;
    }
    std::vector<std::uint16_t> ordinals(info.number_of_names);
    if (!ReadRemoteBytes(process,
                         module_base + info.ordinals_rva,
                         ordinals.data(),
                         ordinals.size() * sizeof(std::uint16_t)))
    {
        *error = "cannot read remote export ordinal table";
        return false;
    }

    // Candidates are non-forwarded named functions sorted by RVA. Forwarders
    // are name strings inside the directory range and would mislabel nearby
    // code if treated as positions.
    std::vector<std::pair<std::uint32_t, std::uint32_t>> candidates;
    candidates.reserve(info.number_of_names);
    for (std::uint32_t index = 0; index < info.number_of_names; ++index)
    {
        if (ordinals[index] >= info.number_of_functions)
        {
            continue;
        }
        const std::uint32_t function_rva = functions[ordinals[index]];
        const bool forwarded =
            function_rva >= info.directory_rva &&
            static_cast<std::size_t>(function_rva - info.directory_rva) < info.directory_size;
        if (forwarded)
        {
            continue;
        }
        candidates.emplace_back(function_rva, index);
    }
    std::sort(candidates.begin(), candidates.end());
    const std::uintptr_t address_rva = address - module_base;
    const auto upper = std::upper_bound(candidates.begin(),
                                        candidates.end(),
                                        address_rva,
                                        [](std::uintptr_t value,
                                           const std::pair<std::uint32_t, std::uint32_t>& candidate) {
                                            return value < candidate.first;
                                        });
    if (upper == candidates.begin())
    {
        *error = "no export exists at or below the address";
        return false;
    }
    const std::pair<std::uint32_t, std::uint32_t>& chosen = *std::prev(upper);

    const ExportNameReader name_reader(process,
                                       module_base,
                                       info.directory_rva,
                                       info.directory_size);
    name_reader.Read(name_addresses[chosen.second], result->function);
    name_reader.Read(info.name_rva, result->module);
    result->function_rva = chosen.first;
    result->offset = static_cast<std::int32_t>(address_rva - chosen.first);
    return true;
}

}  // namespace re2dj::tools::windows_x86_launcher_probe
