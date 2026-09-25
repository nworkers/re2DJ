#include "native_guest_module_image.h"

#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <string>
#include <utility>

#include "re2dj/exe/pe_image.h"
#include "re2dj/hle/modules/guest_pe_facade.h"

namespace re2dj::platform::linux
{
namespace
{

constexpr std::uint32_t kCandidateStride = 0x10000U;
constexpr std::uint32_t kMaximumCandidateCount = 256;
constexpr std::uint32_t kSectionMemExecute = 0x20000000U;
constexpr std::uint32_t kSectionMemWrite = 0x80000000U;

enum class MapResult
{
    kSuccess,
    kAddressUnavailable,
    kFailure,
};

bool AlignUp(std::uint32_t value,
             std::uint32_t alignment,
             std::uint32_t* result)
{
    if (alignment == 0 || result == nullptr)
    {
        return false;
    }
    const std::uint32_t remainder = value % alignment;
    if (remainder == 0)
    {
        *result = value;
        return true;
    }
    const std::uint32_t addition = alignment - remainder;
    if (value > (std::numeric_limits<std::uint32_t>::max)() - addition)
    {
        return false;
    }
    *result = value + addition;
    return true;
}

MapResult MapFacadeBytes(const hle::modules::GuestPeFacadeImage& facade,
                         void** output,
                         std::string* error)
{
    exe::PeImageInfo info;
    if (output == nullptr || error == nullptr || facade.file_bytes.empty() ||
        !exe::ReadPeImageInfo(facade.file_bytes.data(),
                              facade.file_bytes.size(),
                              &info,
                              error) ||
        info.magic != exe::PeMagic::kPe32 || info.machine != exe::kMachineI386 ||
        !info.is_dll || info.image_base != facade.preferred_base.value() ||
        info.entry_point_rva != 0 || info.size_of_image != facade.image_size)
    {
        if (error != nullptr && error->empty())
        {
            *error = "invalid native guest facade image";
        }
        return MapResult::kFailure;
    }

    const long page_size_value = sysconf(_SC_PAGESIZE);
    if (page_size_value <= 0 ||
        static_cast<unsigned long>(page_size_value) >
            (std::numeric_limits<std::uint32_t>::max)())
    {
        *error = "cannot determine native guest facade page size";
        return MapResult::kFailure;
    }
    const auto page_size = static_cast<std::uint32_t>(page_size_value);
    if (facade.preferred_base.value() % page_size != 0 ||
        facade.image_size % page_size != 0 ||
        info.size_of_headers > facade.file_bytes.size() ||
        info.size_of_headers > facade.image_size)
    {
        *error = "native guest facade violates page alignment or size bounds";
        return MapResult::kFailure;
    }

    for (const exe::PeSection& section : info.sections)
    {
        const std::uint32_t virtual_size =
            section.virtual_size != 0 ? section.virtual_size : section.raw_size;
        if ((section.characteristics & kSectionMemWrite) != 0 ||
            section.virtual_address % page_size != 0 ||
            section.virtual_address > info.size_of_image ||
            virtual_size > info.size_of_image - section.virtual_address ||
            section.raw_offset > facade.file_bytes.size() ||
            section.raw_size > facade.file_bytes.size() - section.raw_offset)
        {
            *error = "native guest facade section is invalid or writable";
            return MapResult::kFailure;
        }
    }

    void* memory = mmap(
        reinterpret_cast<void*>(static_cast<std::uintptr_t>(facade.preferred_base.value())),
        facade.image_size,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0);
    if (memory == MAP_FAILED)
    {
        return MapResult::kAddressUnavailable;
    }
    if (reinterpret_cast<std::uintptr_t>(memory) != facade.preferred_base.value())
    {
        munmap(memory, facade.image_size);
        return MapResult::kAddressUnavailable;
    }

    std::memcpy(memory, facade.file_bytes.data(), info.size_of_headers);
    for (const exe::PeSection& section : info.sections)
    {
        const std::uint32_t virtual_size =
            section.virtual_size != 0 ? section.virtual_size : section.raw_size;
        const std::uint32_t copy_size = (std::min)(virtual_size, section.raw_size);
        if (copy_size != 0)
        {
            std::memcpy(static_cast<std::uint8_t*>(memory) + section.virtual_address,
                        facade.file_bytes.data() + section.raw_offset,
                        copy_size);
        }
    }

    if (mprotect(memory, facade.image_size, PROT_READ) != 0)
    {
        munmap(memory, facade.image_size);
        *error = "cannot protect native guest facade as read-only";
        return MapResult::kFailure;
    }
    // Code sections stay writable. The Hardlock envelope hooks ExitProcess by
    // patching its first bytes, as Windows lets a process patch its own
    // kernel32; the guest's protection changes are only recorded
    // (GuestProcess), because every facade thunk shares one page and applying
    // a guest's temporary PAGE_READWRITE there would fault calls Windows
    // serves from other pages.
    for (const exe::PeSection& section : info.sections)
    {
        if ((section.characteristics & kSectionMemExecute) == 0)
        {
            continue;
        }
        const std::uint32_t virtual_size =
            section.virtual_size != 0 ? section.virtual_size : section.raw_size;
        std::uint32_t protection_size = 0;
        if (!AlignUp(virtual_size, page_size, &protection_size) || protection_size == 0 ||
            protection_size > info.size_of_image - section.virtual_address ||
            mprotect(static_cast<std::uint8_t*>(memory) + section.virtual_address,
                     protection_size,
                     PROT_READ | PROT_WRITE | PROT_EXEC) != 0)
        {
            munmap(memory, facade.image_size);
            *error = "cannot protect native guest facade code";
            return MapResult::kFailure;
        }
        __builtin___clear_cache(
            static_cast<char*>(memory) + section.virtual_address,
            static_cast<char*>(memory) + section.virtual_address + protection_size);
    }

    *output = memory;
    error->clear();
    return MapResult::kSuccess;
}

}  // namespace

bool MapNativeGuestModuleImage(
    const hle::modules::GuestModuleDescriptor& descriptor,
    std::span<const runtime::GuestAddress> export_gates,
    std::uintptr_t bridge_address,
    std::uintptr_t cleanup_address,
    std::uint32_t first_candidate_base,
    NativeGuestModuleImage* image,
    std::string* error)
{
    if (image == nullptr || image->memory != nullptr || error == nullptr ||
        first_candidate_base == 0 || first_candidate_base % kCandidateStride != 0 ||
        bridge_address == 0 || cleanup_address == 0 ||
        bridge_address > (std::numeric_limits<std::uint32_t>::max)() ||
        cleanup_address > (std::numeric_limits<std::uint32_t>::max)())
    {
        if (error != nullptr)
        {
            *error = "invalid native guest module image arguments";
        }
        return false;
    }

    hle::modules::GuestPeFacadeImage facade;
    void* mapped_memory = nullptr;
    bool mapped = false;
    for (std::uint32_t index = 0; index < kMaximumCandidateCount; ++index)
    {
        if (index > first_candidate_base / kCandidateStride)
        {
            break;
        }
        const std::uint32_t candidate = first_candidate_base - index * kCandidateStride;
        const hle::modules::GuestPeFacadeBuildOptions options = {
            runtime::GuestAddress(candidate),
            runtime::GuestAddress(static_cast<std::uint32_t>(bridge_address)),
            runtime::GuestAddress(static_cast<std::uint32_t>(cleanup_address)),
            export_gates,
        };
        if (!hle::modules::GuestPeFacadeBuilder::Build(
                descriptor, options, &facade, error))
        {
            return false;
        }
        const MapResult map_result = MapFacadeBytes(facade, &mapped_memory, error);
        if (map_result == MapResult::kFailure)
        {
            return false;
        }
        if (map_result == MapResult::kSuccess)
        {
            mapped = true;
            break;
        }
    }
    if (!mapped)
    {
        *error = "cannot find a collision-free native guest module address";
        return false;
    }

    NativeGuestModuleImage result;
    result.memory = mapped_memory;
    result.mapping.base = facade.preferred_base;
    result.mapping.image_size = facade.image_size;
    result.mapping.export_thunks.reserve(facade.export_thunk_rvas.size());
    for (const std::uint32_t rva : facade.export_thunk_rvas)
    {
        result.mapping.export_thunks.push_back(result.mapping.base + rva);
    }
    *image = std::move(result);
    error->clear();
    return true;
}

void ReleaseNativeGuestModuleImage(NativeGuestModuleImage* image)
{
    if (image != nullptr && image->memory != nullptr)
    {
        munmap(image->memory, image->mapping.image_size);
        *image = {};
    }
}

}  // namespace re2dj::platform::linux
