#include "re2dj/hle/hardlock/transform_responses.h"

#include <string>
#include <vector>

#include "test_support.h"

namespace
{

// A run of unspecified bytes in request-row notation.
std::string Unspecified(std::size_t bytes)
{
    return std::string(bytes * 2, '?');
}

}  // namespace

void RunHardlockTransformResponsesTests(re2dj::test::Context& context)
{
    using re2dj::hle::hardlock::FindHardlockTransformResponse;
    using re2dj::hle::hardlock::HardlockTransformBlock;
    using re2dj::hle::hardlock::HardlockTransformResponseMap;
    using re2dj::hle::hardlock::kHardlockPayloadRecordSize;
    using re2dj::hle::hardlock::PackHardlockTransformResponseMap;
    using re2dj::hle::hardlock::ParseHardlockTransformResponseMap;

    HardlockTransformResponseMap map;
    std::string error;

    // Comments, blank lines, trailing comments, and mixed-case hex all parse.
    const std::string text =
        "# challenge response\n"
        "\n"
        "0011223344556677 8899aabbccddeeff\n"
        "  0102030405060708\t1112131415161718  # second entry\n"
        "AABBCCDDEEFF0011 2233445566778899\n";
    RE2DJ_CHECK(context, ParseHardlockTransformResponseMap(text, &map, &error));
    RE2DJ_CHECK_EQ(context, map.blocks.size(), std::size_t{3});
    RE2DJ_CHECK(context, map.payloads.empty());
    RE2DJ_CHECK(context, error.empty());
    RE2DJ_CHECK_EQ(context, map.blocks[0].input[0], std::uint8_t{0x00});
    RE2DJ_CHECK_EQ(context, map.blocks[0].input[7], std::uint8_t{0x77});
    RE2DJ_CHECK_EQ(context, map.blocks[0].output[0], std::uint8_t{0x88});
    RE2DJ_CHECK_EQ(context, map.blocks[0].output[7], std::uint8_t{0xff});
    RE2DJ_CHECK_EQ(context, map.blocks[2].input[0], std::uint8_t{0xaa});

    // Lookup finds an entry and reports a miss without inventing one.
    const HardlockTransformBlock present = {0x01, 0x02, 0x03, 0x04,
                                            0x05, 0x06, 0x07, 0x08};
    const HardlockTransformBlock absent = {0xde, 0xad, 0xbe, 0xef,
                                           0xde, 0xad, 0xbe, 0xef};
    const HardlockTransformBlock* found = FindHardlockTransformResponse(map.blocks, present);
    RE2DJ_CHECK(context, found != nullptr);
    if (found != nullptr)
    {
        RE2DJ_CHECK_EQ(context, (*found)[0], std::uint8_t{0x11});
        RE2DJ_CHECK_EQ(context, (*found)[7], std::uint8_t{0x18});
    }
    RE2DJ_CHECK(context, FindHardlockTransformResponse(map.blocks, absent) == nullptr);

    // A repeated challenge is rejected: two outputs for one input would make
    // the result depend on call order.
    HardlockTransformResponseMap rejected;
    RE2DJ_CHECK(context,
                !ParseHardlockTransformResponseMap(
                    "0011223344556677 8899aabbccddeeff\n"
                    "0011223344556677 0000000000000000\n",
                    &rejected,
                    &error));
    RE2DJ_CHECK(context, error.find("duplicate") != std::string::npos);

    // Wrong digit counts, non-hex digits, a missing output, extra tokens, and
    // a wildcard in a block row all fail rather than parsing partially.
    const char* const invalid[] = {
        "0011223344556677 8899aabbccddee\n",
        "00112233445566 8899aabbccddeeff\n",
        "001122334455667g 8899aabbccddeeff\n",
        "0011223344556677\n",
        "0011223344556677 8899aabbccddeeff 0011223344556677\n",
        "00112233445566?? 8899aabbccddeeff\n",
    };
    for (const char* const line : invalid)
    {
        HardlockTransformResponseMap unused;
        RE2DJ_CHECK(context, !ParseHardlockTransformResponseMap(line, &unused, &error));
        RE2DJ_CHECK(context, !error.empty());
    }

    // A map with no entries is an error rather than a silently empty map,
    // because it would look like an identity run.
    HardlockTransformResponseMap empty;
    RE2DJ_CHECK(context,
                !ParseHardlockTransformResponseMap("# nothing here\n", &empty, &error));

    // A file without a trailing newline still yields its last entry.
    HardlockTransformResponseMap tail;
    RE2DJ_CHECK(context,
                ParseHardlockTransformResponseMap(
                    "0011223344556677 8899aabbccddeeff", &tail, &error));
    RE2DJ_CHECK_EQ(context, tail.blocks.size(), std::size_t{1});

    // A longer row is a request row and may leave bytes unspecified. Block
    // rows and request rows coexist in one map.
    const std::string mixed = "0011223344556677 8899aabbccddeeff\n" + Unspecified(8) +
                              "0102030405060708 a0a1a2a3a4a5a6a7" + Unspecified(8) + "\n";
    HardlockTransformResponseMap both;
    RE2DJ_CHECK(context, ParseHardlockTransformResponseMap(mixed, &both, &error));
    RE2DJ_CHECK_EQ(context, both.blocks.size(), std::size_t{1});
    RE2DJ_CHECK_EQ(context, both.payloads.size(), std::size_t{1});
    if (both.payloads.size() == 1)
    {
        RE2DJ_CHECK_EQ(context, both.payloads[0].block_count, std::size_t{2});
        RE2DJ_CHECK_EQ(context, both.payloads[0].input_mask[0], std::uint8_t{0});
        RE2DJ_CHECK_EQ(context, both.payloads[0].input_mask[8], std::uint8_t{1});
        RE2DJ_CHECK_EQ(context, both.payloads[0].input[8], std::uint8_t{0x01});
        RE2DJ_CHECK_EQ(context, both.payloads[0].output_mask[0], std::uint8_t{1});
        RE2DJ_CHECK_EQ(context, both.payloads[0].output_mask[8], std::uint8_t{0});
    }

    // Request rows must pair equal lengths, whole blocks, and specify at least
    // one byte on each side; half-specified bytes are refused.
    const std::string two_blocks = "00112233445566778899aabbccddeeff";
    const std::string invalid_requests[] = {
        two_blocks + " " + two_blocks.substr(0, 30) + "\n",
        two_blocks + "00 " + two_blocks + "00\n",
        Unspecified(16) + " " + two_blocks + "\n",
        two_blocks + " " + Unspecified(16) + "\n",
        "?" + two_blocks.substr(1) + " " + two_blocks + "\n",
        // Seventeen blocks, one more than a request row may cover.
        std::string(17 * 16, '0') + " " + std::string(17 * 16, '0') + "\n",
    };
    for (const std::string& line : invalid_requests)
    {
        HardlockTransformResponseMap unused;
        RE2DJ_CHECK(context, !ParseHardlockTransformResponseMap(line, &unused, &error));
        RE2DJ_CHECK(context, !error.empty());
    }

    // Two request rows that agree wherever both specify a byte could match
    // the same payload, so the map is refused. Rows that disagree on a
    // byte both specify, or differ in block count, are distinct.
    const std::string first_only = "00" + Unspecified(15);
    const std::string ninth_only = Unspecified(8) + "01" + Unspecified(7);
    HardlockTransformResponseMap ambiguous;
    RE2DJ_CHECK(context,
                !ParseHardlockTransformResponseMap(first_only + " 11" + Unspecified(15) + "\n" +
                                                       ninth_only + " 22" + Unspecified(15) +
                                                       "\n",
                                                   &ambiguous,
                                                   &error));
    RE2DJ_CHECK(context, error.find("same payload") != std::string::npos);
    const std::string key_one = "00" + Unspecified(7) + "01" + Unspecified(7);
    const std::string key_two = "00" + Unspecified(7) + "02" + Unspecified(7);
    const std::string key_three_blocks = key_one + Unspecified(8);
    HardlockTransformResponseMap distinct;
    RE2DJ_CHECK(context,
                ParseHardlockTransformResponseMap(
                    key_one + " 11" + Unspecified(15) + "\n" + key_two + " 22" +
                        Unspecified(15) + "\n" + key_three_blocks + " 33" + Unspecified(23) +
                        "\n",
                    &distinct,
                    &error));
    RE2DJ_CHECK_EQ(context, distinct.payloads.size(), std::size_t{3});

    // Packing lays block rows out as input/output pairs and request rows as
    // fixed-width records.
    std::vector<std::uint8_t> block_rows;
    std::vector<std::uint8_t> payload_records;
    PackHardlockTransformResponseMap(both, &block_rows, &payload_records);
    RE2DJ_CHECK_EQ(context, block_rows.size(), std::size_t{16});
    RE2DJ_CHECK_EQ(context, payload_records.size(), kHardlockPayloadRecordSize);
    if (block_rows.size() == 16)
    {
        RE2DJ_CHECK_EQ(context, block_rows[0], std::uint8_t{0x00});
        RE2DJ_CHECK_EQ(context, block_rows[8], std::uint8_t{0x88});
    }
}
