#include "re2dj/hle/guest_heap.h"

#include <cstdint>
#include <iterator>

namespace re2dj::hle
{
namespace
{

std::uint64_t Round(std::uint32_t size)
{
    return (static_cast<std::uint64_t>(size == 0 ? 1 : size) + GuestHeap::kAlignment - 1) &
           ~static_cast<std::uint64_t>(GuestHeap::kAlignment - 1);
}

}  // namespace

std::uint32_t GuestHeap::Allocate(std::uint32_t size)
{
    const std::uint64_t wanted = Round(size);
    const std::uint64_t region_end = static_cast<std::uint64_t>(base_) + size_;
    std::uint64_t candidate = (static_cast<std::uint64_t>(base_) + kAlignment - 1) &
                              ~static_cast<std::uint64_t>(kAlignment - 1);
    // First fit: walk the gaps between live blocks in address order.
    for (const auto& [start, block] : blocks_)
    {
        if (candidate + wanted <= start)
        {
            break;
        }
        candidate = static_cast<std::uint64_t>(start) + block.rounded;
    }
    if (size_ == 0 || candidate + wanted > region_end)
    {
        return 0;
    }
    const auto address = static_cast<std::uint32_t>(candidate);
    blocks_.emplace(address, Block{static_cast<std::uint32_t>(wanted), size});
    return address;
}

bool GuestHeap::Free(std::uint32_t address)
{
    return blocks_.erase(address) == 1;
}

std::optional<std::uint32_t> GuestHeap::BlockSize(std::uint32_t address) const
{
    const auto block = blocks_.find(address);
    if (block == blocks_.end())
    {
        return std::nullopt;
    }
    return block->second.requested;
}

bool GuestHeap::ResizeInPlace(std::uint32_t address, std::uint32_t size)
{
    const auto block = blocks_.find(address);
    if (block == blocks_.end())
    {
        return false;
    }
    const std::uint64_t wanted = Round(size);
    const auto next = std::next(block);
    const std::uint64_t limit = next == blocks_.end()
                                    ? static_cast<std::uint64_t>(base_) + size_
                                    : static_cast<std::uint64_t>(next->first);
    if (static_cast<std::uint64_t>(address) + wanted > limit)
    {
        return false;
    }
    block->second = {static_cast<std::uint32_t>(wanted), size};
    return true;
}

bool GuestHeap::BlockContains(std::uint32_t address, std::uint32_t size) const
{
    auto block = blocks_.upper_bound(address);
    if (block == blocks_.begin())
    {
        return false;
    }
    --block;
    const std::uint64_t end = static_cast<std::uint64_t>(block->first) + block->second.rounded;
    return static_cast<std::uint64_t>(address) + size <= end;
}

}  // namespace re2dj::hle
