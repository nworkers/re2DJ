#include "iat_verifier.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>
#include <utility>

namespace re2dj::tools::windows_original_process_probe
{
namespace
{

constexpr std::uint32_t kImportDescriptorSize = 20;
constexpr std::uint32_t kImportByOrdinalFlag32 = 0x80000000;
constexpr std::size_t kMaximumImportStringLength = 4096;

struct IatSlot
{
    std::string module;
    std::uint32_t rva = 0;
};

std::uint32_t ReadU32(const std::uint8_t* bytes)
{
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8) |
           (static_cast<std::uint32_t>(bytes[2]) << 16) |
           (static_cast<std::uint32_t>(bytes[3]) << 24);
}

const std::uint8_t* RvaPointer(const exe::PeImageInfo& info,
                               const std::uint8_t* file,
                               std::size_t file_size,
                               std::uint32_t rva,
                               std::size_t size)
{
    if (rva < info.size_of_headers)
    {
        return size <= file_size - rva ? file + rva : nullptr;
    }
    for (const exe::PeSection& section : info.sections)
    {
        const std::uint32_t span = std::max(section.virtual_size, section.raw_size);
        if (rva < section.virtual_address || rva - section.virtual_address > span ||
            size > span - (rva - section.virtual_address))
        {
            continue;
        }
        const std::uint64_t offset = static_cast<std::uint64_t>(section.raw_offset) +
                                     (rva - section.virtual_address);
        if (offset > file_size || size > file_size - offset)
        {
            return nullptr;
        }
        return file + offset;
    }
    return nullptr;
}

bool ReadCString(const exe::PeImageInfo& info,
                 const std::uint8_t* file,
                 std::size_t file_size,
                 std::uint32_t rva,
                 std::string* value)
{
    value->clear();
    for (std::size_t index = 0; index < kMaximumImportStringLength; ++index)
    {
        if (index > (std::numeric_limits<std::uint32_t>::max)() - rva)
        {
            return false;
        }
        const std::uint8_t* byte = RvaPointer(
            info, file, file_size, rva + static_cast<std::uint32_t>(index), 1);
        if (byte == nullptr)
        {
            return false;
        }
        if (*byte == 0)
        {
            return true;
        }
        value->push_back(static_cast<char>(*byte));
    }
    return false;
}

bool CollectIatSlots(const exe::PeImageInfo& info,
                     const std::uint8_t* file,
                     std::size_t file_size,
                     std::vector<IatSlot>* slots,
                     std::string* error)
{
    const exe::PeDataDirectory* directory = info.Directory(exe::PeDirectoryIndex::kImport);
    if (directory == nullptr || directory->virtual_address == 0 || directory->size == 0)
    {
        *error = "PE image has no import directory";
        return false;
    }
    bool terminated = false;
    for (std::uint32_t offset = 0; offset + kImportDescriptorSize <= directory->size;
         offset += kImportDescriptorSize)
    {
        const std::uint8_t* descriptor = RvaPointer(
            info, file, file_size, directory->virtual_address + offset, kImportDescriptorSize);
        if (descriptor == nullptr)
        {
            *error = "import descriptor lies outside original file";
            return false;
        }
        const std::uint32_t original_first_thunk = ReadU32(descriptor);
        const std::uint32_t name_rva = ReadU32(descriptor + 12);
        const std::uint32_t first_thunk = ReadU32(descriptor + 16);
        if (original_first_thunk == 0 && ReadU32(descriptor + 4) == 0 &&
            ReadU32(descriptor + 8) == 0 && name_rva == 0 && first_thunk == 0)
        {
            terminated = true;
            break;
        }
        std::string module;
        if (name_rva == 0 || first_thunk == 0 ||
            !ReadCString(info, file, file_size, name_rva, &module))
        {
            *error = "import descriptor is malformed";
            return false;
        }
        const std::uint32_t lookup_rva =
            original_first_thunk == 0 ? first_thunk : original_first_thunk;
        for (std::uint32_t index = 0; index <= info.size_of_image / 4; ++index)
        {
            const std::uint64_t offset_bytes = static_cast<std::uint64_t>(index) * 4;
            if (lookup_rva > (std::numeric_limits<std::uint32_t>::max)() - offset_bytes ||
                first_thunk > (std::numeric_limits<std::uint32_t>::max)() - offset_bytes)
            {
                *error = "import thunk RVA overflows";
                return false;
            }
            const std::uint8_t* thunk = RvaPointer(
                info, file, file_size, lookup_rva + static_cast<std::uint32_t>(offset_bytes), 4);
            if (thunk == nullptr)
            {
                *error = "import lookup thunk lies outside original file";
                return false;
            }
            if (ReadU32(thunk) == 0)
            {
                break;
            }
            slots->push_back({module, first_thunk + static_cast<std::uint32_t>(offset_bytes)});
        }
    }
    if (!terminated || slots->empty())
    {
        *error = "import descriptor table is not terminated";
        return false;
    }
    return true;
}

}  // namespace

bool VerifySuspendedIat(HANDLE process,
                        std::uintptr_t image_base,
                        const exe::PeImageInfo& info,
                        const std::uint8_t* file,
                        std::size_t file_size,
                        IatVerificationResult* result,
                        std::string* error)
{
    if (process == nullptr || file == nullptr || result == nullptr || error == nullptr ||
        image_base > (std::numeric_limits<std::uint32_t>::max)())
    {
        return false;
    }
    std::vector<IatSlot> slots;
    if (!CollectIatSlots(info, file, file_size, &slots, error))
    {
        return false;
    }
    *result = {};
    for (const IatSlot& slot : slots)
    {
        if (slot.rva > (std::numeric_limits<std::uint32_t>::max)() - image_base)
        {
            *error = "IAT address overflows child address space";
            return false;
        }
        std::uint32_t value = 0;
        SIZE_T copied = 0;
        if (ReadProcessMemory(process,
                              reinterpret_cast<const void*>(image_base + slot.rva),
                              &value,
                              sizeof(value),
                              &copied) == FALSE ||
            copied != sizeof(value) || value == 0 ||
            (value >= image_base && value < image_base + info.size_of_image))
        {
            char message[256] = {};
            std::snprintf(message,
                          sizeof(message),
                          "suspended child has an unresolved IAT slot in %s at RVA 0x%08x with value 0x%08x",
                          slot.module.c_str(),
                          slot.rva,
                          value);
            *error = message;
            return false;
        }
        auto module = std::find_if(result->modules.begin(),
                                   result->modules.end(),
                                   [&slot](const IatModuleCount& count) {
                                       return count.module == slot.module;
                                   });
        if (module == result->modules.end())
        {
            result->modules.push_back({slot.module, 1});
        }
        else
        {
            ++module->slot_count;
        }
        ++result->slot_count;
    }
    return true;
}

bool FindIatSlotByName(const exe::PeImageInfo& info,
                       const std::uint8_t* file,
                       std::size_t file_size,
                       const std::string& module,
                       const std::string& function,
                       std::uint32_t* slot_rva,
                       std::string* error)
{
    if (slot_rva == nullptr || error == nullptr)
    {
        return false;
    }
    std::vector<std::uint32_t> slot_rvas;
    if (!FindIatSlotsByName(info,
                            file,
                            file_size,
                            module,
                            function,
                            &slot_rvas,
                            error))
    {
        return false;
    }
    *slot_rva = slot_rvas.front();
    return true;
}

bool FindIatSlotsByName(const exe::PeImageInfo& info,
                        const std::uint8_t* file,
                        std::size_t file_size,
                        const std::string& module,
                        const std::string& function,
                        std::vector<std::uint32_t>* slot_rvas,
                        std::string* error)
{
    if (file == nullptr || slot_rvas == nullptr || error == nullptr)
    {
        return false;
    }
    slot_rvas->clear();
    const exe::PeDataDirectory* directory = info.Directory(exe::PeDirectoryIndex::kImport);
    if (directory == nullptr || directory->virtual_address == 0 || directory->size == 0)
    {
        *error = "PE image has no import directory";
        return false;
    }
    for (std::uint32_t offset = 0; offset + kImportDescriptorSize <= directory->size;
         offset += kImportDescriptorSize)
    {
        const std::uint8_t* descriptor = RvaPointer(
            info, file, file_size, directory->virtual_address + offset, kImportDescriptorSize);
        if (descriptor == nullptr)
        {
            *error = "import descriptor lies outside original file";
            return false;
        }
        const std::uint32_t original_first_thunk = ReadU32(descriptor);
        const std::uint32_t name_rva = ReadU32(descriptor + 12);
        const std::uint32_t first_thunk = ReadU32(descriptor + 16);
        if (original_first_thunk == 0 && ReadU32(descriptor + 4) == 0 &&
            ReadU32(descriptor + 8) == 0 && name_rva == 0 && first_thunk == 0)
        {
            break;
        }
        std::string imported_module;
        if (!ReadCString(info, file, file_size, name_rva, &imported_module) ||
            imported_module != module)
        {
            continue;
        }
        const std::uint32_t lookup_rva =
            original_first_thunk == 0 ? first_thunk : original_first_thunk;
        for (std::uint32_t index = 0; index <= info.size_of_image / 4; ++index)
        {
            const std::uint64_t offset_bytes = static_cast<std::uint64_t>(index) * 4;
            if (lookup_rva > (std::numeric_limits<std::uint32_t>::max)() - offset_bytes ||
                first_thunk > (std::numeric_limits<std::uint32_t>::max)() - offset_bytes)
            {
                *error = "import thunk RVA overflows";
                return false;
            }
            const std::uint8_t* thunk = RvaPointer(
                info, file, file_size, lookup_rva + static_cast<std::uint32_t>(offset_bytes), 4);
            if (thunk == nullptr)
            {
                *error = "import lookup thunk lies outside original file";
                return false;
            }
            const std::uint32_t import_by_name_rva = ReadU32(thunk);
            if (import_by_name_rva == 0)
            {
                break;
            }
            if ((import_by_name_rva & kImportByOrdinalFlag32) != 0)
            {
                continue;
            }
            std::string imported_function;
            if (!ReadCString(info,
                             file,
                             file_size,
                             import_by_name_rva + 2,
                             &imported_function))
            {
                *error = "import function name is malformed";
                return false;
            }
            if (imported_function == function)
            {
                slot_rvas->push_back(first_thunk + static_cast<std::uint32_t>(offset_bytes));
            }
        }
    }
    if (!slot_rvas->empty())
    {
        return true;
    }
    *error = "requested import is not present";
    return false;
}

bool FindIatSlotByOrdinal(const exe::PeImageInfo& info,
                          const std::uint8_t* file,
                          std::size_t file_size,
                          const std::string& module,
                          std::uint16_t ordinal,
                          std::uint32_t* slot_rva,
                          std::string* error)
{
    if (file == nullptr || slot_rva == nullptr || error == nullptr) return false;
    const exe::PeDataDirectory* directory = info.Directory(exe::PeDirectoryIndex::kImport);
    if (directory == nullptr || directory->virtual_address == 0 || directory->size == 0) { *error = "PE image has no import directory"; return false; }
    for (std::uint32_t offset = 0; offset + kImportDescriptorSize <= directory->size; offset += kImportDescriptorSize)
    {
        const std::uint8_t* descriptor = RvaPointer(info, file, file_size, directory->virtual_address + offset, kImportDescriptorSize);
        if (descriptor == nullptr) { *error = "import descriptor lies outside original file"; return false; }
        const std::uint32_t original_first_thunk = ReadU32(descriptor);
        const std::uint32_t name_rva = ReadU32(descriptor + 12);
        const std::uint32_t first_thunk = ReadU32(descriptor + 16);
        if (original_first_thunk == 0 && ReadU32(descriptor + 4) == 0 && ReadU32(descriptor + 8) == 0 && name_rva == 0 && first_thunk == 0) break;
        std::string imported_module;
        if (!ReadCString(info, file, file_size, name_rva, &imported_module) || imported_module != module) continue;
        const std::uint32_t lookup_rva = original_first_thunk == 0 ? first_thunk : original_first_thunk;
        for (std::uint32_t index = 0; index <= info.size_of_image / 4; ++index)
        {
            const std::uint64_t offset_bytes = static_cast<std::uint64_t>(index) * 4;
            if (lookup_rva > (std::numeric_limits<std::uint32_t>::max)() - offset_bytes || first_thunk > (std::numeric_limits<std::uint32_t>::max)() - offset_bytes) { *error = "import thunk RVA overflows"; return false; }
            const std::uint8_t* thunk = RvaPointer(info, file, file_size, lookup_rva + static_cast<std::uint32_t>(offset_bytes), 4);
            if (thunk == nullptr) { *error = "import lookup thunk lies outside original file"; return false; }
            const std::uint32_t value = ReadU32(thunk);
            if (value == 0) break;
            if ((value & kImportByOrdinalFlag32) != 0 && static_cast<std::uint16_t>(value) == ordinal) { *slot_rva = first_thunk + static_cast<std::uint32_t>(offset_bytes); return true; }
        }
    }
    *error = "requested ordinal import is not present";
    return false;
}

bool ResolveIatSlot(const exe::PeImageInfo& info,
                    const std::uint8_t* file,
                    std::size_t file_size,
                    std::uint32_t slot_rva,
                    IatSlotResolution* resolution,
                    std::string* error)
{
    if (file == nullptr || resolution == nullptr || error == nullptr)
    {
        return false;
    }
    *resolution = {};
    const exe::PeDataDirectory* directory = info.Directory(exe::PeDirectoryIndex::kImport);
    if (directory == nullptr || directory->virtual_address == 0 || directory->size == 0)
    {
        *error = "PE image has no import directory";
        return false;
    }
    for (std::uint32_t offset = 0; offset + kImportDescriptorSize <= directory->size;
         offset += kImportDescriptorSize)
    {
        const std::uint8_t* descriptor = RvaPointer(
            info, file, file_size, directory->virtual_address + offset, kImportDescriptorSize);
        if (descriptor == nullptr)
        {
            *error = "import descriptor lies outside original file";
            return false;
        }
        const std::uint32_t original_first_thunk = ReadU32(descriptor);
        const std::uint32_t name_rva = ReadU32(descriptor + 12);
        const std::uint32_t first_thunk = ReadU32(descriptor + 16);
        if (original_first_thunk == 0 && ReadU32(descriptor + 4) == 0 &&
            ReadU32(descriptor + 8) == 0 && name_rva == 0 && first_thunk == 0)
        {
            break;
        }
        std::string imported_module;
        if (!ReadCString(info, file, file_size, name_rva, &imported_module))
        {
            continue;
        }
        const std::uint32_t lookup_rva =
            original_first_thunk == 0 ? first_thunk : original_first_thunk;
        for (std::uint32_t index = 0; index <= info.size_of_image / 4; ++index)
        {
            const std::uint64_t offset_bytes = static_cast<std::uint64_t>(index) * 4;
            if (lookup_rva > (std::numeric_limits<std::uint32_t>::max)() - offset_bytes ||
                first_thunk > (std::numeric_limits<std::uint32_t>::max)() - offset_bytes)
            {
                *error = "import thunk RVA overflows";
                return false;
            }
            const std::uint32_t current_slot_rva =
                first_thunk + static_cast<std::uint32_t>(offset_bytes);
            const std::uint8_t* thunk = RvaPointer(
                info, file, file_size, lookup_rva + static_cast<std::uint32_t>(offset_bytes), 4);
            if (thunk == nullptr)
            {
                *error = "import lookup thunk lies outside original file";
                return false;
            }
            const std::uint32_t value = ReadU32(thunk);
            if (value == 0)
            {
                break;
            }
            if (current_slot_rva == slot_rva)
            {
                resolution->module = imported_module;
                if ((value & kImportByOrdinalFlag32) != 0)
                {
                    resolution->is_ordinal = true;
                    resolution->ordinal = static_cast<std::uint16_t>(value);
                }
                else
                {
                    resolution->is_ordinal = false;
                    if (!ReadCString(info, file, file_size, value + 2, &resolution->function))
                    {
                        *error = "import function name is malformed";
                        return false;
                    }
                }
                return true;
            }
        }
    }
    *error = "requested IAT slot was not found in import descriptors";
    return false;
}

}  // namespace re2dj::tools::windows_original_process_probe
