#ifndef RE2DJ_EXE_CODE_SCAN_H
#define RE2DJ_EXE_CODE_SCAN_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace re2dj::exe {

// Result of searching backward for a frame-pointer prologue. The search is
// syntactic: the byte sequence can also occur inside an unrelated instruction
// or in data embedded in code, so a hit is a candidate function start.
struct PrologueSearchResult
{
    bool found = false;
    std::size_t offset = 0;
    // Distance in bytes from the prologue to the anchor that was searched from.
    std::size_t distance = 0;
};

// Searches backward from `anchor` for the nearest `push ebp; mov ebp, esp`
// (`55 8b ec`) within `max_scan_back` bytes. The anchor itself is excluded, so
// an anchor sitting exactly on a prologue finds the previous one.
PrologueSearchResult FindPrologueBefore(const std::uint8_t* bytes,
                                        std::size_t size,
                                        std::size_t anchor,
                                        std::size_t max_scan_back);

// One `call rel32` or `jmp rel32` whose destination matched the requested
// address. Like the prologue search this is syntactic: the five bytes can also
// occur inside another instruction or in data.
struct RelativeBranchSite
{
    std::uint32_t offset = 0;
    // 0xe8 for `call rel32`, 0xe9 for `jmp rel32`.
    std::uint8_t opcode = 0;
};

// A near branch or call found while listing a code range, with the destination
// already resolved. The listing walks bytes linearly without decoding
// instruction lengths, so an entry can also be a byte pattern inside another
// instruction; entries are candidates, not a disassembly.
struct NearBranch
{
    std::uint32_t offset = 0;
    // 0xe8 call rel32, 0xe9 jmp rel32, 0xeb jmp rel8, 0x70-0x7f jcc rel8,
    // 0x80-0x8f for the two-byte `0f 8x` jcc rel32 forms.
    std::uint8_t opcode = 0;
    bool near_form = false;
    std::uint32_t target = 0;
};

// Lists the near branches and calls inside `[start, start + length)` of
// `bytes`, where `base_address` is the address `bytes[0]` is mapped at.
// Collecting stops after `max_branches` while counting continues, so
// `total_branches` stays complete; both out parameters may be null.
std::vector<NearBranch> ListNearBranches(const std::uint8_t* bytes,
                                         std::size_t size,
                                         std::uint32_t base_address,
                                         std::size_t start,
                                         std::size_t length,
                                         std::size_t max_branches,
                                         bool* capped,
                                         std::size_t* total_branches);

// Finds every `call`/`jmp rel32` in `bytes` that reaches `target_address`,
// where `base_address` is the address `bytes[0]` is mapped at. Collecting stops
// after `max_sites` while counting continues, so `total_sites` stays complete;
// both out parameters may be null.
std::vector<RelativeBranchSite> ScanRelativeBranches(const std::uint8_t* bytes,
                                                     std::size_t size,
                                                     std::uint32_t base_address,
                                                     std::uint32_t target_address,
                                                     std::size_t max_sites,
                                                     bool* capped,
                                                     std::size_t* total_sites);


// A legacy port-I/O helper found by signature. These are the compiler
// runtime's `inp`/`outp` family: a build places them at its own address but
// keeps the same shape, so a fixed byte sequence locates them.
enum class PortHelperKind
{
    kInPortByte,
    kInPortWord,
    kInPortDword,
    kOutPortByte,
    kOutPortWord,
};

// Returns the helper's stable name, for diagnostics and tool output.
const char* PortHelperKindName(PortHelperKind kind);

// One helper candidate. Like the rest of this header the match is syntactic:
// the same bytes can occur inside another instruction or in data embedded in
// code, so a hit is a candidate, not a proven helper. `opcode_address` is the
// address of the `in`/`out` instruction itself, which is what a privileged
// fault reports and what a target profile stores.
struct PortHelperSite
{
    PortHelperKind kind = PortHelperKind::kInPortByte;
    // Offset of the first signature byte within `bytes`.
    std::uint32_t signature_offset = 0;
    // Address of the `in`/`out` instruction, already resolved against the base.
    std::uint32_t opcode_address = 0;
};

// Finds legacy port-I/O helpers in `bytes`, where `base_address` is the address
// `bytes[0]` is mapped at. Pass 0 to get offsets, or the image base to get
// virtual addresses. Callers hand this either a build whose `.text` is
// plaintext on disk or a decrypted run-time image dump; in both the file offset
// is the RVA. When several signatures match at one offset only the longest is
// reported. Collecting stops after `max_sites` while counting continues, so
// `total_sites` stays complete; both out parameters may be null.
std::vector<PortHelperSite> ScanPortHelpers(const std::uint8_t* bytes,
                                            std::size_t size,
                                            std::uint32_t base_address,
                                            std::size_t max_sites,
                                            bool* capped,
                                            std::size_t* total_sites);

}  // namespace re2dj::exe

#endif
