#ifndef RE2DJ_HLE_HARDLOCK_TRANSFORM_RESPONSES_H_
#define RE2DJ_HLE_HARDLOCK_TRANSFORM_RESPONSES_H_

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "re2dj/hle/hardlock/payload_responses.h"
#include "re2dj/hle/hardlock/protocol.h"

namespace re2dj::hle::hardlock
{

using HardlockTransformBlock = std::array<std::uint8_t, kHardlockTransformBlockSize>;

// How many block rows the injected runtime can hold.
constexpr std::size_t kHardlockTransformBlockRowCapacity = 256;

// One Function 0x0e challenge and the response it should receive. The response
// is computed outside this repository; nothing here derives it.
struct HardlockTransformResponseEntry
{
    HardlockTransformBlock input = {};
    HardlockTransformBlock output = {};
};

// A parsed response map: rows keyed on a single block, and rows that answer a
// whole request.
struct HardlockTransformResponseMap
{
    std::vector<HardlockTransformResponseEntry> blocks;
    std::vector<HardlockPayloadResponseEntry> payloads;

    bool empty() const { return blocks.empty() && payloads.empty(); }
};

// Parses a response map, one row per line as "<input> <output>". Blank lines
// and lines whose first non-space character is '#' are ignored.
//
// The token length decides the row kind. Sixteen hex digits make a block row,
// applied to each block of any transform. A token covering 2 to 16 blocks, in
// which any byte may be "??", makes a request row, applied to the whole
// payload of a transform with that many blocks; see payload_responses.h.
//
// A repeated block input and a pair of request rows that could match the same
// payload are both rejected, because either would make the result depend on
// row order.
bool ParseHardlockTransformResponseMap(std::string_view text,
                                       HardlockTransformResponseMap* map,
                                       std::string* error);

// Returns the mapped output for one challenge, or nullptr when the map has no
// entry for it. Linear search is deliberate: the observed maps hold a few dozen
// entries.
const HardlockTransformBlock* FindHardlockTransformResponse(
    const std::vector<HardlockTransformResponseEntry>& entries,
    const HardlockTransformBlock& input);

// Serialises a map for the injected runtime: block rows as consecutive
// input/output pairs, and request rows as payload_responses.h records.
void PackHardlockTransformResponseMap(const HardlockTransformResponseMap& map,
                                      std::vector<std::uint8_t>* block_rows,
                                      std::vector<std::uint8_t>* payload_records);

}  // namespace re2dj::hle::hardlock

#endif  // RE2DJ_HLE_HARDLOCK_TRANSFORM_RESPONSES_H_
