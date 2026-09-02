#include "re2dj/hle/hardlock/transform_responses.h"

#include <string>
#include <vector>

#include "test_support.h"

void RunHardlockTransformResponsesTests(re2dj::test::Context& context)
{
    using re2dj::hle::hardlock::FindHardlockTransformResponse;
    using re2dj::hle::hardlock::HardlockTransformBlock;
    using re2dj::hle::hardlock::HardlockTransformResponseEntry;
    using re2dj::hle::hardlock::ParseHardlockTransformResponseTable;

    std::vector<HardlockTransformResponseEntry> entries;
    std::string error;

    // Comments, blank lines, trailing comments, and mixed-case hex all parse.
    const std::string text =
        "# challenge response\n"
        "\n"
        "0011223344556677 8899aabbccddeeff\n"
        "  0102030405060708\t1112131415161718  # second entry\n"
        "AABBCCDDEEFF0011 2233445566778899\n";
    RE2DJ_CHECK(context, ParseHardlockTransformResponseTable(text, &entries, &error));
    RE2DJ_CHECK_EQ(context, entries.size(), std::size_t{3});
    RE2DJ_CHECK(context, error.empty());
    RE2DJ_CHECK_EQ(context, entries[0].input[0], std::uint8_t{0x00});
    RE2DJ_CHECK_EQ(context, entries[0].input[7], std::uint8_t{0x77});
    RE2DJ_CHECK_EQ(context, entries[0].output[0], std::uint8_t{0x88});
    RE2DJ_CHECK_EQ(context, entries[0].output[7], std::uint8_t{0xff});
    RE2DJ_CHECK_EQ(context, entries[2].input[0], std::uint8_t{0xaa});

    // Lookup finds an entry and reports a miss without inventing one.
    const HardlockTransformBlock present = {0x01, 0x02, 0x03, 0x04,
                                            0x05, 0x06, 0x07, 0x08};
    const HardlockTransformBlock absent = {0xde, 0xad, 0xbe, 0xef,
                                           0xde, 0xad, 0xbe, 0xef};
    const HardlockTransformBlock* found =
        FindHardlockTransformResponse(entries, present);
    RE2DJ_CHECK(context, found != nullptr);
    if (found != nullptr)
    {
        RE2DJ_CHECK_EQ(context, (*found)[0], std::uint8_t{0x11});
        RE2DJ_CHECK_EQ(context, (*found)[7], std::uint8_t{0x18});
    }
    RE2DJ_CHECK(context, FindHardlockTransformResponse(entries, absent) == nullptr);

    // A repeated challenge is rejected: two outputs for one input would make
    // the result depend on call order.
    std::vector<HardlockTransformResponseEntry> rejected;
    RE2DJ_CHECK(context,
                !ParseHardlockTransformResponseTable(
                    "0011223344556677 8899aabbccddeeff\n"
                    "0011223344556677 0000000000000000\n",
                    &rejected,
                    &error));
    RE2DJ_CHECK(context, error.find("duplicate") != std::string::npos);

    // Wrong digit counts, non-hex digits, a missing output, and extra tokens
    // all fail rather than parsing partially.
    const char* const invalid[] = {
        "0011223344556677 8899aabbccddee\n",
        "00112233445566 8899aabbccddeeff\n",
        "001122334455667g 8899aabbccddeeff\n",
        "0011223344556677\n",
        "0011223344556677 8899aabbccddeeff 0011223344556677\n",
    };
    for (const char* const line : invalid)
    {
        std::vector<HardlockTransformResponseEntry> unused;
        RE2DJ_CHECK(context,
                    !ParseHardlockTransformResponseTable(line, &unused, &error));
        RE2DJ_CHECK(context, !error.empty());
    }

    // A map with no entries is an error rather than a silently empty map,
    // because it would look like an identity run.
    std::vector<HardlockTransformResponseEntry> empty;
    RE2DJ_CHECK(context,
                !ParseHardlockTransformResponseTable("# nothing here\n", &empty, &error));

    // A file without a trailing newline still yields its last entry.
    std::vector<HardlockTransformResponseEntry> tail;
    RE2DJ_CHECK(context,
                ParseHardlockTransformResponseTable(
                    "0011223344556677 8899aabbccddeeff", &tail, &error));
    RE2DJ_CHECK_EQ(context, tail.size(), std::size_t{1});
}
