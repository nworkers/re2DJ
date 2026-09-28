#include "re2dj/graphics/present_pacer.h"

#include <algorithm>

namespace re2dj::graphics
{

PresentPacer::PresentPacer(double refresh_hz, bool enabled) : enabled_(enabled)
{
    const double rate = refresh_hz > 0.0 ? refresh_hz : kDefaultRefreshHz;
    period_ns_ = static_cast<std::uint64_t>(1.0e9 / rate + 0.5);
    intervals_.reserve(kProbeFrames);
}

std::uint64_t PresentPacer::AfterPresent(std::uint64_t now_ns)
{
    if (!enabled_)
    {
        return 0;
    }
    if (!engaged_)
    {
        if (have_previous_ && now_ns >= previous_ns_)
        {
            intervals_.push_back(now_ns - previous_ns_);
        }
        have_previous_ = true;
        previous_ns_ = now_ns;
        if (intervals_.size() < kProbeFrames)
        {
            return 0;
        }
        std::nth_element(intervals_.begin(), intervals_.begin() + kProbeFrames / 2, intervals_.end());
        const std::uint64_t median = intervals_[kProbeFrames / 2];
        intervals_.clear();
        if (median * 4 >= period_ns_ * 3)
        {
            return 0;
        }
        engaged_ = true;
        next_ns_ = 0;
    }
    // A late present waits for nothing and the schedule restarts from it.
    const std::uint64_t target = std::max(now_ns, next_ns_);
    next_ns_ = target + period_ns_;
    return target - now_ns;
}

}  // namespace re2dj::graphics
