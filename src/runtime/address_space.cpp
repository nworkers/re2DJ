#include "re2dj/runtime/address_space.h"

#include <algorithm>
#include <limits>
#include <new>

namespace re2dj::runtime
{

namespace
{

bool HasAccess(MemoryAccess available, MemoryAccess requested)
{
    const auto available_bits = static_cast<std::uint8_t>(available);
    const auto requested_bits = static_cast<std::uint8_t>(requested);
    return (available_bits & requested_bits) == requested_bits;
}

bool RangeEnds(std::uint32_t base, std::uint32_t size, std::uint64_t* end)
{
    const std::uint64_t result = static_cast<std::uint64_t>(base) + size;
    if (result > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) + 1)
    {
        return false;
    }
    *end = result;
    return true;
}

}  // namespace

bool AddressSpace::Map(GuestAddress base,
                       std::uint32_t size,
                       MemoryAccess access,
                       std::string* error)
{
    if (size == 0)
    {
        if (error != nullptr)
        {
            *error = "cannot map an empty guest range";
        }
        return false;
    }

    std::uint64_t end = 0;
    if (!RangeEnds(base.value(), size, &end))
    {
        if (error != nullptr)
        {
            *error = "guest mapping exceeds the 32-bit address space";
        }
        return false;
    }

    const std::uint32_t mapped_base = base.value() & ~(kGuestPageSize - 1);
    const std::uint64_t mapped_end =
        (end + kGuestPageSize - 1) & ~static_cast<std::uint64_t>(kGuestPageSize - 1);
    const std::uint64_t mapped_size = mapped_end - mapped_base;
    if (mapped_end > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) + 1 ||
        mapped_size > std::numeric_limits<std::uint32_t>::max())
    {
        if (error != nullptr)
        {
            *error = "page-aligned guest mapping is too large";
        }
        return false;
    }

    for (const Region& region : regions_)
    {
        std::uint64_t region_end = 0;
        RangeEnds(region.base, static_cast<std::uint32_t>(region.bytes.size()), &region_end);
        if (static_cast<std::uint64_t>(mapped_base) < region_end &&
            static_cast<std::uint64_t>(region.base) < mapped_end)
        {
            if (error != nullptr)
            {
                *error = "guest mapping overlaps an existing region";
            }
            return false;
        }
    }

    Region region;
    region.base = mapped_base;
    try
    {
        region.bytes.resize(static_cast<std::size_t>(mapped_size), 0);
    }
    catch (const std::bad_alloc&)
    {
        if (error != nullptr)
        {
            *error = "host memory allocation for guest mapping failed";
        }
        return false;
    }
    region.access = access;
    regions_.push_back(std::move(region));
    return true;
}

const AddressSpace::Region* AddressSpace::Find(GuestAddress address,
                                               std::uint32_t size,
                                               MemoryAccess access) const
{
    std::uint64_t requested_end = 0;
    if (!RangeEnds(address.value(), size, &requested_end))
    {
        return nullptr;
    }
    for (const Region& region : regions_)
    {
        const std::uint64_t region_end =
            static_cast<std::uint64_t>(region.base) + region.bytes.size();
        if (address.value() >= region.base && requested_end <= region_end &&
            HasAccess(region.access, access))
        {
            return &region;
        }
    }
    return nullptr;
}

AddressSpace::Region* AddressSpace::Find(GuestAddress address,
                                         std::uint32_t size,
                                         MemoryAccess access)
{
    return const_cast<Region*>(static_cast<const AddressSpace*>(this)->Find(address, size, access));
}

bool AddressSpace::IsMapped(GuestAddress address, std::uint32_t size, MemoryAccess access) const
{
    return Find(address, size, access) != nullptr;
}

bool AddressSpace::Read8(GuestAddress address, std::uint8_t* value) const
{
    const Region* region = Find(address, 1, MemoryAccess::kRead);
    if (region == nullptr || value == nullptr)
    {
        return false;
    }
    *value = region->bytes[address.value() - region->base];
    return true;
}

bool AddressSpace::Read16(GuestAddress address, std::uint16_t* value) const
{
    const Region* region = Find(address, 2, MemoryAccess::kRead);
    if (region == nullptr || value == nullptr)
    {
        return false;
    }
    const std::size_t offset = address.value() - region->base;
    const std::uint8_t low = region->bytes[offset];
    const std::uint8_t high = region->bytes[offset + 1];
    *value = static_cast<std::uint16_t>(low) | (static_cast<std::uint16_t>(high) << 8);
    return true;
}

bool AddressSpace::Read32(GuestAddress address, std::uint32_t* value) const
{
    const Region* region = Find(address, 4, MemoryAccess::kRead);
    if (region == nullptr || value == nullptr)
    {
        return false;
    }
    const std::size_t offset = address.value() - region->base;
    *value = static_cast<std::uint32_t>(region->bytes[offset]) |
             (static_cast<std::uint32_t>(region->bytes[offset + 1]) << 8) |
             (static_cast<std::uint32_t>(region->bytes[offset + 2]) << 16) |
             (static_cast<std::uint32_t>(region->bytes[offset + 3]) << 24);
    return true;
}

bool AddressSpace::Write8(GuestAddress address, std::uint8_t value)
{
    Region* region = Find(address, 1, MemoryAccess::kWrite);
    if (region == nullptr)
    {
        return false;
    }
    region->bytes[address.value() - region->base] = value;
    return true;
}

bool AddressSpace::Write16(GuestAddress address, std::uint16_t value)
{
    Region* region = Find(address, 2, MemoryAccess::kWrite);
    if (region == nullptr)
    {
        return false;
    }
    const std::size_t offset = address.value() - region->base;
    region->bytes[offset] = static_cast<std::uint8_t>(value);
    region->bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
    return true;
}

bool AddressSpace::Write32(GuestAddress address, std::uint32_t value)
{
    Region* region = Find(address, 4, MemoryAccess::kWrite);
    if (region == nullptr)
    {
        return false;
    }
    const std::size_t offset = address.value() - region->base;
    for (std::size_t index = 0; index < 4; ++index)
    {
        region->bytes[offset + index] =
            static_cast<std::uint8_t>(value >> (index * 8));
    }
    return true;
}

bool AddressSpace::WriteBytes(GuestAddress address, const std::uint8_t* bytes, std::size_t size)
{
    if (bytes == nullptr || size > std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }
    Region* region = Find(address, static_cast<std::uint32_t>(size), MemoryAccess::kWrite);
    if (region == nullptr)
    {
        return false;
    }
    std::copy(bytes, bytes + size, region->bytes.begin() + (address.value() - region->base));
    return true;
}

}  // namespace re2dj::runtime
