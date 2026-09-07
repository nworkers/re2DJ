#ifndef RE2DJ_STORAGE_CHD_HUNK_CACHE_H_
#define RE2DJ_STORAGE_CHD_HUNK_CACHE_H_

#include <cstddef>
#include <cstdint>
#include <list>
#include <unordered_map>
#include <vector>

namespace re2dj::storage
{

// Least-recently-used cache of decompressed CHD hunks.
//
// libchdr decompresses on every chd_read call, so a FAT32 layer that reads one
// 512-byte sector at a time pays a full hunk decompression per sector, and a
// chain walk pays it once per hop. The image is opened read-only and is never
// modified, so a cached hunk stays valid for the life of the image and the
// cache needs no invalidation path.
class ChdHunkCache
{
public:
    // Default budget for the cached payload. The entry count is derived from
    // the hunk size, so images with larger hunks simply hold fewer of them.
    static constexpr std::size_t kDefaultBudgetBytes = 32u * 1024u * 1024u;

    ChdHunkCache() = default;

    // A zero hunk size or a budget smaller than one hunk yields a cache with no
    // capacity, which reports every lookup as a miss and drops every insert.
    ChdHunkCache(std::uint32_t hunk_bytes, std::size_t budget_bytes = kDefaultBudgetBytes);

    std::uint32_t hunk_bytes() const
    {
        return hunk_bytes_;
    }

    std::size_t capacity() const
    {
        return capacity_;
    }

    std::size_t size() const
    {
        return index_.size();
    }

    // Returns the cached payload for `hunk` and marks it most recently used, or
    // nullptr when the hunk is not cached. The pointer stays valid until the
    // next Insert call.
    const std::uint8_t* Lookup(std::uint32_t hunk);

    // Stores one hunk payload, evicting the least recently used entry when the
    // cache is full. `length` must equal the configured hunk size; anything
    // else is rejected so a short read can never be served as a full hunk.
    void Insert(std::uint32_t hunk, const std::uint8_t* payload, std::size_t length);

    void Clear();

private:
    struct Entry
    {
        std::uint32_t hunk = 0;
        std::vector<std::uint8_t> payload;
    };

    // Front is the most recently used entry.
    using EntryList = std::list<Entry>;

    std::uint32_t hunk_bytes_ = 0;
    std::size_t capacity_ = 0;
    EntryList entries_;
    std::unordered_map<std::uint32_t, EntryList::iterator> index_;
};

}  // namespace re2dj::storage

#endif  // RE2DJ_STORAGE_CHD_HUNK_CACHE_H_
