#include "re2dj/exe/immediate_scan.h"

#include <cstdint>
#include <vector>

#include "test_support.h"

namespace
{

using re2dj::exe::ImmediateReference;
using re2dj::exe::ScanImmediateReferences;

std::vector<ImmediateReference> Scan(const std::vector<std::uint8_t>& bytes,
                                    const std::vector<std::uint32_t>& values,
                                    std::size_t max_matches,
                                    bool* capped,
                                    std::size_t* total = nullptr)
{
    return ScanImmediateReferences(bytes.data(),
                                   bytes.size(),
                                   values.data(),
                                   values.size(),
                                   max_matches,
                                   capped,
                                   total);
}

}  // namespace

void RunImmediateScanTests(re2dj::test::Context& context)
{
    // `mov ecx, 0x00acd708` followed by `push 0x004dd054`, the two forms the
    // launcher probe looks for when locating references to a static object.
    const std::vector<std::uint8_t> code = {
        0x90, 0xb9, 0x08, 0xd7, 0xac, 0x00, 0x68, 0x54,
        0xd0, 0x4d, 0x00, 0xc3,
    };
    bool capped = true;
    const std::vector<ImmediateReference> matches =
        Scan(code, {0x00acd708u, 0x004dd054u}, 16, &capped);
    RE2DJ_CHECK_EQ(context, matches.size(), std::size_t{2});
    RE2DJ_CHECK(context, !capped);
    RE2DJ_CHECK_EQ(context, matches[0].offset, std::uint32_t{2});
    RE2DJ_CHECK_EQ(context, matches[0].value, std::uint32_t{0x00acd708});
    RE2DJ_CHECK_EQ(context, matches[0].leading_count, std::uint8_t{2});
    RE2DJ_CHECK_EQ(context, matches[0].leading[matches[0].leading_count - 1],
                   std::uint8_t{0xb9});
    // The immediate is followed by `push 0x004dd054`, so the trailing window
    // carries the next opcode.
    RE2DJ_CHECK_EQ(context, matches[0].trailing_count, std::uint8_t{6});
    RE2DJ_CHECK_EQ(context, matches[0].trailing[0], std::uint8_t{0x68});
    RE2DJ_CHECK_EQ(context, matches[1].offset, std::uint32_t{7});
    RE2DJ_CHECK_EQ(context, matches[1].value, std::uint32_t{0x004dd054});
    RE2DJ_CHECK_EQ(context, matches[1].leading[matches[1].leading_count - 1],
                   std::uint8_t{0x68});
    // Only `c3` follows the last immediate, so the trailing window is short.
    RE2DJ_CHECK_EQ(context, matches[1].trailing_count, std::uint8_t{1});
    RE2DJ_CHECK_EQ(context, matches[1].trailing[0], std::uint8_t{0xc3});

    // A value at the very start reports no leading context rather than reading
    // before the buffer.
    const std::vector<std::uint8_t> leading_edge = {0x08, 0xd7, 0xac, 0x00, 0x90};
    const std::vector<ImmediateReference> edge =
        Scan(leading_edge, {0x00acd708u}, 16, nullptr);
    RE2DJ_CHECK_EQ(context, edge.size(), std::size_t{1});
    RE2DJ_CHECK_EQ(context, edge[0].offset, std::uint32_t{0});
    RE2DJ_CHECK_EQ(context, edge[0].leading_count, std::uint8_t{0});
    RE2DJ_CHECK_EQ(context, edge[0].trailing_count, std::uint8_t{1});
    RE2DJ_CHECK_EQ(context, edge[0].trailing[0], std::uint8_t{0x90});

    // The cap truncates and is reported, so a scan of a large region cannot
    // flood the diagnostic log.
    const std::vector<std::uint8_t> repeated = {
        0x08, 0xd7, 0xac, 0x00, 0x08, 0xd7, 0xac, 0x00,
        0x08, 0xd7, 0xac, 0x00,
    };
    capped = false;
    std::size_t total = 0;
    const std::vector<ImmediateReference> limited =
        Scan(repeated, {0x00acd708u}, 2, &capped, &total);
    RE2DJ_CHECK_EQ(context, limited.size(), std::size_t{2});
    RE2DJ_CHECK(context, capped);
    // Counting continues past the cap so a truncated scan still reports how
    // many references exist.
    RE2DJ_CHECK_EQ(context, total, std::size_t{3});

    // Absent values, short buffers, and empty value lists all scan to nothing.
    RE2DJ_CHECK_EQ(context, Scan(code, {0x11223344u}, 16, nullptr).size(),
                   std::size_t{0});
    const std::vector<std::uint8_t> too_short = {0x08, 0xd7, 0xac};
    RE2DJ_CHECK_EQ(context, Scan(too_short, {0x00acd708u}, 16, nullptr).size(),
                   std::size_t{0});
    RE2DJ_CHECK_EQ(context, Scan(code, {}, 16, nullptr).size(), std::size_t{0});
    RE2DJ_CHECK_EQ(context,
                   ScanImmediateReferences(nullptr, 8, nullptr, 0, 16, nullptr, nullptr)
                       .size(),
                   std::size_t{0});
}
