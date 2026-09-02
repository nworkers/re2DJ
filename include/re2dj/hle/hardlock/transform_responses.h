#ifndef RE2DJ_HLE_HARDLOCK_TRANSFORM_RESPONSES_H_
#define RE2DJ_HLE_HARDLOCK_TRANSFORM_RESPONSES_H_

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace re2dj::hle::hardlock
{

constexpr std::size_t kHardlockTransformBlockSize = 8;

using HardlockTransformBlock = std::array<std::uint8_t, kHardlockTransformBlockSize>;

// One Function 0x0e challenge and the response it should receive. The response
// is computed outside this repository; nothing here derives it.
struct HardlockTransformResponseEntry
{
    HardlockTransformBlock input = {};
    HardlockTransformBlock output = {};
};

// Parses a response map: one entry per line as "<16 hex input> <16 hex output>".
// Blank lines and lines whose first non-space character is '#' are ignored. A
// repeated input is rejected, because two different outputs for one challenge
// would make the run depend on call order.
bool ParseHardlockTransformResponseTable(
    std::string_view text,
    std::vector<HardlockTransformResponseEntry>* entries,
    std::string* error);

// Returns the mapped output for one challenge, or nullptr when the map has no
// entry for it. Linear search is deliberate: the observed maps hold a few dozen
// entries.
const HardlockTransformBlock* FindHardlockTransformResponse(
    const std::vector<HardlockTransformResponseEntry>& entries,
    const HardlockTransformBlock& input);

}  // namespace re2dj::hle::hardlock

#endif  // RE2DJ_HLE_HARDLOCK_TRANSFORM_RESPONSES_H_
