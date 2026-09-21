#include "native_pe_image.h"

#include <sys/mman.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>

namespace re2dj::platform::linux
{
namespace
{

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

bool ApplyHighLowRelocations(const exe::PeImageInfo& info, NativePeImage* image)
{
    const std::int64_t delta = static_cast<std::int64_t>(image->entry_point - info.entry_point_rva) -
                               static_cast<std::int64_t>(info.image_base);
    if (delta == 0)
    {
        return true;
    }
    const auto* directory = info.Directory(exe::PeDirectoryIndex::kBaseRelocation);
    if (directory == nullptr || directory->virtual_address == 0 || directory->size < 8 ||
        directory->virtual_address > image->size ||
        directory->size > image->size - directory->virtual_address)
    {
        return false;
    }
    auto* bytes = static_cast<std::uint8_t*>(image->memory);
    std::uint32_t offset = 0;
    while (offset < directory->size)
    {
        if (directory->size - offset < 8)
        {
            return false;
        }
        const std::uint32_t page_rva = ReadU32(bytes + directory->virtual_address + offset);
        const std::uint32_t block_size = ReadU32(bytes + directory->virtual_address + offset + 4);
        if (block_size < 8 || block_size > directory->size - offset || (block_size & 1U) != 0)
        {
            return false;
        }
        for (std::uint32_t entry_offset = 8; entry_offset < block_size; entry_offset += 2)
        {
            const auto entry = static_cast<std::uint16_t>(
                bytes[directory->virtual_address + offset + entry_offset] |
                (static_cast<std::uint16_t>(bytes[directory->virtual_address + offset + entry_offset + 1]) << 8));
            const std::uint16_t type = entry >> 12;
            if (type == 0)
            {
                continue;
            }
            const std::uint32_t target_rva = page_rva + (entry & 0x0FFFU);
            if (type != 3 || target_rva > image->size || 4 > image->size - target_rva)
            {
                return false;
            }
            WriteU32(bytes + target_rva,
                     static_cast<std::uint32_t>(static_cast<std::int64_t>(ReadU32(bytes + target_rva)) + delta));
        }
        offset += block_size;
    }
    return offset == directory->size;
}

}  // namespace

void ReleaseNativePeImage(NativePeImage* image)
{
    if (image != nullptr && image->memory != nullptr)
    {
        munmap(image->memory, image->size);
    }
    if (image != nullptr)
    {
        *image = {};
    }
}

bool MapNativePe32Image(const std::vector<std::uint8_t>& file,
                        const exe::PeImageInfo& info,
                        std::uint32_t requested_base,
                        NativePeImage* image)
{
    if (image == nullptr || !exe::IsGuestExecutable(info) || requested_base == 0 ||
        info.size_of_image == 0 || info.size_of_headers > file.size() ||
        info.size_of_headers > info.size_of_image ||
        requested_base > (std::numeric_limits<std::uint32_t>::max)() - info.entry_point_rva)
    {
        return false;
    }
    *image = {};
    void* memory = mmap(reinterpret_cast<void*>(static_cast<std::uintptr_t>(requested_base)),
                        info.size_of_image, PROT_READ | PROT_WRITE | PROT_EXEC,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED || reinterpret_cast<std::uintptr_t>(memory) != requested_base)
    {
        if (memory != MAP_FAILED)
        {
            munmap(memory, info.size_of_image);
        }
        return false;
    }
    std::memcpy(memory, file.data(), info.size_of_headers);
    for (const exe::PeSection& section : info.sections)
    {
        const std::uint32_t size = section.virtual_size != 0 ? section.virtual_size : section.raw_size;
        if (size > info.size_of_image - section.virtual_address || section.raw_offset > file.size() ||
            section.raw_size > file.size() - section.raw_offset)
        {
            munmap(memory, info.size_of_image);
            return false;
        }
        const std::uint32_t copy_size = std::min(size, section.raw_size);
        if (copy_size != 0)
        {
            std::memcpy(static_cast<std::uint8_t*>(memory) + section.virtual_address,
                        file.data() + section.raw_offset, copy_size);
        }
    }
    image->memory = memory;
    image->size = info.size_of_image;
    image->entry_point = requested_base + info.entry_point_rva;
    if (!ApplyHighLowRelocations(info, image))
    {
        ReleaseNativePeImage(image);
        return false;
    }
    return true;
}

}  // namespace re2dj::platform::linux
