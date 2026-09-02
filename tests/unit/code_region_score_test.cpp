#include "re2dj/analysis/code_region_score.h"

#include <cstdint>
#include <vector>

#include "test_support.h"

namespace
{

// A deterministic 32-bit LCG. A fixed sequence keeps the ciphertext case
// reproducible; the constants are the ones ISO C lists for rand.
std::vector<std::uint8_t> PseudoRandomBytes(std::size_t count, std::uint32_t seed)
{
    std::vector<std::uint8_t> bytes;
    bytes.reserve(count);
    std::uint32_t state = seed;
    for (std::size_t index = 0; index < count; ++index)
    {
        state = state * 1103515245u + 12345u;
        bytes.push_back(static_cast<std::uint8_t>((state >> 16) & 0xff));
    }
    return bytes;
}

// A stand-in for compiled x86: repeated small functions with frame setup and
// `cc` alignment padding between them. It is not disassembled by anything, so
// only its byte statistics matter.
std::vector<std::uint8_t> SyntheticCodeBytes(std::size_t function_count)
{
    std::vector<std::uint8_t> bytes;
    for (std::size_t index = 0; index < function_count; ++index)
    {
        const std::uint8_t body[] = {
            0x55, 0x8b, 0xec, 0x83, 0xec, 0x08, 0x8b, 0x45,
            0x08, 0x03, 0x45, 0x0c, 0x89, 0x45, 0xfc, 0x8b,
            0x4d, 0xfc, 0x8b, 0xe5, 0x5d, 0xc3,
        };
        bytes.insert(bytes.end(), std::begin(body), std::end(body));
        bytes.insert(bytes.end(), 6, 0xcc);
    }
    return bytes;
}

}  // namespace

void RunCodeRegionScoreTests(re2dj::test::Context& context)
{
    using re2dj::analysis::CodeRegionScore;
    using re2dj::analysis::CodeRegionVerdict;
    using re2dj::analysis::ScoreCodeRegion;
    using re2dj::analysis::ScoreCodeRegionChunks;

    // An empty region measures nothing and claims nothing.
    const CodeRegionScore empty = ScoreCodeRegion({});
    RE2DJ_CHECK_EQ(context, empty.byte_count, std::size_t{0});
    RE2DJ_CHECK_EQ(context, empty.verdict, CodeRegionVerdict::kIndeterminate);

    // A single repeated byte carries no information.
    const std::vector<std::uint8_t> uniform(1024, 0x41);
    const CodeRegionScore uniform_score = ScoreCodeRegion(uniform);
    RE2DJ_CHECK(context, uniform_score.entropy_bits_per_byte == 0.0);
    RE2DJ_CHECK_EQ(context, uniform_score.zero_byte_share, 0.0);
    RE2DJ_CHECK_EQ(context, uniform_score.verdict, CodeRegionVerdict::kIndeterminate);

    // Every byte value once: the maximum, exactly 8 bits per byte.
    std::vector<std::uint8_t> ramp(256, 0);
    for (std::size_t index = 0; index < ramp.size(); ++index)
    {
        ramp[index] = static_cast<std::uint8_t>(index);
    }
    const CodeRegionScore ramp_score = ScoreCodeRegion(ramp);
    RE2DJ_CHECK(context, ramp_score.entropy_bits_per_byte == 8.0);
    RE2DJ_CHECK_EQ(context, ramp_score.zero_byte_share, 1.0 / 256.0);

    // Pseudo-random bytes stand in for the protected image: near-maximum
    // entropy, no prologue, no padding run.
    const std::vector<std::uint8_t> random = PseudoRandomBytes(0x8000, 0x13572468u);
    const CodeRegionScore random_score = ScoreCodeRegion(random);
    RE2DJ_CHECK(context, random_score.entropy_bits_per_byte > 7.9);
    RE2DJ_CHECK_EQ(context, random_score.prologue_count, std::size_t{0});
    RE2DJ_CHECK_EQ(context, random_score.padding_run_count, std::size_t{0});
    RE2DJ_CHECK_EQ(context, random_score.verdict, CodeRegionVerdict::kCiphertextLike);

    // Synthetic code reads the other way on all three signals.
    const std::vector<std::uint8_t> code = SyntheticCodeBytes(64);
    const CodeRegionScore code_score = ScoreCodeRegion(code);
    RE2DJ_CHECK(context, code_score.entropy_bits_per_byte < 7.0);
    RE2DJ_CHECK_EQ(context, code_score.prologue_count, std::size_t{64});
    RE2DJ_CHECK_EQ(context, code_score.padding_run_count, std::size_t{64});
    RE2DJ_CHECK_EQ(context, code_score.verdict, CodeRegionVerdict::kCodeLike);

    // A `cc` run counts once however long it runs, and a run shorter than the
    // minimum does not count at all. This is what makes the metric survive
    // random data, where one byte in 256 is `cc`.
    std::vector<std::uint8_t> padding(512, 0x00);
    padding[16] = 0xcc;
    padding[17] = 0xcc;
    padding[18] = 0xcc;
    for (std::size_t index = 64; index < 96; ++index)
    {
        padding[index] = 0xcc;
    }
    RE2DJ_CHECK_EQ(context, ScoreCodeRegion(padding).padding_run_count, std::size_t{1});

    // High entropy with a prologue present, and low entropy with none, are both
    // refused rather than forced into a verdict.
    std::vector<std::uint8_t> random_with_prologue = random;
    random_with_prologue[0] = 0x55;
    random_with_prologue[1] = 0x8b;
    random_with_prologue[2] = 0xec;
    const CodeRegionScore mixed = ScoreCodeRegion(random_with_prologue);
    RE2DJ_CHECK(context, mixed.prologue_count >= 1);
    RE2DJ_CHECK_EQ(context, mixed.verdict, CodeRegionVerdict::kIndeterminate);
    RE2DJ_CHECK_EQ(context, ScoreCodeRegion(std::vector<std::uint8_t>(1024, 0x00)).verdict,
                   CodeRegionVerdict::kIndeterminate);

    // Too small to measure, whatever it holds.
    const std::vector<std::uint8_t> short_code(code.begin(), code.begin() + 64);
    RE2DJ_CHECK_EQ(context, ScoreCodeRegion(short_code).verdict,
                   CodeRegionVerdict::kIndeterminate);

    // Chunking splits the region and judges each piece on its own, which is how
    // a single decrypted 32 KiB chunk is meant to become visible.
    std::vector<std::uint8_t> mixed_region = PseudoRandomBytes(0x8000, 0x2468ace0u);
    const std::vector<std::uint8_t> code_chunk = SyntheticCodeBytes(1024);
    mixed_region.insert(mixed_region.end(), code_chunk.begin(), code_chunk.end());
    const std::vector<CodeRegionScore> chunks =
        ScoreCodeRegionChunks(mixed_region, 0x8000);
    RE2DJ_CHECK_EQ(context, chunks.size(), std::size_t{2});
    RE2DJ_CHECK_EQ(context, chunks[0].verdict, CodeRegionVerdict::kCiphertextLike);
    RE2DJ_CHECK_EQ(context, chunks[1].verdict, CodeRegionVerdict::kCodeLike);
    RE2DJ_CHECK_EQ(context, ScoreCodeRegionChunks(mixed_region, 0).size(), std::size_t{0});

    // A trailing partial chunk is scored as-is rather than dropped.
    const std::vector<CodeRegionScore> uneven = ScoreCodeRegionChunks(random, 0x3000);
    RE2DJ_CHECK_EQ(context, uneven.size(), std::size_t{3});
    RE2DJ_CHECK_EQ(context, uneven.back().byte_count, std::size_t{0x2000});
}
