#include "re2dj/analysis/code_region_score.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace re2dj::analysis
{
namespace
{

constexpr std::uint8_t kPrologue[] = {0x55, 0x8b, 0xec};
constexpr std::uint8_t kPaddingByte = 0xcc;

double ShannonEntropy(const std::array<std::size_t, 256>& histogram, std::size_t total)
{
    if (total == 0)
    {
        return 0.0;
    }
    const double count = static_cast<double>(total);
    double entropy = 0.0;
    for (const std::size_t bucket : histogram)
    {
        if (bucket == 0)
        {
            continue;
        }
        const double probability = static_cast<double>(bucket) / count;
        entropy -= probability * std::log2(probability);
    }
    return entropy;
}

CodeRegionVerdict Judge(const CodeRegionScore& score)
{
    if (score.byte_count < kMinimumJudgeableBytes)
    {
        return CodeRegionVerdict::kIndeterminate;
    }
    if (score.entropy_bits_per_byte <= kCodeEntropyCeiling && score.prologue_count > 0)
    {
        return CodeRegionVerdict::kCodeLike;
    }
    if (score.entropy_bits_per_byte >= kCiphertextEntropyFloor && score.prologue_count == 0)
    {
        return CodeRegionVerdict::kCiphertextLike;
    }
    return CodeRegionVerdict::kIndeterminate;
}

}  // namespace

CodeRegionScore ScoreCodeRegion(std::span<const std::uint8_t> bytes)
{
    CodeRegionScore score;
    score.byte_count = bytes.size();
    if (bytes.empty())
    {
        return score;
    }

    std::array<std::size_t, 256> histogram = {};
    std::size_t padding_run = 0;
    for (std::size_t index = 0; index < bytes.size(); ++index)
    {
        const std::uint8_t value = bytes[index];
        ++histogram[value];
        if (value == kPaddingByte)
        {
            ++padding_run;
            // Counted once when the run reaches the minimum, so a longer run
            // does not inflate the count.
            if (padding_run == kPaddingRunMinimum)
            {
                ++score.padding_run_count;
            }
        }
        else
        {
            padding_run = 0;
        }
        if (index + std::size(kPrologue) <= bytes.size() &&
            std::equal(std::begin(kPrologue), std::end(kPrologue), bytes.begin() + static_cast<std::ptrdiff_t>(index)))
        {
            ++score.prologue_count;
        }
    }

    score.entropy_bits_per_byte = ShannonEntropy(histogram, bytes.size());
    score.zero_byte_share = static_cast<double>(histogram[0]) / static_cast<double>(bytes.size());
    score.verdict = Judge(score);
    return score;
}

std::vector<CodeRegionScore> ScoreCodeRegionChunks(std::span<const std::uint8_t> bytes,
                                                   std::size_t chunk_size)
{
    std::vector<CodeRegionScore> scores;
    if (chunk_size == 0 || bytes.empty())
    {
        return scores;
    }
    scores.reserve(bytes.size() / chunk_size + 1);
    for (std::size_t offset = 0; offset < bytes.size(); offset += chunk_size)
    {
        const std::size_t length = (std::min)(chunk_size, bytes.size() - offset);
        scores.push_back(ScoreCodeRegion(bytes.subspan(offset, length)));
    }
    return scores;
}

std::string_view CodeRegionVerdictName(CodeRegionVerdict verdict)
{
    switch (verdict)
    {
    case CodeRegionVerdict::kIndeterminate:
        return "indeterminate";
    case CodeRegionVerdict::kCiphertextLike:
        return "ciphertext-like";
    case CodeRegionVerdict::kCodeLike:
        return "code-like";
    }
    return "indeterminate";
}

}  // namespace re2dj::analysis
