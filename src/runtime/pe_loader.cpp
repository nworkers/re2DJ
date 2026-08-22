#include "re2dj/runtime/pe_loader.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <utility>

namespace re2dj::runtime
{

namespace
{

constexpr std::uint32_t kImportDescriptorSize = 20;
constexpr std::uint32_t kRelocationBlockHeaderSize = 8;
constexpr std::uint16_t kRelocationAbsolute = 0;
constexpr std::uint16_t kRelocationHighLow = 3;
constexpr std::uint32_t kImportByOrdinalFlag32 = 0x80000000;
constexpr std::size_t kMaximumImportStringLength = 4096;

bool Fail(std::string* error, std::string message)
{
    if (error != nullptr)
    {
        *error = std::move(message);
    }
    return false;
}

std::string NormalizeModule(std::string_view module)
{
    std::string normalized(module);
    for (char& character : normalized)
    {
        const unsigned char byte = static_cast<unsigned char>(character);
        if (byte < 0x80)
        {
            character = static_cast<char>(std::tolower(byte));
        }
    }
    return normalized;
}

bool AddAddress(GuestAddress base,
                std::uint64_t offset,
                GuestAddress* result,
                std::string* error)
{
    const std::uint64_t value = static_cast<std::uint64_t>(base.value()) + offset;
    if (value > std::numeric_limits<std::uint32_t>::max())
    {
        return Fail(error, "guest address addition overflowed");
    }
    *result = GuestAddress(static_cast<std::uint32_t>(value));
    return true;
}

bool ReadCString(const AddressSpace& address_space,
                 GuestAddress address,
                 std::string* value,
                 std::string* error)
{
    value->clear();
    for (std::size_t index = 0; index < kMaximumImportStringLength; ++index)
    {
        GuestAddress current;
        if (!AddAddress(address, static_cast<std::uint32_t>(index), &current, error))
        {
            return false;
        }
        std::uint8_t byte = 0;
        if (!address_space.Read8(current, &byte))
        {
            return Fail(error, "import string lies outside mapped guest memory");
        }
        if (byte == 0)
        {
            return true;
        }
        value->push_back(static_cast<char>(byte));
    }
    return Fail(error, "import string is not terminated");
}

bool ApplyRelocations(const exe::PeImageInfo& info,
                      GuestAddress load_base,
                      AddressSpace* address_space,
                      std::string* error)
{
    const std::int64_t delta = static_cast<std::int64_t>(load_base.value()) -
                               static_cast<std::int64_t>(info.image_base);
    if (delta == 0)
    {
        return true;
    }

    const exe::PeDataDirectory* directory =
        info.Directory(exe::PeDirectoryIndex::kBaseRelocation);
    if (directory == nullptr || directory->virtual_address == 0 || directory->size == 0)
    {
        return Fail(error, "image requires relocation but has no base relocation directory");
    }

    std::uint32_t consumed = 0;
    while (consumed < directory->size)
    {
        if (directory->size - consumed < kRelocationBlockHeaderSize)
        {
            return Fail(error, "base relocation block header is truncated");
        }
        GuestAddress block_address;
        if (!AddAddress(load_base,
                        static_cast<std::uint64_t>(directory->virtual_address) + consumed,
                        &block_address,
                        error))
        {
            return false;
        }
        std::uint32_t page_rva = 0;
        std::uint32_t block_size = 0;
        if (!address_space->Read32(block_address, &page_rva) ||
            !address_space->Read32(block_address + 4, &block_size))
        {
            return Fail(error, "base relocation block lies outside mapped image");
        }
        if (block_size < kRelocationBlockHeaderSize || block_size > directory->size - consumed ||
            ((block_size - kRelocationBlockHeaderSize) % 2) != 0)
        {
            return Fail(error, "base relocation block has an invalid size");
        }

        const std::uint32_t entry_count =
            (block_size - kRelocationBlockHeaderSize) / 2;
        for (std::uint32_t index = 0; index < entry_count; ++index)
        {
            std::uint16_t entry = 0;
            if (!address_space->Read16(block_address + kRelocationBlockHeaderSize + index * 2,
                                       &entry))
            {
                return Fail(error, "base relocation entry lies outside mapped image");
            }
            const std::uint16_t type = entry >> 12;
            if (type == kRelocationAbsolute)
            {
                continue;
            }
            if (type != kRelocationHighLow)
            {
                return Fail(error, "unsupported PE32 base relocation type");
            }

            GuestAddress target;
            if (!AddAddress(load_base,
                            static_cast<std::uint64_t>(page_rva) + (entry & 0x0FFF),
                            &target,
                            error))
            {
                return false;
            }
            std::uint32_t original = 0;
            if (!address_space->Read32(target, &original))
            {
                return Fail(error, "base relocation target lies outside mapped image");
            }
            const std::uint32_t relocated = static_cast<std::uint32_t>(
                static_cast<std::int64_t>(original) + delta);
            if (!address_space->Write32(target, relocated))
            {
                return Fail(error, "cannot write base relocation target");
            }
        }
        consumed += block_size;
    }
    return true;
}

bool BindImports(const exe::PeImageInfo& info,
                 GuestAddress load_base,
                 AddressSpace* address_space,
                 ImportGateTable* gates,
                 std::string* error)
{
    const exe::PeDataDirectory* directory = info.Directory(exe::PeDirectoryIndex::kImport);
    if (directory == nullptr || directory->virtual_address == 0 || directory->size == 0)
    {
        return true;
    }

    bool terminated = false;
    for (std::uint32_t offset = 0;
         offset + kImportDescriptorSize <= directory->size;
         offset += kImportDescriptorSize)
    {
        GuestAddress descriptor;
        if (!AddAddress(load_base,
                        static_cast<std::uint64_t>(directory->virtual_address) + offset,
                        &descriptor,
                        error))
        {
            return false;
        }
        std::uint32_t original_first_thunk = 0;
        std::uint32_t timestamp = 0;
        std::uint32_t forwarder_chain = 0;
        std::uint32_t name_rva = 0;
        std::uint32_t first_thunk = 0;
        if (!address_space->Read32(descriptor, &original_first_thunk) ||
            !address_space->Read32(descriptor + 4, &timestamp) ||
            !address_space->Read32(descriptor + 8, &forwarder_chain) ||
            !address_space->Read32(descriptor + 12, &name_rva) ||
            !address_space->Read32(descriptor + 16, &first_thunk))
        {
            return Fail(error, "import descriptor lies outside mapped image");
        }
        if (original_first_thunk == 0 && timestamp == 0 && forwarder_chain == 0 &&
            name_rva == 0 && first_thunk == 0)
        {
            terminated = true;
            break;
        }
        if (name_rva == 0 || first_thunk == 0)
        {
            return Fail(error, "import descriptor is missing its name or IAT");
        }

        GuestAddress module_address;
        if (!AddAddress(load_base, name_rva, &module_address, error))
        {
            return false;
        }
        std::string module;
        if (!ReadCString(*address_space, module_address, &module, error))
        {
            return false;
        }

        const std::uint32_t lookup_rva =
            original_first_thunk != 0 ? original_first_thunk : first_thunk;
        for (std::uint32_t index = 0;; ++index)
        {
            if (index > info.size_of_image / 4)
            {
                return Fail(error, "import thunk array is not terminated");
            }
            GuestAddress lookup_address;
            GuestAddress iat_address;
            const std::uint64_t thunk_offset = static_cast<std::uint64_t>(index) * 4;
            if (!AddAddress(load_base,
                            static_cast<std::uint64_t>(lookup_rva) + thunk_offset,
                            &lookup_address,
                            error) ||
                !AddAddress(load_base,
                            static_cast<std::uint64_t>(first_thunk) + thunk_offset,
                            &iat_address,
                            error))
            {
                return false;
            }
            std::uint32_t thunk = 0;
            if (!address_space->Read32(lookup_address, &thunk))
            {
                return Fail(error, "import thunk lies outside mapped image");
            }
            if (thunk == 0)
            {
                break;
            }

            GuestAddress gate;
            if ((thunk & kImportByOrdinalFlag32) != 0)
            {
                if (!gates->BindByOrdinal(module,
                                          static_cast<std::uint16_t>(thunk),
                                          &gate,
                                          error))
                {
                    return false;
                }
            }
            else
            {
                GuestAddress import_name_address;
                if (!AddAddress(load_base,
                                static_cast<std::uint64_t>(thunk) + 2,
                                &import_name_address,
                                error))
                {
                    return false;
                }
                std::string import_name;
                if (!ReadCString(*address_space, import_name_address, &import_name, error) ||
                    !gates->BindByName(module, import_name, &gate, error))
                {
                    return false;
                }
            }
            if (address_space->IsMapped(gate, 1, MemoryAccess::kNone))
            {
                return Fail(error, "import gate overlaps mapped guest memory");
            }
            if (!address_space->Write32(iat_address, gate.value()))
            {
                return Fail(error, "cannot write import gate into IAT");
            }
        }
    }
    if (!terminated)
    {
        return Fail(error, "import descriptor table is not terminated");
    }
    return true;
}

}  // namespace

ImportGateTable::ImportGateTable(GuestAddress base, std::uint32_t stride)
    : base_(base), stride_(stride)
{
}

bool ImportGateTable::BindByName(std::string_view module,
                                 std::string_view name,
                                 GuestAddress* address,
                                 std::string* error)
{
    if (name.empty())
    {
        return Fail(error, "cannot bind an empty import name");
    }
    return Bind(module, name, 0, false, address, error);
}

bool ImportGateTable::BindByOrdinal(std::string_view module,
                                    std::uint16_t ordinal,
                                    GuestAddress* address,
                                    std::string* error)
{
    return Bind(module, {}, ordinal, true, address, error);
}

bool ImportGateTable::Bind(std::string_view module,
                           std::string_view name,
                           std::uint16_t ordinal,
                           bool by_ordinal,
                           GuestAddress* address,
                           std::string* error)
{
    if (module.empty() || address == nullptr || stride_ == 0)
    {
        return Fail(error, "invalid import gate request");
    }
    const std::string normalized_module = NormalizeModule(module);
    for (const ImportGate& gate : gates_)
    {
        if (gate.module == normalized_module && gate.by_ordinal == by_ordinal &&
            ((by_ordinal && gate.ordinal == ordinal) || (!by_ordinal && gate.name == name)))
        {
            *address = gate.address;
            return true;
        }
    }

    const std::uint64_t offset = static_cast<std::uint64_t>(gates_.size()) * stride_;
    const std::uint64_t gate_value = static_cast<std::uint64_t>(base_.value()) + offset;
    if (gate_value > std::numeric_limits<std::uint32_t>::max())
    {
        return Fail(error, "import gate address range is exhausted");
    }

    ImportGate gate;
    gate.module = normalized_module;
    gate.name = std::string(name);
    gate.ordinal = ordinal;
    gate.by_ordinal = by_ordinal;
    gate.address = GuestAddress(static_cast<std::uint32_t>(gate_value));
    gates_.push_back(gate);
    *address = gate.address;
    return true;
}

bool LoadPe32Image(const std::uint8_t* file_bytes,
                   std::size_t file_size,
                   const exe::PeImageInfo& info,
                   GuestAddress requested_base,
                   AddressSpace* address_space,
                   ImportGateTable* gates,
                   LoadedPeImage* loaded,
                   std::string* error)
{
    if (file_bytes == nullptr || address_space == nullptr || gates == nullptr || loaded == nullptr)
    {
        return Fail(error, "invalid PE loader argument");
    }
    if (!exe::IsGuestExecutable(info) || info.image_base > std::numeric_limits<std::uint32_t>::max())
    {
        return Fail(error, "loader accepts only 32-bit x86 PE32 executables");
    }
    if (info.size_of_image == 0 || info.entry_point_rva >= info.size_of_image ||
        info.size_of_headers > info.size_of_image ||
        info.size_of_headers > file_size)
    {
        return Fail(error, "PE image size or header size is invalid");
    }

    const GuestAddress load_base = requested_base.value() == 0
                                       ? GuestAddress(static_cast<std::uint32_t>(info.image_base))
                                       : requested_base;
    AddressSpace staged_space = *address_space;
    ImportGateTable staged_gates = *gates;
    const MemoryAccess image_access =
        MemoryAccess::kRead | MemoryAccess::kWrite | MemoryAccess::kExecute;
    if (!staged_space.Map(load_base, info.size_of_image, image_access, error) ||
        !staged_space.WriteBytes(load_base, file_bytes, info.size_of_headers))
    {
        return Fail(error, error != nullptr && !error->empty() ? *error : "cannot map PE headers");
    }

    for (const exe::PeSection& section : info.sections)
    {
        const std::uint32_t virtual_size =
            section.virtual_size != 0 ? section.virtual_size : section.raw_size;
        if (section.virtual_address > info.size_of_image ||
            virtual_size > info.size_of_image - section.virtual_address)
        {
            return Fail(error, "PE section lies outside SizeOfImage");
        }
        const std::uint32_t copy_size = std::min(section.raw_size, virtual_size);
        if (copy_size != 0 &&
            (section.raw_offset > file_size || copy_size > file_size - section.raw_offset))
        {
            return Fail(error, "PE section raw data lies outside the file");
        }
        GuestAddress destination;
        if (!AddAddress(load_base, section.virtual_address, &destination, error) ||
            (copy_size != 0 &&
             !staged_space.WriteBytes(destination, file_bytes + section.raw_offset, copy_size)))
        {
            return Fail(error, error != nullptr && !error->empty() ? *error
                                                                   : "cannot copy PE section");
        }
    }

    if (!ApplyRelocations(info, load_base, &staged_space, error) ||
        !BindImports(info, load_base, &staged_space, &staged_gates, error))
    {
        return false;
    }

    GuestAddress entry_point;
    if (!AddAddress(load_base, info.entry_point_rva, &entry_point, error))
    {
        return false;
    }
    LoadedPeImage result;
    result.load_base = load_base;
    result.entry_point = entry_point;
    const exe::PeDataDirectory* tls = info.Directory(exe::PeDirectoryIndex::kTls);
    if (tls != nullptr)
    {
        result.tls_directory_rva = tls->virtual_address;
        result.tls_directory_size = tls->size;
    }
    result.imports = staged_gates.gates();
    *address_space = std::move(staged_space);
    *gates = std::move(staged_gates);
    *loaded = std::move(result);
    return true;
}

}  // namespace re2dj::runtime
