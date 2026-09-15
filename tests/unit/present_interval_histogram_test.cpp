#include "re2dj/graphics/present_interval_histogram.h"

#include <cmath>
#include <limits>

#include "test_support.h"

namespace
{

using re2dj::graphics::PresentIntervalHistogram;

}  // namespace

void RunPresentIntervalHistogramTests(re2dj::test::Context& context)
{
    // An empty histogram reports nothing rather than a divide-by-zero mean.
    {
        const PresentIntervalHistogram histogram;
        const auto summary = histogram.Summarize();
        RE2DJ_CHECK(context, summary.samples == 0);
        RE2DJ_CHECK(context, summary.mean_milliseconds == 0.0);
        RE2DJ_CHECK(context, summary.top_buckets[0].count == 0);
    }

    // Mean, minimum and maximum come from the samples themselves, so they are
    // exact; percentiles come from bucket edges and are accurate to the width.
    {
        PresentIntervalHistogram histogram;
        for (int i = 0; i < 90; ++i)
        {
            histogram.Add(16.70);
        }
        for (int i = 0; i < 10; ++i)
        {
            histogram.Add(33.40);
        }
        const auto summary = histogram.Summarize();
        RE2DJ_CHECK(context, summary.samples == 100);
        RE2DJ_CHECK(context, std::fabs(summary.mean_milliseconds - 18.37) < 0.01);
        RE2DJ_CHECK(context, std::fabs(summary.minimum_milliseconds - 16.70) < 0.001);
        RE2DJ_CHECK(context, std::fabs(summary.maximum_milliseconds - 33.40) < 0.001);
        // 16.70 lands in the bucket starting at 16.50; 33.40 in the one at 33.25.
        RE2DJ_CHECK(context, std::fabs(summary.median_milliseconds - 16.50) < 0.001);
        RE2DJ_CHECK(context, std::fabs(summary.percentile95_milliseconds - 33.25) < 0.001);
        // The distribution shape is what the top buckets must show.
        RE2DJ_CHECK(context, summary.top_buckets[0].count == 90);
        RE2DJ_CHECK(context, std::fabs(summary.top_buckets[0].lower_milliseconds - 16.50) < 0.001);
        RE2DJ_CHECK(context, summary.top_buckets[1].count == 10);
        RE2DJ_CHECK(context, std::fabs(summary.top_buckets[1].lower_milliseconds - 33.25) < 0.001);
        RE2DJ_CHECK(context, summary.top_buckets[2].count == 0);
    }

    // A whole-millisecond timer target and a refresh-bound cap must not look
    // alike, which is the entire reason this records a distribution.
    {
        PresentIntervalHistogram timer_paced;
        for (int i = 0; i < 50; ++i)
        {
            timer_paced.Add(17.0);
            timer_paced.Add(18.0);
        }
        const auto summary = timer_paced.Summarize();
        RE2DJ_CHECK(context, std::fabs(summary.top_buckets[0].lower_milliseconds - 17.00) < 0.001);
        RE2DJ_CHECK(context, std::fabs(summary.top_buckets[1].lower_milliseconds - 18.00) < 0.001);
        RE2DJ_CHECK(context, summary.top_buckets[0].count == 50);
        RE2DJ_CHECK(context, summary.top_buckets[1].count == 50);
    }

    // Anything past the covered range collapses into the overflow bucket, whose
    // reported edge is the end of the range.
    {
        PresentIntervalHistogram histogram;
        histogram.Add(1000.0);
        const auto summary = histogram.Summarize();
        RE2DJ_CHECK(context, summary.samples == 1);
        RE2DJ_CHECK(context, std::fabs(summary.maximum_milliseconds - 1000.0) < 0.001);
        RE2DJ_CHECK(context,
                    std::fabs(summary.top_buckets[0].lower_milliseconds -
                              PresentIntervalHistogram::kRangeMilliseconds) < 0.001);
    }

    // A backwards or non-finite clock reading must not become a readable frame.
    {
        PresentIntervalHistogram histogram;
        histogram.Add(-1.0);
        histogram.Add(std::nan(""));
        histogram.Add(std::numeric_limits<double>::infinity());
        RE2DJ_CHECK(context, histogram.samples() == 0);
        histogram.Add(16.7);
        RE2DJ_CHECK(context, histogram.samples() == 1);
    }

    // Reset makes each summary window independent of the one before it.
    {
        PresentIntervalHistogram histogram;
        histogram.Add(16.7);
        histogram.Reset();
        RE2DJ_CHECK(context, histogram.samples() == 0);
        RE2DJ_CHECK(context, histogram.Summarize().mean_milliseconds == 0.0);
    }
}
