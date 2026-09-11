#ifndef RE2DJ_HLE_HARDLOCK_PAYLOAD_RESPONSES_H_
#define RE2DJ_HLE_HARDLOCK_PAYLOAD_RESPONSES_H_

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "re2dj/hle/hardlock/protocol.h"

namespace re2dj::hle::hardlock
{

// A transform answered as a whole request rather than block by block. The
// Function 0x11 request observed on ez2d2m needs this: its answer depends on
// each block's position, and part of its input changes from run to run, so no
// single-block key can hold it. The answer is computed outside this
// repository; this module only matches and applies it.
constexpr std::size_t kHardlockPayloadMinBlocks = 2;
constexpr std::size_t kHardlockPayloadMaxBlocks = 16;
constexpr std::size_t kHardlockPayloadMaxBytes =
    kHardlockPayloadMaxBlocks * kHardlockTransformBlockSize;

// Add groups are whole 32-bit little-endian words, because the one transform
// that needs them (API_CODE block 3) adds two DWORDs into the guest buffer.
constexpr std::size_t kHardlockPayloadAddGroup = 4;

struct HardlockPayloadResponseEntry
{
    std::size_t block_count = 0;
    // One mask byte per payload byte, non-zero where the row specifies that
    // byte. An unspecified input byte is not compared; an unspecified output
    // byte keeps the guest's value.
    std::vector<std::uint8_t> input;
    std::vector<std::uint8_t> input_mask;
    std::vector<std::uint8_t> output;
    std::vector<std::uint8_t> output_mask;
    // Bytes added, as 32-bit little-endian words with carry, to the guest's
    // value rather than written. Empty when the row has no add token. Each set
    // add byte lies in a fully-specified four-byte group disjoint from
    // output_mask, so a byte is written, added, or kept — never two of these.
    std::vector<std::uint8_t> output_add;
    std::vector<std::uint8_t> output_add_mask;
};

// Parses one request-row token: two characters per byte, each pair either
// two hex digits or "??". The token must cover kHardlockPayloadMinBlocks to
// kHardlockPayloadMaxBlocks whole blocks. On success bytes and mask have one
// entry per byte.
bool ParseHardlockPayloadPattern(std::string_view token,
                                 std::vector<std::uint8_t>* bytes,
                                 std::vector<std::uint8_t>* mask,
                                 std::string* error);

// True when some payload would match both rows: same block count, and equal
// bytes wherever both specify one. Such a pair makes the answer depend on
// row order, so a map containing it is rejected.
bool HardlockPayloadResponsesOverlap(const HardlockPayloadResponseEntry& first,
                                     const HardlockPayloadResponseEntry& second);

// Checks the internal consistency a hand-written or parsed row must satisfy:
// field sizes match block_count, write and add masks are disjoint, and every
// set add byte belongs to a fully-specified four-byte group. Returns false and
// sets error otherwise.
bool ValidateHardlockPayloadResponse(const HardlockPayloadResponseEntry& entry,
                                     std::string* error);

// Returns the row whose specified input bytes all equal the payload, or
// nullptr. The parser guarantees at most one row can match.
const HardlockPayloadResponseEntry* FindHardlockPayloadResponse(
    const std::vector<HardlockPayloadResponseEntry>& entries,
    std::span<const std::uint8_t> payload);

// Writes the row's specified output bytes into the payload, adds its add-word
// groups to the guest's value, and leaves every other byte as the guest sent
// it. The payload must be the row's size.
void ApplyHardlockPayloadResponse(const HardlockPayloadResponseEntry& entry,
                                  std::span<std::uint8_t> payload);

// Fixed-width record used to hand request rows to the injected runtime: a
// little-endian u32 block count, then input, input mask, output, output mask,
// add and add mask, each kHardlockPayloadMaxBytes wide.
constexpr std::size_t kHardlockPayloadRecordSize = 4 + kHardlockPayloadMaxBytes * 6;
// How many request rows the runtime can hold.
constexpr std::size_t kHardlockPayloadRecordCapacity = 8;

void PackHardlockPayloadResponse(const HardlockPayloadResponseEntry& entry,
                                 std::span<std::uint8_t> record);

// Rebuilds a row from a record, rejecting a block count outside the row
// contract.
bool UnpackHardlockPayloadResponse(std::span<const std::uint8_t> record,
                                   HardlockPayloadResponseEntry* entry);

}  // namespace re2dj::hle::hardlock

#endif  // RE2DJ_HLE_HARDLOCK_PAYLOAD_RESPONSES_H_
