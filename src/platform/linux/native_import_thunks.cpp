#include "native_import_thunks.h"

#include <sys/mman.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "native_low_memory.h"

namespace re2dj::platform::linux
{
namespace
{

struct ImageView
{
    void* memory = nullptr;
    std::uint32_t size = 0;
};

std::uint8_t* ImagePointer(const ImageView& image, std::uint32_t rva, std::uint32_t size)
{
    if (rva > image.size || size > image.size - rva)
    {
        return nullptr;
    }
    return static_cast<std::uint8_t*>(image.memory) + rva;
}

std::uint32_t ReadU32(const std::uint8_t* bytes)
{
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8) |
           (static_cast<std::uint32_t>(bytes[2]) << 16) |
           (static_cast<std::uint32_t>(bytes[3]) << 24);
}

void WriteU32(std::uint8_t* bytes, std::uint32_t value)
{
    for (std::size_t index = 0; index < 4; ++index)
    {
        bytes[index] = static_cast<std::uint8_t>(value >> (index * 8));
    }
}

bool ReadImageString(const ImageView& image, std::uint32_t rva, std::string* value)
{
    value->clear();
    for (std::uint32_t index = 0; index < kMaximumImportStringSize; ++index)
    {
        if (index > (std::numeric_limits<std::uint32_t>::max)() - rva)
        {
            return false;
        }
        std::uint8_t* byte = ImagePointer(image, rva + index, 1);
        if (byte == nullptr)
        {
            return false;
        }
        if (*byte == 0)
        {
            return !value->empty();
        }
        value->push_back(static_cast<char>(*byte));
    }
    return false;
}

bool IsZeroDescriptor(const std::uint8_t* descriptor)
{
    for (std::size_t index = 0; index < 20; ++index)
    {
        if (descriptor[index] != 0)
        {
            return false;
        }
    }
    return true;
}

bool ParseImports(const exe::PeImageInfo& info,
                  const ImageView& image,
                  runtime::ImportGateTable* gates,
                  std::vector<NativeImportSlotBinding>* bindings,
                  std::string* error)
{
    const exe::PeDataDirectory* directory = info.Directory(exe::PeDirectoryIndex::kImport);
    if (directory == nullptr || directory->virtual_address == 0 || directory->size == 0)
    {
        return true;
    }
    if (directory->size < 20 ||
        ImagePointer(image, directory->virtual_address, directory->size) == nullptr)
    {
        *error = "import directory lies outside the mapped image";
        return false;
    }
    for (std::uint32_t descriptor_offset = 0;
         descriptor_offset <= directory->size - 20;
         descriptor_offset += 20)
    {
        std::uint8_t* descriptor =
            ImagePointer(image, directory->virtual_address + descriptor_offset, 20);
        if (IsZeroDescriptor(descriptor))
        {
            return true;
        }
        const std::uint32_t original_lookup = ReadU32(descriptor);
        const std::uint32_t iat_rva = ReadU32(descriptor + 16);
        const std::uint32_t lookup_rva = original_lookup != 0 ? original_lookup : iat_rva;
        std::string module;
        if (lookup_rva == 0 || iat_rva == 0 ||
            !ReadImageString(image, ReadU32(descriptor + 12), &module))
        {
            *error = "invalid import descriptor";
            return false;
        }
        for (std::uint32_t index = 0; index <= image.size / 4; ++index)
        {
            if (index > ((std::numeric_limits<std::uint32_t>::max)() - lookup_rva) / 4 ||
                index > ((std::numeric_limits<std::uint32_t>::max)() - iat_rva) / 4)
            {
                *error = "import thunk offset overflows";
                return false;
            }
            std::uint8_t* lookup = ImagePointer(image, lookup_rva + index * 4, 4);
            std::uint8_t* iat = ImagePointer(image, iat_rva + index * 4, 4);
            if (lookup == nullptr || iat == nullptr)
            {
                *error = "import thunk lies outside the mapped image";
                return false;
            }
            const std::uint32_t value = ReadU32(lookup);
            if (value == 0)
            {
                break;
            }
            runtime::GuestAddress gate;
            if ((value & 0x80000000U) != 0)
            {
                if ((value & 0x7FFF0000U) != 0 ||
                    !gates->BindByOrdinal(module, static_cast<std::uint16_t>(value), &gate, error))
                {
                    if (error->empty()) *error = "invalid ordinal import";
                    return false;
                }
            }
            else
            {
                std::string name;
                if (value > (std::numeric_limits<std::uint32_t>::max)() - 2 ||
                    !ReadImageString(image, value + 2, &name) ||
                    !gates->BindByName(module, name, &gate, error))
                {
                    if (error->empty()) *error = "invalid named import";
                    return false;
                }
            }
            const runtime::ImportGate* bound_gate = nullptr;
            for (const runtime::ImportGate& candidate : gates->gates())
            {
                if (candidate.address == gate)
                {
                    bound_gate = &candidate;
                    break;
                }
            }
            if (bound_gate == nullptr)
            {
                *error = "bound import gate is missing";
                return false;
            }
            bindings->push_back({iat, *bound_gate});
        }
    }
    *error = "import descriptor table is not terminated";
    return false;
}

bool EmitThunks(const runtime::ImportGateTable& gates,
                const std::vector<NativeImportSlotBinding>& bindings,
                std::uintptr_t bridge_address,
                std::uintptr_t cleanup_address,
                NativeImportThunkRegion* region,
                std::string* error)
{
    constexpr std::uint32_t kThunkBytes = 19;
    if (gates.gates().empty())
    {
        return true;
    }
    if (gates.gates().size() > kMaximumImportCount ||
        gates.gates().size() > (std::numeric_limits<std::uint32_t>::max)() / kThunkBytes)
    {
        *error = "native thunk count exceeds the limit";
        return false;
    }
    // Guest code calls the thunks and the IAT stores their addresses, so they
    // must be guest-addressable.
    NativeLowMemory mapping;
    if (!MapNativeLowMemory(static_cast<std::uint32_t>(gates.gates().size()) * kThunkBytes,
                            PROT_READ | PROT_WRITE, &mapping, error))
    {
        *error = "cannot allocate native import thunks: " + *error;
        return false;
    }
    region->memory = mapping.memory;
    region->size = mapping.size;
    auto* bytes = static_cast<std::uint8_t*>(region->memory);
    const std::uint32_t bridge = static_cast<std::uint32_t>(bridge_address);
    const std::uint32_t cleanup = static_cast<std::uint32_t>(cleanup_address);
    for (std::size_t index = 0; index < gates.gates().size(); ++index)
    {
        std::uint8_t* thunk = bytes + index * kThunkBytes;
        const std::uint32_t address = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(thunk));
        thunk[0] = 0x68;
        WriteU32(thunk + 1, gates.gates()[index].address.value());
        thunk[5] = 0xE8;
        WriteU32(thunk + 6, bridge - (address + 10));
        thunk[10] = 0x59;
        thunk[11] = 0x03;
        thunk[12] = 0x25;
        WriteU32(thunk + 13, cleanup);
        thunk[17] = 0xFF;
        thunk[18] = 0xE1;
    }
    for (const NativeImportSlotBinding& binding : bindings)
    {
        std::size_t index = 0;
        while (index < gates.gates().size() &&
               gates.gates()[index].address != binding.gate.address)
        {
            ++index;
        }
        if (index == gates.gates().size())
        {
            *error = "IAT binding references an unknown gate";
            return false;
        }
        WriteU32(binding.slot, static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(bytes + index * kThunkBytes)));
    }
    if (mprotect(region->memory, region->size, PROT_READ | PROT_EXEC) != 0)
    {
        *error = "cannot protect native import thunks";
        return false;
    }
    __builtin___clear_cache(static_cast<char*>(region->memory),
                            static_cast<char*>(region->memory) + region->size);
    return true;
}

}  // namespace

bool BindNativeImportThunks(const exe::PeImageInfo& info,
                            void* image_memory,
                            std::uint32_t image_size,
                            std::uintptr_t bridge_address,
                            std::uintptr_t cleanup_address,
                            runtime::ImportGateTable* gates,
                            NativeImportThunkRegion* region,
                            std::string* error)
{
    constexpr std::uintptr_t kGuestAddressLimit = (std::numeric_limits<std::uint32_t>::max)();
    if (image_memory == nullptr || bridge_address == 0 || cleanup_address == 0 ||
        bridge_address > kGuestAddressLimit || cleanup_address > kGuestAddressLimit ||
        gates == nullptr || region == nullptr || region->memory != nullptr || error == nullptr)
    {
        if (error != nullptr) *error = "invalid native import thunk arguments";
        return false;
    }
    std::vector<NativeImportSlotBinding> bindings;
    if (!ParseImports(info, {image_memory, image_size}, gates, &bindings, error) ||
        !EmitThunks(*gates, bindings, bridge_address, cleanup_address, region, error))
    {
        ReleaseNativeImportThunks(region);
        return false;
    }
    region->slots = std::move(bindings);
    return true;
}

bool RebindNativeGuestModuleImports(
    NativeImportThunkRegion* region,
    const hle::modules::GuestModuleRegistry& registry,
    std::vector<NativeGuestImportRebinding>* rebindings,
    std::string* error)
{
    if (region == nullptr || region->memory == nullptr || rebindings == nullptr ||
        error == nullptr)
    {
        if (error != nullptr)
        {
            *error = "invalid native guest import rebinding arguments";
        }
        return false;
    }

    std::vector<NativeGuestImportRebinding> staged;
    for (const NativeImportSlotBinding& binding : region->slots)
    {
        const hle::modules::RegisteredGuestExport* export_entry =
            registry.FindExport(binding.gate);
        if (export_entry == nullptr)
        {
            continue;
        }
        WriteU32(binding.slot, export_entry->thunk_address.value());
        staged.push_back({binding.gate, export_entry->thunk_address});
    }
    *rebindings = std::move(staged);
    error->clear();
    return true;
}

void ReleaseNativeImportThunks(NativeImportThunkRegion* region)
{
    if (region != nullptr && region->memory != nullptr)
    {
        munmap(region->memory, region->size);
        *region = {};
    }
}

}  // namespace re2dj::platform::linux
