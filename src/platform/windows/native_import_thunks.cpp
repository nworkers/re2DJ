#define NOMINMAX
#include <windows.h>

#include "native_import_thunks.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "../native_helper_protocol.h"

namespace re2dj::platform::windows
{
namespace
{

namespace protocol = re2dj::platform::native_protocol;

struct ImageView
{
    void* memory = nullptr;
    std::uint32_t size = 0;
};

struct IatBinding
{
    std::uint8_t* slot = nullptr;
    runtime::GuestAddress gate_address;
};

std::uint8_t* ImagePointer(const ImageView& image,
                           std::uint32_t rva,
                           std::uint32_t size)
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

bool ReadImageString(const ImageView& image,
                     std::uint32_t rva,
                     std::string* value)
{
    value->clear();
    for (std::uint32_t index = 0; index < protocol::kMaximumImportStringSize; ++index)
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
            return true;
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
                  std::vector<IatBinding>* bindings,
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

    bool terminated = false;
    for (std::uint32_t descriptor_offset = 0;
         descriptor_offset <= directory->size - 20;
         descriptor_offset += 20)
    {
        std::uint8_t* descriptor =
            ImagePointer(image, directory->virtual_address + descriptor_offset, 20);
        if (IsZeroDescriptor(descriptor))
        {
            terminated = true;
            break;
        }

        const std::uint32_t original_lookup_rva = ReadU32(descriptor);
        const std::uint32_t module_name_rva = ReadU32(descriptor + 12);
        const std::uint32_t iat_rva = ReadU32(descriptor + 16);
        const std::uint32_t lookup_rva =
            original_lookup_rva != 0 ? original_lookup_rva : iat_rva;
        std::string module;
        if (lookup_rva == 0 || iat_rva == 0 ||
            !ReadImageString(image, module_name_rva, &module) || module.empty())
        {
            *error = "import descriptor contains invalid table or module data";
            return false;
        }

        bool thunk_terminated = false;
        for (std::uint32_t thunk_index = 0;
             thunk_index <= image.size / sizeof(std::uint32_t);
             ++thunk_index)
        {
            if (thunk_index >
                    ((std::numeric_limits<std::uint32_t>::max)() - lookup_rva) / 4 ||
                thunk_index >
                    ((std::numeric_limits<std::uint32_t>::max)() - iat_rva) / 4)
            {
                *error = "import thunk offset overflows the guest address space";
                return false;
            }
            std::uint8_t* lookup =
                ImagePointer(image, lookup_rva + thunk_index * 4, 4);
            std::uint8_t* iat = ImagePointer(image, iat_rva + thunk_index * 4, 4);
            if (lookup == nullptr || iat == nullptr)
            {
                *error = "import thunk lies outside the mapped image";
                return false;
            }
            const std::uint32_t lookup_value = ReadU32(lookup);
            if (lookup_value == 0)
            {
                thunk_terminated = true;
                break;
            }

            runtime::GuestAddress gate_address;
            if ((lookup_value & 0x80000000U) != 0)
            {
                if ((lookup_value & 0x7FFF0000U) != 0)
                {
                    *error = "ordinal import uses reserved thunk bits";
                    return false;
                }
                if (!gates->BindByOrdinal(module,
                                          static_cast<std::uint16_t>(lookup_value),
                                          &gate_address,
                                          error))
                {
                    return false;
                }
            }
            else
            {
                std::string name;
                if (lookup_value > (std::numeric_limits<std::uint32_t>::max)() - 2 ||
                    !ReadImageString(image, lookup_value + 2, &name) || name.empty() ||
                    !gates->BindByName(module, name, &gate_address, error))
                {
                    if (error->empty())
                    {
                        *error = "named import data is invalid";
                    }
                    return false;
                }
            }
            bindings->push_back({iat, gate_address});
        }
        if (!thunk_terminated)
        {
            *error = "import thunk table is not terminated";
            return false;
        }
    }
    if (!terminated)
    {
        *error = "import descriptor table is not terminated";
        return false;
    }
    return true;
}

bool EmitThunks(const runtime::ImportGateTable& gates,
                const std::vector<IatBinding>& bindings,
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
    if (gates.gates().size() > protocol::kMaximumImportCount ||
        gates.gates().size() >
            (std::numeric_limits<std::uint32_t>::max)() / kThunkBytes)
    {
        *error = "import count exceeds the native thunk limit";
        return false;
    }

    region->size = static_cast<std::uint32_t>(gates.gates().size()) * kThunkBytes;
    region->memory = VirtualAlloc(nullptr,
                                  region->size,
                                  MEM_RESERVE | MEM_COMMIT,
                                  PAGE_READWRITE);
    if (region->memory == nullptr)
    {
        *error = "cannot allocate native import thunks";
        return false;
    }

    auto* thunk_bytes = static_cast<std::uint8_t*>(region->memory);
    const std::uint32_t bridge = static_cast<std::uint32_t>(bridge_address);
    const std::uint32_t cleanup = static_cast<std::uint32_t>(cleanup_address);
    for (std::size_t index = 0; index < gates.gates().size(); ++index)
    {
        std::uint8_t* thunk = thunk_bytes + index * kThunkBytes;
        const std::uint32_t thunk_address =
            static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(thunk));
        thunk[0] = 0x68;
        WriteU32(thunk + 1, gates.gates()[index].address.value());
        thunk[5] = 0xE8;
        WriteU32(thunk + 6, bridge - (thunk_address + 10));
        thunk[10] = 0x59;
        thunk[11] = 0x03;
        thunk[12] = 0x25;
        WriteU32(thunk + 13, cleanup);
        thunk[17] = 0xFF;
        thunk[18] = 0xE1;
    }

    for (const IatBinding& binding : bindings)
    {
        std::size_t gate_index = 0;
        while (gate_index < gates.gates().size() &&
               gates.gates()[gate_index].address != binding.gate_address)
        {
            ++gate_index;
        }
        if (gate_index == gates.gates().size())
        {
            *error = "IAT binding references an unknown import gate";
            return false;
        }
        const std::uintptr_t thunk = reinterpret_cast<std::uintptr_t>(region->memory) +
                                     gate_index * kThunkBytes;
        WriteU32(binding.slot, static_cast<std::uint32_t>(thunk));
    }

    DWORD previous = 0;
    if (!VirtualProtect(region->memory, region->size, PAGE_EXECUTE_READ, &previous))
    {
        *error = "cannot protect native import thunks";
        return false;
    }
    FlushInstructionCache(GetCurrentProcess(), region->memory, region->size);
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
    if (image_memory == nullptr || bridge_address == 0 || cleanup_address == 0 ||
        bridge_address > (std::numeric_limits<std::uint32_t>::max)() ||
        cleanup_address > (std::numeric_limits<std::uint32_t>::max)() ||
        gates == nullptr || region == nullptr || region->memory != nullptr ||
        region->size != 0 || error == nullptr)
    {
        if (error != nullptr)
        {
            *error = "invalid native import thunk arguments";
        }
        return false;
    }
    const ImageView image = {image_memory, image_size};
    std::vector<IatBinding> bindings;
    if (!ParseImports(info, image, gates, &bindings, error) ||
        !EmitThunks(*gates,
                    bindings,
                    bridge_address,
                    cleanup_address,
                    region,
                    error))
    {
        ReleaseNativeImportThunks(region);
        return false;
    }
    return true;
}

void ReleaseNativeImportThunks(NativeImportThunkRegion* region)
{
    if (region != nullptr && region->memory != nullptr)
    {
        VirtualFree(region->memory, 0, MEM_RELEASE);
        region->memory = nullptr;
        region->size = 0;
    }
}

}  // namespace re2dj::platform::windows
