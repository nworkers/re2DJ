#ifndef RE2DJ_HLE_GUEST_HEAP_H_
#define RE2DJ_HLE_GUEST_HEAP_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>

namespace re2dj::hle
{

// Block bookkeeping for one guest heap over a guest-addressable region: first
// fit, eight-byte alignment as Win32 heaps give on x86, and each block's
// requested size, which HeapSize reports.
class GuestHeap
{
public:
    static constexpr std::uint32_t kAlignment = 8;

    GuestHeap() = default;
    GuestHeap(std::uint32_t base, std::uint32_t size) : base_(base), size_(size) {}

    std::uint32_t base() const { return base_; }
    std::uint32_t size() const { return size_; }

    // A block of at least size bytes (one byte for zero), or 0 when the region
    // has no room.
    std::uint32_t Allocate(std::uint32_t size);
    // False for an address that is not the start of a live block.
    bool Free(std::uint32_t address);
    // The size requested for the live block starting at address.
    std::optional<std::uint32_t> BlockSize(std::uint32_t address) const;
    // Changes a live block's size without moving it; false when the space
    // after it is taken or the address is no block.
    bool ResizeInPlace(std::uint32_t address, std::uint32_t size);
    // True when [address, address + size) lies within one live block.
    bool BlockContains(std::uint32_t address, std::uint32_t size) const;
    std::size_t live_blocks() const { return blocks_.size(); }

private:
    struct Block
    {
        std::uint32_t rounded = 0;
        std::uint32_t requested = 0;
    };

    std::uint32_t base_ = 0;
    std::uint32_t size_ = 0;
    std::map<std::uint32_t, Block> blocks_;
};

}  // namespace re2dj::hle

#endif  // RE2DJ_HLE_GUEST_HEAP_H_
