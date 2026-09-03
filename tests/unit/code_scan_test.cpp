#include "re2dj/exe/code_scan.h"

#include <cstdint>
#include <vector>

#include "test_support.h"

namespace
{

using re2dj::exe::FindPrologueBefore;
using re2dj::exe::PrologueSearchResult;

PrologueSearchResult Find(const std::vector<std::uint8_t>& bytes,
                          std::size_t anchor,
                          std::size_t max_scan_back)
{
    return FindPrologueBefore(bytes.data(), bytes.size(), anchor, max_scan_back);
}

}  // namespace

void RunRelativeBranchScanTests(re2dj::test::Context& context);
void RunNearBranchListTests(re2dj::test::Context& context);

void RunCodeScanTests(re2dj::test::Context& context)
{
    // Two frame-pointer prologues with padding between them, the shape the
    // launcher probe walks back through to find a function start.
    const std::vector<std::uint8_t> code = {
        0x55, 0x8b, 0xec, 0x51, 0x8b, 0x45, 0x08, 0xc9,
        0xc3, 0xcc, 0xcc, 0xcc, 0x55, 0x8b, 0xec, 0x83,
        0xec, 0x08, 0x89, 0x4d, 0xfc, 0xc9, 0xc3,
    };

    // An anchor inside the second function finds that function's prologue.
    const PrologueSearchResult second = Find(code, 20, 0x100);
    RE2DJ_CHECK(context, second.found);
    RE2DJ_CHECK_EQ(context, second.offset, std::size_t{12});
    RE2DJ_CHECK_EQ(context, second.distance, std::size_t{8});

    // An anchor sitting exactly on a prologue finds the previous one rather
    // than itself, so a function start never reports a zero-length body.
    const PrologueSearchResult previous = Find(code, 12, 0x100);
    RE2DJ_CHECK(context, previous.found);
    RE2DJ_CHECK_EQ(context, previous.offset, std::size_t{0});

    // The scan-back limit is honored, so a distant prologue is not reported.
    RE2DJ_CHECK(context, !Find(code, 20, 4).found);

    // Degenerate inputs report no match instead of reading out of bounds.
    RE2DJ_CHECK(context, !Find(code, 0, 0x100).found);
    RE2DJ_CHECK(context, !Find(code, code.size() + 1, 0x100).found);
    const std::vector<std::uint8_t> without_prologue = {0x90, 0x90, 0x90, 0x90};
    RE2DJ_CHECK(context, !Find(without_prologue, 4, 0x100).found);
    RE2DJ_CHECK(context, !FindPrologueBefore(nullptr, 8, 4, 0x100).found);

    RunRelativeBranchScanTests(context);
}

void RunRelativeBranchScanTests(re2dj::test::Context& context)
{
    using re2dj::exe::RelativeBranchSite;
    using re2dj::exe::ScanRelativeBranches;

    // Mapped at 0x00401000: a `call` at offset 0 and a `jmp` at offset 8, both
    // reaching 0x00401020, plus a `call` at offset 16 reaching elsewhere.
    std::vector<std::uint8_t> code(32, 0x90);
    const auto encode = [&code](std::size_t offset,
                                std::uint8_t opcode,
                                std::uint32_t relative) {
        code[offset] = opcode;
        code[offset + 1] = static_cast<std::uint8_t>(relative & 0xff);
        code[offset + 2] = static_cast<std::uint8_t>((relative >> 8) & 0xff);
        code[offset + 3] = static_cast<std::uint8_t>((relative >> 16) & 0xff);
        code[offset + 4] = static_cast<std::uint8_t>((relative >> 24) & 0xff);
    };
    // 0x00401000 + 0 + 5 + 0x1b = 0x00401020
    encode(0, 0xe8, 0x1b);
    // 0x00401000 + 8 + 5 + 0x13 = 0x00401020
    encode(8, 0xe9, 0x13);
    encode(16, 0xe8, 0x100);

    bool capped = true;
    std::size_t total = 0;
    const std::vector<RelativeBranchSite> sites = ScanRelativeBranches(
        code.data(), code.size(), 0x00401000u, 0x00401020u, 8, &capped, &total);
    RE2DJ_CHECK_EQ(context, sites.size(), std::size_t{2});
    RE2DJ_CHECK_EQ(context, total, std::size_t{2});
    RE2DJ_CHECK(context, !capped);
    RE2DJ_CHECK_EQ(context, sites[0].offset, std::uint32_t{0});
    RE2DJ_CHECK_EQ(context, sites[0].opcode, std::uint8_t{0xe8});
    RE2DJ_CHECK_EQ(context, sites[1].offset, std::uint32_t{8});
    RE2DJ_CHECK_EQ(context, sites[1].opcode, std::uint8_t{0xe9});

    // Counting continues past the collection cap.
    capped = false;
    total = 0;
    const std::vector<RelativeBranchSite> limited = ScanRelativeBranches(
        code.data(), code.size(), 0x00401000u, 0x00401020u, 1, &capped, &total);
    RE2DJ_CHECK_EQ(context, limited.size(), std::size_t{1});
    RE2DJ_CHECK_EQ(context, total, std::size_t{2});
    RE2DJ_CHECK(context, capped);

    // A backward branch resolves through the 32-bit wrap rather than
    // underflowing.
    std::vector<std::uint8_t> backward(8, 0x90);
    backward[0] = 0xe8;
    backward[1] = 0xfb;
    backward[2] = 0xff;
    backward[3] = 0xff;
    backward[4] = 0xff;
    RE2DJ_CHECK_EQ(
        context,
        ScanRelativeBranches(
            backward.data(), backward.size(), 0x00401000u, 0x00401000u, 8, nullptr, nullptr)
            .size(),
        std::size_t{1});

    // No match, a buffer too short to hold a branch, and null input.
    RE2DJ_CHECK_EQ(context,
                   ScanRelativeBranches(code.data(), code.size(), 0x00401000u,
                                        0x00500000u, 8, nullptr, nullptr)
                       .size(),
                   std::size_t{0});
    RE2DJ_CHECK_EQ(context,
                   ScanRelativeBranches(code.data(), 4, 0x00401000u, 0x00401020u, 8,
                                        nullptr, nullptr)
                       .size(),
                   std::size_t{0});
    RE2DJ_CHECK_EQ(
        context,
        ScanRelativeBranches(nullptr, 32, 0x00401000u, 0x00401020u, 8, nullptr, nullptr)
            .size(),
        std::size_t{0});

    RunNearBranchListTests(context);
}

void RunNearBranchListTests(re2dj::test::Context& context)
{
    using re2dj::exe::ListNearBranches;
    using re2dj::exe::NearBranch;

    // Mapped at 0x00401000: `je +4`, `call rel32`, `jmp rel8` backward, and a
    // two-byte `jne rel32`.
    const std::vector<std::uint8_t> code = {
        0x74, 0x04,                                // 0: je 0x00401006
        0xe8, 0x10, 0x00, 0x00, 0x00,              // 2: call 0x00401017
        0xeb, 0xf7,                                // 7: jmp 0x00401000
        0x0f, 0x85, 0x20, 0x00, 0x00, 0x00,        // 9: jne 0x0040102f
        0x90,
    };
    bool capped = true;
    std::size_t total = 0;
    const std::vector<NearBranch> branches = ListNearBranches(
        code.data(), code.size(), 0x00401000u, 0, code.size(), 16, &capped, &total);
    RE2DJ_CHECK_EQ(context, branches.size(), std::size_t{4});
    RE2DJ_CHECK_EQ(context, total, std::size_t{4});
    RE2DJ_CHECK(context, !capped);
    RE2DJ_CHECK_EQ(context, branches[0].opcode, std::uint8_t{0x74});
    RE2DJ_CHECK_EQ(context, branches[0].target, std::uint32_t{0x00401006});
    RE2DJ_CHECK(context, !branches[0].near_form);
    RE2DJ_CHECK_EQ(context, branches[1].opcode, std::uint8_t{0xe8});
    RE2DJ_CHECK_EQ(context, branches[1].target, std::uint32_t{0x00401017});
    RE2DJ_CHECK(context, branches[1].near_form);
    // A negative short displacement resolves backward instead of wrapping high.
    RE2DJ_CHECK_EQ(context, branches[2].opcode, std::uint8_t{0xeb});
    RE2DJ_CHECK_EQ(context, branches[2].target, std::uint32_t{0x00401000});
    RE2DJ_CHECK_EQ(context, branches[3].opcode, std::uint8_t{0x85});
    RE2DJ_CHECK_EQ(context, branches[3].target, std::uint32_t{0x0040102f});
    RE2DJ_CHECK(context, branches[3].near_form);

    // The listed range is honored, so branches before `start` are skipped.
    RE2DJ_CHECK_EQ(context,
                   ListNearBranches(code.data(), code.size(), 0x00401000u, 7,
                                    code.size(), 16, nullptr, nullptr)
                       .size(),
                   std::size_t{2});

    // Counting continues past the collection cap.
    capped = false;
    total = 0;
    RE2DJ_CHECK_EQ(context,
                   ListNearBranches(code.data(), code.size(), 0x00401000u, 0,
                                    code.size(), 1, &capped, &total)
                       .size(),
                   std::size_t{1});
    RE2DJ_CHECK_EQ(context, total, std::size_t{4});
    RE2DJ_CHECK(context, capped);

    // A start past the buffer and null input list nothing.
    RE2DJ_CHECK_EQ(context,
                   ListNearBranches(code.data(), code.size(), 0x00401000u,
                                    code.size(), 16, 16, nullptr, nullptr)
                       .size(),
                   std::size_t{0});
    RE2DJ_CHECK_EQ(
        context,
        ListNearBranches(nullptr, 16, 0x00401000u, 0, 16, 16, nullptr, nullptr).size(),
        std::size_t{0});
}
