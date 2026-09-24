#include "re2dj/hle/modules/guest_pe_facade.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "re2dj/exe/pe_image.h"

namespace re2dj::hle::modules
{
namespace
{

constexpr std::uint32_t kPeOffset = 0x80;
constexpr std::uint32_t kFileHeaderOffset = kPeOffset + 4;
constexpr std::uint32_t kOptionalHeaderOffset = kFileHeaderOffset + 20;
constexpr std::uint16_t kOptionalHeaderSize = 224;
constexpr std::uint32_t kSectionTableOffset = kOptionalHeaderOffset + kOptionalHeaderSize;
constexpr std::uint32_t kHeaderSize = 0x200;
constexpr std::uint32_t kFileAlignment = 0x200;
constexpr std::uint32_t kSectionAlignment = 0x1000;
constexpr std::uint32_t kExportDirectorySize = 40;
constexpr std::uint32_t kThunkSize = 19;
constexpr std::uint32_t kSectionReadInitializedData = 0x40000040;
constexpr std::uint32_t kSectionExecuteReadCode = 0x60000020;
constexpr std::uint16_t kFileCharacteristics = 0x210E;
constexpr std::uint64_t kGuestAddressLimit =
    static_cast<std::uint64_t>((std::numeric_limits<std::uint32_t>::max)()) + 1U;

struct NamedExport
{
    std::size_t descriptor_index = 0;
    std::uint32_t string_offset = 0;
};

void SetError(std::string* error, const char* message)
{
    if (error != nullptr)
    {
        *error = message;
    }
}

bool Add(std::uint32_t left, std::uint32_t right, std::uint32_t* result)
{
    const std::uint64_t value = static_cast<std::uint64_t>(left) + right;
    if (value > (std::numeric_limits<std::uint32_t>::max)())
    {
        return false;
    }
    *result = static_cast<std::uint32_t>(value);
    return true;
}

bool AddSize(std::uint32_t value, std::size_t amount, std::uint32_t* result)
{
    if (amount > (std::numeric_limits<std::uint32_t>::max)())
    {
        return false;
    }
    return Add(value, static_cast<std::uint32_t>(amount), result);
}

bool AddStringSize(std::uint32_t value,
                   std::string_view text,
                   std::uint32_t* result)
{
    std::uint32_t string_end = 0;
    return AddSize(value, text.size(), &string_end) && Add(string_end, 1, result);
}

bool Multiply(std::uint32_t left, std::uint32_t right, std::uint32_t* result)
{
    const std::uint64_t value = static_cast<std::uint64_t>(left) * right;
    if (value > (std::numeric_limits<std::uint32_t>::max)())
    {
        return false;
    }
    *result = static_cast<std::uint32_t>(value);
    return true;
}

bool AlignUp(std::uint32_t value, std::uint32_t alignment, std::uint32_t* result)
{
    if (alignment == 0)
    {
        return false;
    }
    const std::uint32_t remainder = value % alignment;
    if (remainder == 0)
    {
        *result = value;
        return true;
    }
    return Add(value, alignment - remainder, result);
}

void WriteU16(std::vector<std::uint8_t>* bytes,
              std::uint32_t offset,
              std::uint16_t value)
{
    (*bytes)[offset] = static_cast<std::uint8_t>(value);
    (*bytes)[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void WriteU32(std::vector<std::uint8_t>* bytes,
              std::uint32_t offset,
              std::uint32_t value)
{
    for (std::uint32_t index = 0; index < 4; ++index)
    {
        (*bytes)[offset + index] = static_cast<std::uint8_t>(value >> (index * 8));
    }
}

void WriteString(std::vector<std::uint8_t>* bytes,
                 std::uint32_t offset,
                 std::string_view value)
{
    for (std::size_t index = 0; index < value.size(); ++index)
    {
        (*bytes)[offset + index] = static_cast<std::uint8_t>(value[index]);
    }
}

void WriteSectionName(std::vector<std::uint8_t>* bytes,
                      std::uint32_t offset,
                      std::string_view name)
{
    for (std::size_t index = 0; index < name.size() && index < 8; ++index)
    {
        (*bytes)[offset + index] = static_cast<std::uint8_t>(name[index]);
    }
}

bool BytewiseLess(std::string_view left, std::string_view right)
{
    const std::size_t common = (std::min)(left.size(), right.size());
    for (std::size_t index = 0; index < common; ++index)
    {
        const auto left_byte = static_cast<std::uint8_t>(left[index]);
        const auto right_byte = static_cast<std::uint8_t>(right[index]);
        if (left_byte != right_byte)
        {
            return left_byte < right_byte;
        }
    }
    return left.size() < right.size();
}

bool AssignOrdinals(const GuestModuleDescriptor& descriptor,
                    std::vector<std::uint16_t>* assigned,
                    std::string* error)
{
    if (descriptor.exports.size() > (std::numeric_limits<std::uint16_t>::max)())
    {
        SetError(error, "guest facade has too many exports");
        return false;
    }

    std::unordered_set<std::uint16_t> used;
    for (const GuestExportDescriptor& export_descriptor : descriptor.exports)
    {
        if (export_descriptor.ordinal.has_value())
        {
            used.insert(export_descriptor.ordinal.value());
        }
    }

    assigned->reserve(descriptor.exports.size());
    std::uint32_t candidate = 1;
    for (const GuestExportDescriptor& export_descriptor : descriptor.exports)
    {
        if (export_descriptor.ordinal.has_value())
        {
            assigned->push_back(export_descriptor.ordinal.value());
            continue;
        }
        while (candidate <= (std::numeric_limits<std::uint16_t>::max)() &&
               used.contains(static_cast<std::uint16_t>(candidate)))
        {
            ++candidate;
        }
        if (candidate > (std::numeric_limits<std::uint16_t>::max)())
        {
            SetError(error, "guest facade exhausted the ordinal space");
            return false;
        }
        const auto ordinal = static_cast<std::uint16_t>(candidate);
        assigned->push_back(ordinal);
        used.insert(ordinal);
        ++candidate;
    }
    return true;
}

bool ValidateOptions(const GuestModuleDescriptor& descriptor,
                     const GuestPeFacadeBuildOptions& options,
                     std::string* error)
{
    if (options.image_base.value() == 0 ||
        (options.image_base.value() & 0xFFFFU) != 0 ||
        options.bridge_address.value() == 0 ||
        options.cleanup_address.value() == 0 ||
        options.export_gates.size() != descriptor.exports.size())
    {
        SetError(error, "invalid guest facade build options");
        return false;
    }
    for (const runtime::GuestAddress gate : options.export_gates)
    {
        if (gate.value() == 0)
        {
            SetError(error, "guest facade export gate is zero");
            return false;
        }
    }
    return true;
}

}  // namespace

bool GuestPeFacadeBuilder::Build(const GuestModuleDescriptor& descriptor,
                                 const GuestPeFacadeBuildOptions& options,
                                 GuestPeFacadeImage* image,
                                 std::string* error)
{
    if (image == nullptr)
    {
        SetError(error, "guest facade output is null");
        return false;
    }
    if (!ValidateGuestModuleDescriptor(descriptor, error) ||
        !ValidateOptions(descriptor, options, error))
    {
        return false;
    }

    GuestPeFacadeImage built;
    built.preferred_base = options.image_base;
    if (!AssignOrdinals(descriptor, &built.export_ordinals, error))
    {
        return false;
    }

    const auto minimum_ordinal = *std::min_element(built.export_ordinals.begin(),
                                                   built.export_ordinals.end());
    const auto maximum_ordinal = *std::max_element(built.export_ordinals.begin(),
                                                   built.export_ordinals.end());
    const std::uint32_t function_count =
        static_cast<std::uint32_t>(maximum_ordinal) - minimum_ordinal + 1U;

    std::vector<NamedExport> named_exports;
    for (std::size_t index = 0; index < descriptor.exports.size(); ++index)
    {
        if (!descriptor.exports[index].name.empty())
        {
            named_exports.push_back({index, 0});
        }
    }
    std::sort(named_exports.begin(), named_exports.end(),
              [&descriptor](const NamedExport& left, const NamedExport& right)
              {
                  return BytewiseLess(descriptor.exports[left.descriptor_index].name,
                                      descriptor.exports[right.descriptor_index].name);
              });

    std::uint32_t cursor = kExportDirectorySize;
    const std::uint32_t eat_offset = cursor;
    std::uint32_t table_size = 0;
    if (!Multiply(function_count, 4, &table_size) || !Add(cursor, table_size, &cursor))
    {
        SetError(error, "guest facade export address table overflows");
        return false;
    }
    const std::uint32_t name_pointer_offset = cursor;
    if (named_exports.size() > (std::numeric_limits<std::uint32_t>::max)() ||
        !Multiply(static_cast<std::uint32_t>(named_exports.size()), 4, &table_size) ||
        !Add(cursor, table_size, &cursor))
    {
        SetError(error, "guest facade name pointer table overflows");
        return false;
    }
    const std::uint32_t name_ordinal_offset = cursor;
    if (!Multiply(static_cast<std::uint32_t>(named_exports.size()), 2, &table_size) ||
        !Add(cursor, table_size, &cursor))
    {
        SetError(error, "guest facade name ordinal table overflows");
        return false;
    }
    const std::uint32_t module_name_offset = cursor;
    if (!AddStringSize(cursor, descriptor.name, &cursor))
    {
        SetError(error, "guest facade module name overflows");
        return false;
    }
    for (NamedExport& named_export : named_exports)
    {
        named_export.string_offset = cursor;
        if (!AddStringSize(cursor,
                           descriptor.exports[named_export.descriptor_index].name,
                           &cursor))
        {
            SetError(error, "guest facade export names overflow");
            return false;
        }
    }
    const std::uint32_t edata_virtual_size = cursor;
    std::uint32_t edata_raw_size = 0;
    if (!AlignUp(edata_virtual_size, kFileAlignment, &edata_raw_size))
    {
        SetError(error, "guest facade export section alignment overflows");
        return false;
    }

    constexpr std::uint32_t edata_rva = kSectionAlignment;
    std::uint32_t edata_end = 0;
    std::uint32_t text_rva = 0;
    if (!Add(edata_rva, edata_virtual_size, &edata_end) ||
        !AlignUp(edata_end, kSectionAlignment, &text_rva))
    {
        SetError(error, "guest facade text RVA overflows");
        return false;
    }
    std::uint32_t text_virtual_size = 0;
    if (!Multiply(static_cast<std::uint32_t>(descriptor.exports.size()),
                  kThunkSize,
                  &text_virtual_size))
    {
        SetError(error, "guest facade thunk section overflows");
        return false;
    }
    std::uint32_t text_raw_size = 0;
    if (!AlignUp(text_virtual_size, kFileAlignment, &text_raw_size))
    {
        SetError(error, "guest facade thunk alignment overflows");
        return false;
    }
    std::uint32_t text_end = 0;
    if (!Add(text_rva, text_virtual_size, &text_end) ||
        !AlignUp(text_end, kSectionAlignment, &built.image_size) ||
        static_cast<std::uint64_t>(options.image_base.value()) + built.image_size >
            kGuestAddressLimit)
    {
        SetError(error, "guest facade image exceeds the 32-bit address space");
        return false;
    }

    constexpr std::uint32_t edata_raw_offset = kHeaderSize;
    std::uint32_t text_raw_offset = 0;
    std::uint32_t file_size = 0;
    if (!Add(edata_raw_offset, edata_raw_size, &text_raw_offset) ||
        !Add(text_raw_offset, text_raw_size, &file_size))
    {
        SetError(error, "guest facade file layout overflows");
        return false;
    }
    built.file_bytes.assign(file_size, 0);

    built.file_bytes[0] = 'M';
    built.file_bytes[1] = 'Z';
    WriteU32(&built.file_bytes, 0x3C, kPeOffset);
    built.file_bytes[kPeOffset] = 'P';
    built.file_bytes[kPeOffset + 1] = 'E';
    WriteU16(&built.file_bytes, kFileHeaderOffset, exe::kMachineI386);
    WriteU16(&built.file_bytes, kFileHeaderOffset + 2, 2);
    WriteU16(&built.file_bytes, kFileHeaderOffset + 16, kOptionalHeaderSize);
    WriteU16(&built.file_bytes, kFileHeaderOffset + 18, kFileCharacteristics);

    WriteU16(&built.file_bytes,
             kOptionalHeaderOffset,
             static_cast<std::uint16_t>(exe::PeMagic::kPe32));
    WriteU32(&built.file_bytes, kOptionalHeaderOffset + 4, text_raw_size);
    WriteU32(&built.file_bytes, kOptionalHeaderOffset + 8, edata_raw_size);
    WriteU32(&built.file_bytes, kOptionalHeaderOffset + 20, text_rva);
    WriteU32(&built.file_bytes, kOptionalHeaderOffset + 24, edata_rva);
    WriteU32(&built.file_bytes, kOptionalHeaderOffset + 28, options.image_base.value());
    WriteU32(&built.file_bytes, kOptionalHeaderOffset + 32, kSectionAlignment);
    WriteU32(&built.file_bytes, kOptionalHeaderOffset + 36, kFileAlignment);
    WriteU16(&built.file_bytes, kOptionalHeaderOffset + 40, 4);
    WriteU16(&built.file_bytes, kOptionalHeaderOffset + 48, 4);
    WriteU32(&built.file_bytes, kOptionalHeaderOffset + 56, built.image_size);
    WriteU32(&built.file_bytes, kOptionalHeaderOffset + 60, kHeaderSize);
    WriteU16(&built.file_bytes, kOptionalHeaderOffset + 68, exe::kSubsystemWindowsGui);
    WriteU32(&built.file_bytes, kOptionalHeaderOffset + 72, 0x00100000);
    WriteU32(&built.file_bytes, kOptionalHeaderOffset + 76, 0x00001000);
    WriteU32(&built.file_bytes, kOptionalHeaderOffset + 80, 0x00100000);
    WriteU32(&built.file_bytes, kOptionalHeaderOffset + 84, 0x00001000);
    WriteU32(&built.file_bytes, kOptionalHeaderOffset + 92, 16);
    WriteU32(&built.file_bytes, kOptionalHeaderOffset + 96, edata_rva);
    WriteU32(&built.file_bytes, kOptionalHeaderOffset + 100, edata_virtual_size);

    WriteSectionName(&built.file_bytes, kSectionTableOffset, ".edata");
    WriteU32(&built.file_bytes, kSectionTableOffset + 8, edata_virtual_size);
    WriteU32(&built.file_bytes, kSectionTableOffset + 12, edata_rva);
    WriteU32(&built.file_bytes, kSectionTableOffset + 16, edata_raw_size);
    WriteU32(&built.file_bytes, kSectionTableOffset + 20, edata_raw_offset);
    WriteU32(&built.file_bytes, kSectionTableOffset + 36, kSectionReadInitializedData);

    const std::uint32_t text_section = kSectionTableOffset + 40;
    WriteSectionName(&built.file_bytes, text_section, ".text");
    WriteU32(&built.file_bytes, text_section + 8, text_virtual_size);
    WriteU32(&built.file_bytes, text_section + 12, text_rva);
    WriteU32(&built.file_bytes, text_section + 16, text_raw_size);
    WriteU32(&built.file_bytes, text_section + 20, text_raw_offset);
    WriteU32(&built.file_bytes, text_section + 36, kSectionExecuteReadCode);

    WriteU32(&built.file_bytes, edata_raw_offset + 12, edata_rva + module_name_offset);
    WriteU32(&built.file_bytes, edata_raw_offset + 16, minimum_ordinal);
    WriteU32(&built.file_bytes, edata_raw_offset + 20, function_count);
    WriteU32(&built.file_bytes,
             edata_raw_offset + 24,
             static_cast<std::uint32_t>(named_exports.size()));
    WriteU32(&built.file_bytes, edata_raw_offset + 28, edata_rva + eat_offset);
    WriteU32(&built.file_bytes,
             edata_raw_offset + 32,
             edata_rva + name_pointer_offset);
    WriteU32(&built.file_bytes,
             edata_raw_offset + 36,
             edata_rva + name_ordinal_offset);

    built.export_thunk_rvas.reserve(descriptor.exports.size());
    for (std::size_t index = 0; index < descriptor.exports.size(); ++index)
    {
        const std::uint32_t thunk_rva =
            text_rva + static_cast<std::uint32_t>(index) * kThunkSize;
        built.export_thunk_rvas.push_back(thunk_rva);
        const std::uint32_t eat_index = built.export_ordinals[index] - minimum_ordinal;
        WriteU32(&built.file_bytes,
                 edata_raw_offset + eat_offset + eat_index * 4,
                 thunk_rva);

        const std::uint32_t thunk_offset =
            text_raw_offset + static_cast<std::uint32_t>(index) * kThunkSize;
        built.file_bytes[thunk_offset] = 0x68;
        WriteU32(&built.file_bytes, thunk_offset + 1, options.export_gates[index].value());
        built.file_bytes[thunk_offset + 5] = 0xE8;
        const std::uint32_t next_instruction =
            options.image_base.value() + thunk_rva + 10U;
        WriteU32(&built.file_bytes,
                 thunk_offset + 6,
                 options.bridge_address.value() - next_instruction);
        built.file_bytes[thunk_offset + 10] = 0x59;
        built.file_bytes[thunk_offset + 11] = 0x03;
        built.file_bytes[thunk_offset + 12] = 0x25;
        WriteU32(&built.file_bytes,
                 thunk_offset + 13,
                 options.cleanup_address.value());
        built.file_bytes[thunk_offset + 17] = 0xFF;
        built.file_bytes[thunk_offset + 18] = 0xE1;
    }

    WriteString(&built.file_bytes,
                edata_raw_offset + module_name_offset,
                descriptor.name);
    for (std::size_t index = 0; index < named_exports.size(); ++index)
    {
        const NamedExport& named_export = named_exports[index];
        WriteU32(&built.file_bytes,
                 edata_raw_offset + name_pointer_offset +
                     static_cast<std::uint32_t>(index) * 4,
                 edata_rva + named_export.string_offset);
        const std::uint16_t ordinal_index = static_cast<std::uint16_t>(
            built.export_ordinals[named_export.descriptor_index] - minimum_ordinal);
        WriteU16(&built.file_bytes,
                 edata_raw_offset + name_ordinal_offset +
                     static_cast<std::uint32_t>(index) * 2,
                 ordinal_index);
        WriteString(&built.file_bytes,
                    edata_raw_offset + named_export.string_offset,
                    descriptor.exports[named_export.descriptor_index].name);
    }

    *image = std::move(built);
    if (error != nullptr)
    {
        error->clear();
    }
    return true;
}

}  // namespace re2dj::hle::modules
