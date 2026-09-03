#ifndef RE2DJ_EXE_IMMEDIATE_SCAN_H
#define RE2DJ_EXE_IMMEDIATE_SCAN_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace re2dj::exe {

// A 32-bit little-endian value found inside a code buffer. The scan is
// syntactic and uses no instruction decoder, so a reference is a candidate
// operand rather than a confirmed one: the same bytes can appear inside a
// displacement, an unrelated immediate, or data embedded in code.
struct ImmediateReference
{
    // Byte offset of the matched value within the scanned buffer.
    std::uint32_t offset = 0;
    std::uint32_t value = 0;
    // Bytes surrounding the value, oldest first, so callers can classify the
    // enclosing instruction and what follows it without a decoder. Fewer than
    // `kContextBytes` are returned near either end of the buffer.
    static constexpr std::size_t kContextBytes = 8;
    std::uint8_t leading[kContextBytes] = {};
    std::uint8_t leading_count = 0;
    std::uint8_t trailing[kContextBytes] = {};
    std::uint8_t trailing_count = 0;
};

// Scans `bytes` for any of `values`, returning matches ordered by offset.
// Collecting stops after `max_matches`, but the scan continues so the caller
// still learns how many matches exist: `total_matches` receives the full count
// and `capped` reports the truncation. Both out parameters may be null.
std::vector<ImmediateReference> ScanImmediateReferences(const std::uint8_t* bytes,
                                                        std::size_t size,
                                                        const std::uint32_t* values,
                                                        std::size_t value_count,
                                                        std::size_t max_matches,
                                                        bool* capped,
                                                        std::size_t* total_matches);

}  // namespace re2dj::exe

#endif
