#define NOMINMAX
#include <windows.h>

#include "native_pe_image.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace re2dj::platform::windows
{
namespace
{

constexpr std::uint32_t kRelocationBlockHeaderSize = 8;
constexpr std::uint16_t kRelocationAbsolute = 0;
constexpr std::uint16_t kRelocationHighLow = 3;
constexpr std::uint32_t kTlsDirectory32Size = 24;
constexpr std::uint32_t kDllProcessAttach = 1;

std::uint8_t* ImagePointer(const NativePeImage& image,
                           std::uint32_t rva,
                           std::uint32_t size)
{
    if (rva > image.size || size > image.size - rva)
    {
        return nullptr;
    }
    return static_cast<std::uint8_t*>(image.memory) + rva;
}

std::uint16_t ReadU16(const std::uint8_t* bytes)
{
    return static_cast<std::uint16_t>(bytes[0]) |
           static_cast<std::uint16_t>(bytes[1] << 8);
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

DWORD SectionProtection(std::uint32_t characteristics)
{
    constexpr std::uint32_t kExecute = 0x20000000;
    constexpr std::uint32_t kWrite = 0x80000000;
    if ((characteristics & kExecute) != 0)
    {
        return (characteristics & kWrite) != 0 ? PAGE_EXECUTE_READWRITE
                                                : PAGE_EXECUTE_READ;
    }
    return (characteristics & kWrite) != 0 ? PAGE_READWRITE : PAGE_READONLY;
}

bool ApplyRelocations(const exe::PeImageInfo& info,
                      NativePeImage* image,
                      std::string* error)
{
    const std::int64_t delta = static_cast<std::int64_t>(image->load_base) -
                               static_cast<std::int64_t>(info.image_base);
    if (delta == 0)
    {
        return true;
    }

    const exe::PeDataDirectory* directory =
        info.Directory(exe::PeDirectoryIndex::kBaseRelocation);
    if (directory == nullptr || directory->virtual_address == 0 || directory->size == 0)
    {
        *error = "image requires relocation but has no base relocation directory";
        return false;
    }
    if (ImagePointer(*image, directory->virtual_address, directory->size) == nullptr)
    {
        *error = "base relocation directory lies outside the mapped image";
        return false;
    }

    std::uint32_t consumed = 0;
    while (consumed < directory->size)
    {
        if (directory->size - consumed < kRelocationBlockHeaderSize)
        {
            *error = "base relocation block header is truncated";
            return false;
        }
        std::uint8_t* block =
            ImagePointer(*image, directory->virtual_address + consumed, 8);
        const std::uint32_t page_rva = ReadU32(block);
        const std::uint32_t block_size = ReadU32(block + 4);
        if (block_size < kRelocationBlockHeaderSize ||
            block_size > directory->size - consumed ||
            ((block_size - kRelocationBlockHeaderSize) % 2) != 0)
        {
            *error = "base relocation block has an invalid size";
            return false;
        }

        const std::uint32_t entry_count =
            (block_size - kRelocationBlockHeaderSize) / 2;
        for (std::uint32_t index = 0; index < entry_count; ++index)
        {
            const std::uint8_t* entry_bytes = ImagePointer(
                *image,
                directory->virtual_address + consumed +
                    kRelocationBlockHeaderSize + index * 2,
                2);
            const std::uint16_t entry = ReadU16(entry_bytes);
            const std::uint16_t type = entry >> 12;
            if (type == kRelocationAbsolute)
            {
                continue;
            }
            if (type != kRelocationHighLow)
            {
                *error = "unsupported PE32 base relocation type";
                return false;
            }
            const std::uint64_t target_rva =
                static_cast<std::uint64_t>(page_rva) + (entry & 0x0FFF);
            if (target_rva > (std::numeric_limits<std::uint32_t>::max)())
            {
                *error = "base relocation target overflows the guest address space";
                return false;
            }
            std::uint8_t* target =
                ImagePointer(*image, static_cast<std::uint32_t>(target_rva), 4);
            if (target == nullptr)
            {
                *error = "base relocation target lies outside the mapped image";
                return false;
            }
            const std::uint32_t relocated = static_cast<std::uint32_t>(
                static_cast<std::int64_t>(ReadU32(target)) + delta);
            WriteU32(target, relocated);
        }
        consumed += block_size;
    }
    return true;
}

bool ProtectImage(const exe::PeImageInfo& info,
                  NativePeImage* image,
                  std::string* error)
{
    DWORD previous = 0;
    if (!VirtualProtect(image->memory,
                        info.size_of_headers,
                        PAGE_READONLY,
                        &previous))
    {
        *error = "cannot protect PE headers";
        return false;
    }
    for (const exe::PeSection& section : info.sections)
    {
        const std::uint32_t virtual_size =
            section.virtual_size != 0 ? section.virtual_size : section.raw_size;
        if (virtual_size == 0)
        {
            continue;
        }
        if (!VirtualProtect(ImagePointer(*image, section.virtual_address, virtual_size),
                            virtual_size,
                            SectionProtection(section.characteristics),
                            &previous))
        {
            *error = "cannot apply PE section protection";
            return false;
        }
    }
    FlushInstructionCache(GetCurrentProcess(), image->memory, image->size);
    return true;
}

}  // namespace

bool MapNativePeImage(std::span<const std::uint8_t> file,
                      const exe::PeImageInfo& info,
                      std::uint32_t requested_base,
                      std::uintptr_t bridge_address,
                      std::uintptr_t cleanup_address,
                      runtime::ImportGateTable* gates,
                      NativePeImage* image,
                      std::string* error)
{
    if (file.empty() || gates == nullptr || image == nullptr || error == nullptr ||
        image->memory != nullptr || !exe::IsGuestExecutable(info) ||
        info.image_base > (std::numeric_limits<std::uint32_t>::max)() ||
        info.size_of_image == 0 || info.entry_point_rva >= info.size_of_image ||
        info.size_of_headers > info.size_of_image ||
        info.size_of_headers > file.size())
    {
        if (error != nullptr)
        {
            *error = "invalid native PE image arguments";
        }
        return false;
    }

    image->load_base = requested_base == 0
                           ? static_cast<std::uint32_t>(info.image_base)
                           : requested_base;
    if (static_cast<std::uint64_t>(image->load_base) + info.size_of_image >
            (std::uint64_t{1} << 32) ||
        image->load_base >
            (std::numeric_limits<std::uint32_t>::max)() - info.entry_point_rva)
    {
        *error = "native PE image range overflows the guest address space";
        *image = {};
        return false;
    }
    image->size = info.size_of_image;
    image->entry_point = image->load_base + info.entry_point_rva;
    image->memory = VirtualAlloc(
        reinterpret_cast<void*>(static_cast<std::uintptr_t>(image->load_base)),
        image->size,
        MEM_RESERVE | MEM_COMMIT,
        PAGE_READWRITE);
    if (image->memory == nullptr ||
        reinterpret_cast<std::uintptr_t>(image->memory) != image->load_base)
    {
        *error = "cannot reserve the PE image at the requested base";
        ReleaseNativePeImage(image);
        return false;
    }

    std::memcpy(image->memory, file.data(), info.size_of_headers);
    for (const exe::PeSection& section : info.sections)
    {
        const std::uint32_t virtual_size =
            section.virtual_size != 0 ? section.virtual_size : section.raw_size;
        const std::uint32_t copy_size = std::min(section.raw_size, virtual_size);
        std::uint8_t* destination =
            ImagePointer(*image, section.virtual_address, virtual_size);
        if (destination == nullptr || section.raw_offset > file.size() ||
            copy_size > file.size() - section.raw_offset)
        {
            *error = "PE section lies outside the file or mapped image";
            ReleaseNativePeImage(image);
            return false;
        }
        if (copy_size != 0)
        {
            std::memcpy(destination, file.data() + section.raw_offset, copy_size);
        }
    }

    if (!ApplyRelocations(info, image, error) ||
        !BindNativeImportThunks(info,
                                image->memory,
                                image->size,
                                bridge_address,
                                cleanup_address,
                                gates,
                                &image->import_thunks,
                                error) ||
        !ProtectImage(info, image, error))
    {
        ReleaseNativePeImage(image);
        return false;
    }
    return true;
}

bool RunNativeTlsCallbacks(const exe::PeImageInfo& info,
                           const NativePeImage& image,
                           std::string* error)
{
    const exe::PeDataDirectory* directory = info.Directory(exe::PeDirectoryIndex::kTls);
    if (directory == nullptr || directory->virtual_address == 0 || directory->size == 0)
    {
        return true;
    }
    if (directory->size < kTlsDirectory32Size)
    {
        *error = "PE32 TLS directory is truncated";
        return false;
    }
    const std::uint8_t* tls =
        ImagePointer(image, directory->virtual_address, kTlsDirectory32Size);
    if (tls == nullptr)
    {
        *error = "PE32 TLS directory lies outside the mapped image";
        return false;
    }
    const std::uint32_t callbacks_va = ReadU32(tls + 12);
    if (callbacks_va == 0)
    {
        return true;
    }
    if (callbacks_va < image.load_base || callbacks_va - image.load_base >= image.size)
    {
        *error = "PE32 TLS callback array lies outside the mapped image";
        return false;
    }

    const std::uint32_t callbacks_rva = callbacks_va - image.load_base;
    bool terminated = false;
    for (std::uint32_t index = 0; index <= image.size / 4; ++index)
    {
        if (index >
            ((std::numeric_limits<std::uint32_t>::max)() - callbacks_rva) / 4)
        {
            break;
        }
        const std::uint8_t* slot =
            ImagePointer(image, callbacks_rva + index * 4, 4);
        if (slot == nullptr)
        {
            break;
        }
        const std::uint32_t callback_va = ReadU32(slot);
        if (callback_va == 0)
        {
            terminated = true;
            break;
        }
        if (callback_va < image.load_base || callback_va - image.load_base >= image.size)
        {
            *error = "PE32 TLS callback target lies outside the mapped image";
            return false;
        }
        using TlsCallback = void(__stdcall*)(void*, std::uint32_t, void*);
#pragma warning(suppress : 4191)
        const TlsCallback callback = reinterpret_cast<TlsCallback>(
            static_cast<std::uintptr_t>(callback_va));
        callback(image.memory, kDllProcessAttach, nullptr);
    }
    if (!terminated)
    {
        *error = "PE32 TLS callback array is not terminated";
        return false;
    }
    return true;
}

void ReleaseNativePeImage(NativePeImage* image)
{
    if (image == nullptr)
    {
        return;
    }
    ReleaseNativeImportThunks(&image->import_thunks);
    if (image->memory != nullptr)
    {
        VirtualFree(image->memory, 0, MEM_RELEASE);
    }
    *image = {};
}

}  // namespace re2dj::platform::windows
