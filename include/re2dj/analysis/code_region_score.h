#ifndef RE2DJ_ANALYSIS_CODE_REGION_SCORE_H_
#define RE2DJ_ANALYSIS_CODE_REGION_SCORE_H_

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace re2dj::analysis
{

// Statistical judgement of whether a byte region reads as 32-bit x86 code or as
// ciphertext. The protected ez2dj4th image is ciphertext throughout, so a
// Hardlock transform response candidate is judged by whether the region it is
// supposed to decrypt stops looking like ciphertext.
//
// Nothing here decrypts anything or knows any transform. It only measures bytes
// someone else produced.

enum class CodeRegionVerdict
{
    // Too small to measure, or measurements that match neither profile.
    kIndeterminate,
    // High entropy with no code landmarks: an undecrypted region, which is what
    // every observed ez2dj4th section currently reads as.
    kCiphertextLike,
    // Entropy in the code range with at least one function prologue. This marks
    // a candidate for human confirmation; it is not a decryption proof.
    kCodeLike,
};

struct CodeRegionScore
{
    std::size_t byte_count = 0;
    // Shannon entropy of the byte histogram, in bits per byte, 0.0 to 8.0.
    double entropy_bits_per_byte = 0.0;
    // Occurrences of the period MSVC frame setup `55 8b ec`.
    std::size_t prologue_count = 0;
    // Maximal runs of `cc` at least kPaddingRunMinimum bytes long. Counting
    // runs rather than bytes matters: uniform random data carries one `cc` byte
    // in 256, but a run of four has probability 2^-32 per position.
    std::size_t padding_run_count = 0;
    // Share of `00` bytes, 0.0 to 1.0. Uniform random data converges to 1/256.
    double zero_byte_share = 0.0;
    CodeRegionVerdict verdict = CodeRegionVerdict::kIndeterminate;
};

// Verdict thresholds. These are heuristics, not facts confirmed from the
// original binary. They sit in the empty band between two confirmed
// measurements: every observed ez2dj4th section reads 7.896 or higher with zero
// prologues, while x86 code of this era is expected in the sixes.
inline constexpr double kCodeEntropyCeiling = 7.0;
inline constexpr double kCiphertextEntropyFloor = 7.9;
inline constexpr std::size_t kPaddingRunMinimum = 4;
// Below this, byte statistics say nothing: a 64-byte fault page dump reaches
// entropy 6 whatever it holds, because 64 samples cannot fill 256 buckets.
inline constexpr std::size_t kMinimumJudgeableBytes = 256;
// The transform challenge granularity, so a candidate that decrypts one chunk
// changes exactly that chunk's verdict.
inline constexpr std::size_t kDefaultChunkBytes = 0x8000;

CodeRegionScore ScoreCodeRegion(std::span<const std::uint8_t> bytes);

// Splits `bytes` into `chunk_size` pieces and scores each. A trailing partial
// chunk is scored as-is; it may fall below kMinimumJudgeableBytes and read
// indeterminate, which is the honest answer for it.
std::vector<CodeRegionScore> ScoreCodeRegionChunks(std::span<const std::uint8_t> bytes,
                                                   std::size_t chunk_size);

std::string_view CodeRegionVerdictName(CodeRegionVerdict verdict);

}  // namespace re2dj::analysis

#endif  // RE2DJ_ANALYSIS_CODE_REGION_SCORE_H_
