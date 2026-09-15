#ifndef RE2DJ_GRAPHICS_PRESENT_INTERVAL_HISTOGRAM_H_
#define RE2DJ_GRAPHICS_PRESENT_INTERVAL_HISTOGRAM_H_

#include <array>
#include <cstddef>
#include <cstdint>

namespace re2dj::graphics
{

// Accumulates the spacing between presents so the shape of a guest's frame
// pacing can be read, not just its average.
//
// An average hides what matters here. A guest capped by a refresh boundary and
// a guest running its own millisecond timer can report the same mean while
// their distributions look nothing alike: the first clusters on multiples of
// the refresh period, the second on adjacent whole milliseconds. Only the
// distribution separates them.
//
// Adding a sample is a bucket increment, so this can sit on the present path
// without changing the timing it measures. Formatting happens once per summary.
class PresentIntervalHistogram
{
public:
    // 0.25 ms buckets over 0-40 ms. The width resolves whole-millisecond
    // clustering, and the range covers everything from an uncapped present to
    // a frame that missed several 60 Hz refreshes; anything slower lands in
    // the overflow bucket, where its exact value no longer changes the reading.
    static constexpr double kBucketMilliseconds = 0.25;
    static constexpr std::size_t kBucketCount = 160;
    static constexpr double kRangeMilliseconds = kBucketMilliseconds * kBucketCount;
    // How many of the highest-count buckets a summary reports.
    static constexpr std::size_t kReportedBuckets = 5;

    struct Bucket
    {
        // Inclusive lower edge of the bucket, in milliseconds.
        double lower_milliseconds = 0.0;
        std::uint32_t count = 0;
    };

    struct Summary
    {
        std::uint32_t samples = 0;
        double mean_milliseconds = 0.0;
        double minimum_milliseconds = 0.0;
        double median_milliseconds = 0.0;
        double percentile95_milliseconds = 0.0;
        double maximum_milliseconds = 0.0;
        // Highest-count buckets, most populated first. Unused entries keep a
        // zero count. Percentiles come from bucket lower edges, so they are
        // accurate to the bucket width rather than exact.
        std::array<Bucket, kReportedBuckets> top_buckets = {};
    };

    // Ignores a non-finite or negative interval rather than distorting the
    // distribution: the caller's clock can go backwards across a counter
    // glitch, and one bad sample must not be readable as a real frame.
    void Add(double milliseconds);

    Summary Summarize() const;
    void Reset();

    std::uint32_t samples() const { return samples_; }

private:
    std::array<std::uint32_t, kBucketCount + 1> buckets_ = {};
    std::uint32_t samples_ = 0;
    double total_milliseconds_ = 0.0;
    double minimum_milliseconds_ = 0.0;
    double maximum_milliseconds_ = 0.0;
};

}  // namespace re2dj::graphics

#endif  // RE2DJ_GRAPHICS_PRESENT_INTERVAL_HISTOGRAM_H_
