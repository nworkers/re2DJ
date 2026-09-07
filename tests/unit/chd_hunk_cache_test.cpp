#include "re2dj/storage/chd_hunk_cache.h"

#include <cstdint>
#include <vector>

#include "test_support.h"

namespace
{

std::vector<std::uint8_t> MakeHunk(std::size_t size, std::uint8_t fill)
{
    return std::vector<std::uint8_t>(size, fill);
}

}  // namespace

void RunChdHunkCacheTests(re2dj::test::Context& context)
{
    constexpr std::uint32_t kHunkBytes = 16;

    // A budget of four hunks yields exactly four entries.
    re2dj::storage::ChdHunkCache cache(kHunkBytes, kHunkBytes * 4);
    RE2DJ_CHECK_EQ(context, cache.capacity(), static_cast<std::size_t>(4));
    RE2DJ_CHECK_EQ(context, cache.size(), static_cast<std::size_t>(0));
    RE2DJ_CHECK(context, cache.Lookup(0) == nullptr);

    const std::vector<std::uint8_t> first = MakeHunk(kHunkBytes, 0x11);
    cache.Insert(0, first.data(), first.size());
    const std::uint8_t* stored = cache.Lookup(0);
    RE2DJ_CHECK(context, stored != nullptr);
    RE2DJ_CHECK_EQ(context, static_cast<int>(stored[0]), 0x11);
    RE2DJ_CHECK_EQ(context, cache.size(), static_cast<std::size_t>(1));

    // A payload whose length is not one hunk is rejected, so a short read can
    // never be served back as a full hunk.
    const std::vector<std::uint8_t> short_payload = MakeHunk(kHunkBytes - 1, 0x22);
    cache.Insert(1, short_payload.data(), short_payload.size());
    RE2DJ_CHECK(context, cache.Lookup(1) == nullptr);
    RE2DJ_CHECK_EQ(context, cache.size(), static_cast<std::size_t>(1));

    for (std::uint32_t hunk = 1; hunk < 4; ++hunk)
    {
        const std::vector<std::uint8_t> payload =
            MakeHunk(kHunkBytes, static_cast<std::uint8_t>(0x20 + hunk));
        cache.Insert(hunk, payload.data(), payload.size());
    }
    RE2DJ_CHECK_EQ(context, cache.size(), static_cast<std::size_t>(4));

    // Touching hunk 0 makes it most recently used, so inserting a fifth hunk
    // must evict hunk 1 rather than hunk 0.
    RE2DJ_CHECK(context, cache.Lookup(0) != nullptr);
    const std::vector<std::uint8_t> fifth = MakeHunk(kHunkBytes, 0x55);
    cache.Insert(4, fifth.data(), fifth.size());
    RE2DJ_CHECK_EQ(context, cache.size(), static_cast<std::size_t>(4));
    RE2DJ_CHECK(context, cache.Lookup(0) != nullptr);
    RE2DJ_CHECK(context, cache.Lookup(1) == nullptr);
    RE2DJ_CHECK(context, cache.Lookup(4) != nullptr);

    // Re-inserting a cached hunk replaces its payload without growing.
    const std::vector<std::uint8_t> replacement = MakeHunk(kHunkBytes, 0x66);
    cache.Insert(0, replacement.data(), replacement.size());
    RE2DJ_CHECK_EQ(context, cache.size(), static_cast<std::size_t>(4));
    const std::uint8_t* replaced = cache.Lookup(0);
    RE2DJ_CHECK(context, replaced != nullptr);
    RE2DJ_CHECK_EQ(context, static_cast<int>(replaced[0]), 0x66);

    cache.Clear();
    RE2DJ_CHECK_EQ(context, cache.size(), static_cast<std::size_t>(0));
    RE2DJ_CHECK(context, cache.Lookup(0) == nullptr);

    // A budget smaller than one hunk leaves no capacity, and every lookup then
    // reports a miss so the caller falls back to decompressing.
    re2dj::storage::ChdHunkCache tiny(kHunkBytes, kHunkBytes - 1);
    RE2DJ_CHECK_EQ(context, tiny.capacity(), static_cast<std::size_t>(0));
    tiny.Insert(0, first.data(), first.size());
    RE2DJ_CHECK(context, tiny.Lookup(0) == nullptr);

    // A default-constructed cache has no hunk size and behaves the same way.
    re2dj::storage::ChdHunkCache empty;
    RE2DJ_CHECK_EQ(context, empty.capacity(), static_cast<std::size_t>(0));
    RE2DJ_CHECK(context, empty.Lookup(0) == nullptr);
}
