#include "re2dj/storage/chd_hunk_cache.h"

#include <algorithm>
#include <utility>

namespace re2dj::storage
{

ChdHunkCache::ChdHunkCache(std::uint32_t hunk_bytes, std::size_t budget_bytes)
    : hunk_bytes_(hunk_bytes)
{
    if (hunk_bytes_ == 0)
    {
        return;
    }
    capacity_ = budget_bytes / hunk_bytes_;
}

const std::uint8_t* ChdHunkCache::Lookup(std::uint32_t hunk)
{
    const auto found = index_.find(hunk);
    if (found == index_.end())
    {
        return nullptr;
    }
    entries_.splice(entries_.begin(), entries_, found->second);
    return entries_.front().payload.data();
}

void ChdHunkCache::Insert(std::uint32_t hunk, const std::uint8_t* payload, std::size_t length)
{
    if (capacity_ == 0 || payload == nullptr || length != hunk_bytes_)
    {
        return;
    }
    const auto found = index_.find(hunk);
    if (found != index_.end())
    {
        entries_.splice(entries_.begin(), entries_, found->second);
        std::copy_n(payload, length, entries_.front().payload.begin());
        return;
    }
    if (index_.size() >= capacity_)
    {
        // Reuse the evicted entry's buffer so a steady-state miss does not
        // allocate: the payload is always exactly one hunk.
        EntryList::iterator victim = std::prev(entries_.end());
        index_.erase(victim->hunk);
        victim->hunk = hunk;
        std::copy_n(payload, length, victim->payload.begin());
        entries_.splice(entries_.begin(), entries_, victim);
        index_.emplace(hunk, entries_.begin());
        return;
    }
    Entry entry;
    entry.hunk = hunk;
    entry.payload.assign(payload, payload + length);
    entries_.push_front(std::move(entry));
    index_.emplace(hunk, entries_.begin());
}

void ChdHunkCache::Clear()
{
    index_.clear();
    entries_.clear();
}

}  // namespace re2dj::storage
