#include "re2dj/exe/code_scan.h"

#include <cstdint>
#include <vector>

#include "test_support.h"

namespace
{

using re2dj::exe::PortHelperKind;
using re2dj::exe::PortHelperSite;
using re2dj::exe::ScanPortHelpers;

// The helper block as it appears in the 1st SE canonical build, where the two
// byte-width entries land on the RVAs the target profile stores.
const std::vector<std::uint8_t> kHelperBlock = {
    0x33, 0xc0, 0x66, 0x8b, 0x54, 0x24, 0x04, 0xec, 0xc3,              // inportb
    0x66, 0x8b, 0x54, 0x24, 0x04, 0x66, 0xed, 0xc3,                    // inportw
    0x66, 0x8b, 0x54, 0x24, 0x04, 0xed, 0xc3,                          // inportl
    0x66, 0x8b, 0x54, 0x24, 0x04, 0x8a, 0x44, 0x24, 0x08, 0xee, 0xc3,  // outportb
    0x66, 0x8b, 0x54, 0x24, 0x04, 0x66, 0x8b, 0x44, 0x24, 0x08,
    0x66, 0xef, 0xc3,  // outportw
};

std::vector<PortHelperSite> Scan(const std::vector<std::uint8_t>& bytes,
                                 std::uint32_t base_address,
                                 std::size_t max_sites,
                                 bool* capped,
                                 std::size_t* total)
{
    return ScanPortHelpers(bytes.data(), bytes.size(), base_address, max_sites, capped, total);
}

}  // namespace

void RunPortHelperScanTests(re2dj::test::Context& context)
{
    {
        // Every width is found once, in address order, and each opcode address
        // points at the `in`/`out` instruction rather than the signature start.
        bool capped = true;
        std::size_t total = 0;
        const std::vector<PortHelperSite> sites = Scan(kHelperBlock, 0, 16, &capped, &total);
        RE2DJ_CHECK(context, !capped);
        RE2DJ_CHECK(context, total == 5);
        RE2DJ_CHECK(context, sites.size() == 5);
        if (sites.size() == 5)
        {
            RE2DJ_CHECK(context, sites[0].kind == PortHelperKind::kInPortByte);
            RE2DJ_CHECK(context, sites[0].opcode_address == 7);
            RE2DJ_CHECK(context, kHelperBlock[sites[0].opcode_address] == 0xec);

            RE2DJ_CHECK(context, sites[1].kind == PortHelperKind::kInPortWord);
            RE2DJ_CHECK(context, kHelperBlock[sites[1].opcode_address] == 0x66);
            RE2DJ_CHECK(context, kHelperBlock[sites[1].opcode_address + 1] == 0xed);

            RE2DJ_CHECK(context, sites[2].kind == PortHelperKind::kInPortDword);
            RE2DJ_CHECK(context, kHelperBlock[sites[2].opcode_address] == 0xed);

            RE2DJ_CHECK(context, sites[3].kind == PortHelperKind::kOutPortByte);
            RE2DJ_CHECK(context, kHelperBlock[sites[3].opcode_address] == 0xee);

            RE2DJ_CHECK(context, sites[4].kind == PortHelperKind::kOutPortWord);
            RE2DJ_CHECK(context, kHelperBlock[sites[4].opcode_address] == 0x66);
            RE2DJ_CHECK(context, kHelperBlock[sites[4].opcode_address + 1] == 0xef);
        }
    }

    {
        // The base address is added, so an image base yields the VA a profile
        // and a privileged fault both speak in.
        std::size_t total = 0;
        const std::vector<PortHelperSite> sites =
            Scan(kHelperBlock, 0x00400000, 16, nullptr, &total);
        RE2DJ_CHECK(context, total == 5);
        RE2DJ_CHECK(context, sites.size() == 5);
        if (!sites.empty())
        {
            RE2DJ_CHECK(context, sites[0].opcode_address == 0x00400007);
        }
    }

    {
        // Padding in front shifts every result by the same amount, which is
        // what makes a file offset usable as an RVA.
        std::vector<std::uint8_t> padded(0x100, 0x90);
        padded.insert(padded.end(), kHelperBlock.begin(), kHelperBlock.end());
        std::size_t total = 0;
        const std::vector<PortHelperSite> sites = Scan(padded, 0, 16, nullptr, &total);
        RE2DJ_CHECK(context, total == 5);
        if (!sites.empty())
        {
            RE2DJ_CHECK(context, sites[0].signature_offset == 0x100);
            RE2DJ_CHECK(context, sites[0].opcode_address == 0x107);
        }
    }

    {
        // Counting continues past the collection cap, so a capped listing still
        // reports how many exist.
        bool capped = false;
        std::size_t total = 0;
        const std::vector<PortHelperSite> sites = Scan(kHelperBlock, 0, 2, &capped, &total);
        RE2DJ_CHECK(context, capped);
        RE2DJ_CHECK(context, sites.size() == 2);
        RE2DJ_CHECK(context, total == 5);
    }

    {
        // Ciphertext, an empty buffer and a null pointer are ordinary results,
        // not errors: a protected build read off disk produces the first.
        const std::vector<std::uint8_t> noise(0x200, 0x5a);
        std::size_t total = 1;
        const std::vector<PortHelperSite> sites = Scan(noise, 0, 16, nullptr, &total);
        RE2DJ_CHECK(context, sites.empty());
        RE2DJ_CHECK(context, total == 0);

        std::size_t empty_total = 1;
        const std::vector<PortHelperSite> none =
            ScanPortHelpers(nullptr, 0, 0, 16, nullptr, &empty_total);
        RE2DJ_CHECK(context, none.empty());
        RE2DJ_CHECK(context, empty_total == 0);
    }

    {
        // A signature cut short by the end of the buffer must not be reported,
        // and must not read past it.
        std::vector<std::uint8_t> truncated(kHelperBlock.begin(), kHelperBlock.begin() + 5);
        std::size_t total = 1;
        const std::vector<PortHelperSite> sites = Scan(truncated, 0, 16, nullptr, &total);
        RE2DJ_CHECK(context, sites.empty());
        RE2DJ_CHECK(context, total == 0);
    }
}
