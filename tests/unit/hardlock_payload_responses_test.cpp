#include "re2dj/hle/hardlock/payload_responses.h"

#include <string>
#include <vector>

#include "test_support.h"

namespace
{

using re2dj::hle::hardlock::HardlockPayloadResponseEntry;

// Builds a synthetic request row from two pattern tokens.
bool MakeRow(const std::string& input,
             const std::string& output,
             HardlockPayloadResponseEntry* entry,
             std::string* error)
{
    using re2dj::hle::hardlock::ParseHardlockPayloadPattern;
    if (!ParseHardlockPayloadPattern(input, &entry->input, &entry->input_mask, error) ||
        !ParseHardlockPayloadPattern(output, &entry->output, &entry->output_mask, error))
    {
        return false;
    }
    entry->block_count = entry->input.size() / 8;
    return true;
}

}  // namespace

void RunHardlockPayloadResponsesTests(re2dj::test::Context& context)
{
    using re2dj::hle::hardlock::ApplyHardlockPayloadResponse;
    using re2dj::hle::hardlock::FindHardlockPayloadResponse;
    using re2dj::hle::hardlock::HardlockPayloadResponsesOverlap;
    using re2dj::hle::hardlock::kHardlockPayloadRecordSize;
    using re2dj::hle::hardlock::PackHardlockPayloadResponse;
    using re2dj::hle::hardlock::ParseHardlockPayloadPattern;
    using re2dj::hle::hardlock::UnpackHardlockPayloadResponse;
    using re2dj::hle::hardlock::ValidateHardlockPayloadResponse;

    std::string error;

    // "??" leaves a byte unspecified; hex digits of either case specify it.
    std::vector<std::uint8_t> bytes;
    std::vector<std::uint8_t> mask;
    RE2DJ_CHECK(context,
                ParseHardlockPayloadPattern(
                    "Ab??000000000000" "00000000000000ff", &bytes, &mask, &error));
    RE2DJ_CHECK_EQ(context, bytes.size(), std::size_t{16});
    RE2DJ_CHECK_EQ(context, bytes[0], std::uint8_t{0xab});
    RE2DJ_CHECK_EQ(context, mask[0], std::uint8_t{1});
    RE2DJ_CHECK_EQ(context, bytes[1], std::uint8_t{0x00});
    RE2DJ_CHECK_EQ(context, mask[1], std::uint8_t{0});
    RE2DJ_CHECK_EQ(context, bytes[15], std::uint8_t{0xff});

    // One block is a block row, not a request row, and is refused here.
    RE2DJ_CHECK(context,
                !ParseHardlockPayloadPattern("0011223344556677", &bytes, &mask, &error));

    // A three-block row keyed on its middle block, answering the first block
    // and leaving the other two as the guest sent them.
    HardlockPayloadResponseEntry row;
    const std::string input = std::string(16, '?') + "5a5a5a5a5a5a5a5a" + std::string(16, '?');
    const std::string output = "c0ffee0102030405" + std::string(32, '?');
    RE2DJ_CHECK(context, MakeRow(input, output, &row, &error));
    RE2DJ_CHECK_EQ(context, row.block_count, std::size_t{3});
    const std::vector<HardlockPayloadResponseEntry> rows = {row};

    std::vector<std::uint8_t> payload(24, 0x77);
    for (std::size_t index = 8; index < 16; ++index)
    {
        payload[index] = 0x5a;
    }
    // Bytes the row leaves unspecified may hold anything.
    payload[0] = 0x11;
    payload[23] = 0x22;
    const HardlockPayloadResponseEntry* found = FindHardlockPayloadResponse(rows, payload);
    RE2DJ_CHECK(context, found != nullptr);
    if (found != nullptr)
    {
        ApplyHardlockPayloadResponse(*found, payload);
    }
    RE2DJ_CHECK_EQ(context, payload[0], std::uint8_t{0xc0});
    RE2DJ_CHECK_EQ(context, payload[7], std::uint8_t{0x05});
    RE2DJ_CHECK_EQ(context, payload[8], std::uint8_t{0x5a});
    RE2DJ_CHECK_EQ(context, payload[23], std::uint8_t{0x22});

    // One differing specified byte, or a different size, is a miss.
    std::vector<std::uint8_t> other(24, 0x5a);
    other[15] = 0x5b;
    RE2DJ_CHECK(context, FindHardlockPayloadResponse(rows, other) == nullptr);
    const std::vector<std::uint8_t> shorter(16, 0x5a);
    RE2DJ_CHECK(context, FindHardlockPayloadResponse(rows, shorter) == nullptr);

    // Overlap: agreement on every commonly specified byte, same size.
    HardlockPayloadResponseEntry disjoint;
    RE2DJ_CHECK(context,
                MakeRow("01" + std::string(46, '?'), "02" + std::string(46, '?'), &disjoint,
                        &error));
    RE2DJ_CHECK(context, HardlockPayloadResponsesOverlap(row, disjoint));
    HardlockPayloadResponseEntry conflicting;
    RE2DJ_CHECK(context,
                MakeRow(std::string(16, '?') + "5a5a5a5a5a5a5a5b" + std::string(16, '?'),
                        "03" + std::string(46, '?'),
                        &conflicting,
                        &error));
    RE2DJ_CHECK(context, !HardlockPayloadResponsesOverlap(row, conflicting));
    HardlockPayloadResponseEntry two_blocks;
    RE2DJ_CHECK(context,
                MakeRow(std::string(16, '?') + "5a5a5a5a5a5a5a5a", "04" + std::string(30, '?'),
                        &two_blocks,
                        &error));
    RE2DJ_CHECK(context, !HardlockPayloadResponsesOverlap(row, two_blocks));

    // A record round-trips every field.
    std::vector<std::uint8_t> record(kHardlockPayloadRecordSize, 0xee);
    PackHardlockPayloadResponse(row, record);
    RE2DJ_CHECK_EQ(context, record[0], std::uint8_t{3});
    RE2DJ_CHECK_EQ(context, record[1], std::uint8_t{0});
    HardlockPayloadResponseEntry restored;
    RE2DJ_CHECK(context, UnpackHardlockPayloadResponse(record, &restored));
    RE2DJ_CHECK_EQ(context, restored.block_count, row.block_count);
    RE2DJ_CHECK(context, restored.input == row.input);
    RE2DJ_CHECK(context, restored.input_mask == row.input_mask);
    RE2DJ_CHECK(context, restored.output == row.output);
    RE2DJ_CHECK(context, restored.output_mask == row.output_mask);

    // A record whose block count is outside the row contract, or which
    // specifies nothing, or which is truncated, does not unpack.
    std::vector<std::uint8_t> bad_count = record;
    bad_count[0] = 1;
    RE2DJ_CHECK(context, !UnpackHardlockPayloadResponse(bad_count, &restored));
    bad_count[0] = 17;
    RE2DJ_CHECK(context, !UnpackHardlockPayloadResponse(bad_count, &restored));
    std::vector<std::uint8_t> blank(kHardlockPayloadRecordSize, 0);
    blank[0] = 2;
    RE2DJ_CHECK(context, !UnpackHardlockPayloadResponse(blank, &restored));
    const std::vector<std::uint8_t> truncated(record.begin(), record.end() - 1);
    RE2DJ_CHECK(context, !UnpackHardlockPayloadResponse(truncated, &restored));

    // A hand-built row with inconsistent field sizes is never matched or
    // applied, so it cannot read or write out of bounds.
    HardlockPayloadResponseEntry malformed = row;
    malformed.output_mask.pop_back();
    const std::vector<HardlockPayloadResponseEntry> malformed_rows = {malformed};
    std::vector<std::uint8_t> untouched(24, 0x5a);
    RE2DJ_CHECK(context, FindHardlockPayloadResponse(malformed_rows, untouched) == nullptr);
    ApplyHardlockPayloadResponse(malformed, untouched);
    RE2DJ_CHECK_EQ(context, untouched[0], std::uint8_t{0x5a});

    // Add groups: 32-bit little-endian words added with carry to the guest buffer.
    HardlockPayloadResponseEntry add_row = row;
    add_row.output_add.assign(24, 0);
    add_row.output_add_mask.assign(24, 0);
    // Add 0x12345678 to bytes 8..11 (group 2).
    add_row.output_add[8] = 0x78;
    add_row.output_add[9] = 0x56;
    add_row.output_add[10] = 0x34;
    add_row.output_add[11] = 0x12;
    for (std::size_t i = 8; i < 12; ++i)
    {
        add_row.output_add_mask[i] = 1;
    }
    RE2DJ_CHECK(context, ValidateHardlockPayloadResponse(add_row, &error));

    std::vector<std::uint8_t> add_payload(24, 0);
    // Initial value in bytes 8..11: 0x00000002
    add_payload[8] = 0x02;
    ApplyHardlockPayloadResponse(add_row, add_payload);
    // Written output: bytes 0..7
    RE2DJ_CHECK_EQ(context, add_payload[0], std::uint8_t{0xc0});
    RE2DJ_CHECK_EQ(context, add_payload[7], std::uint8_t{0x05});
    // Added output: 0x12345678 + 0x02 = 0x1234567a
    RE2DJ_CHECK_EQ(context, add_payload[8], std::uint8_t{0x7a});
    RE2DJ_CHECK_EQ(context, add_payload[9], std::uint8_t{0x56});
    RE2DJ_CHECK_EQ(context, add_payload[10], std::uint8_t{0x34});
    RE2DJ_CHECK_EQ(context, add_payload[11], std::uint8_t{0x12});

    // Validation rejects bytes that are both written and added.
    HardlockPayloadResponseEntry conflict_row = add_row;
    conflict_row.output_mask[8] = 1;
    RE2DJ_CHECK(context, !ValidateHardlockPayloadResponse(conflict_row, &error));

    // Validation rejects add groups that are only partly specified.
    HardlockPayloadResponseEntry partial_add = add_row;
    partial_add.output_add_mask[11] = 0;
    RE2DJ_CHECK(context, !ValidateHardlockPayloadResponse(partial_add, &error));

    // Record round-trip with add fields preserved.
    std::vector<std::uint8_t> add_record(kHardlockPayloadRecordSize, 0);
    PackHardlockPayloadResponse(add_row, add_record);
    HardlockPayloadResponseEntry restored_add;
    RE2DJ_CHECK(context, UnpackHardlockPayloadResponse(add_record, &restored_add));
    RE2DJ_CHECK(context, restored_add.output_add == add_row.output_add);
    RE2DJ_CHECK(context, restored_add.output_add_mask == add_row.output_add_mask);
}

