#include "re2dj/graphics/present_interval_histogram.h"

#include <cmath>

namespace re2dj::graphics
{
namespace
{

// The value reported for a bucket is its inclusive lower edge. The overflow
// bucket reports the end of the covered range, which reads as "at least this".
double BucketLowerEdge(std::size_t index)
{
    return static_cast<double>(index) * PresentIntervalHistogram::kBucketMilliseconds;
}

// Walks the buckets until the requested fraction of samples has been passed and
// returns that bucket's lower edge. Percentiles are therefore accurate to the
// bucket width, which is all the distribution shape needs.
double PercentileFromBuckets(const std::array<std::uint32_t,
                                              PresentIntervalHistogram::kBucketCount + 1>& buckets,
                             std::uint32_t samples,
                             double fraction)
{
    if (samples == 0)
    {
        return 0.0;
    }
    const double target = static_cast<double>(samples) * fraction;
    double seen = 0.0;
    for (std::size_t index = 0; index < buckets.size(); ++index)
    {
        seen += static_cast<double>(buckets[index]);
        if (seen >= target)
        {
            return BucketLowerEdge(index);
        }
    }
    return BucketLowerEdge(buckets.size() - 1);
}

}  // namespace

void PresentIntervalHistogram::Add(double milliseconds)
{
    if (!std::isfinite(milliseconds) || milliseconds < 0.0)
    {
        return;
    }
    std::size_t index = kBucketCount;
    if (milliseconds < kRangeMilliseconds)
    {
        index = static_cast<std::size_t>(milliseconds / kBucketMilliseconds);
        if (index >= kBucketCount)
        {
            index = kBucketCount - 1;
        }
    }
    ++buckets_[index];
    if (samples_ == 0 || milliseconds < minimum_milliseconds_)
    {
        minimum_milliseconds_ = milliseconds;
    }
    if (samples_ == 0 || milliseconds > maximum_milliseconds_)
    {
        maximum_milliseconds_ = milliseconds;
    }
    ++samples_;
    total_milliseconds_ += milliseconds;
}

PresentIntervalHistogram::Summary PresentIntervalHistogram::Summarize() const
{
    Summary summary;
    summary.samples = samples_;
    if (samples_ == 0)
    {
        return summary;
    }
    summary.mean_milliseconds = total_milliseconds_ / static_cast<double>(samples_);
    summary.minimum_milliseconds = minimum_milliseconds_;
    summary.maximum_milliseconds = maximum_milliseconds_;
    summary.median_milliseconds = PercentileFromBuckets(buckets_, samples_, 0.5);
    summary.percentile95_milliseconds = PercentileFromBuckets(buckets_, samples_, 0.95);

    // Selection sort over the reported count rather than sorting every bucket:
    // the list is short and fixed, and this keeps the summary allocation-free.
    std::array<bool, kBucketCount + 1> taken = {};
    for (std::size_t slot = 0; slot < kReportedBuckets; ++slot)
    {
        std::size_t best = buckets_.size();
        for (std::size_t index = 0; index < buckets_.size(); ++index)
        {
            if (taken[index] || buckets_[index] == 0)
            {
                continue;
            }
            if (best == buckets_.size() || buckets_[index] > buckets_[best])
            {
                best = index;
            }
        }
        if (best == buckets_.size())
        {
            break;
        }
        taken[best] = true;
        summary.top_buckets[slot].lower_milliseconds = BucketLowerEdge(best);
        summary.top_buckets[slot].count = buckets_[best];
    }
    return summary;
}

void PresentIntervalHistogram::Reset()
{
    buckets_.fill(0);
    samples_ = 0;
    total_milliseconds_ = 0.0;
    minimum_milliseconds_ = 0.0;
    maximum_milliseconds_ = 0.0;
}

}  // namespace re2dj::graphics
