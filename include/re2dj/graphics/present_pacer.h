#ifndef RE2DJ_GRAPHICS_PRESENT_PACER_H_
#define RE2DJ_GRAPHICS_PRESENT_PACER_H_

#include <cstddef>
#include <cstdint>
#include <vector>

namespace re2dj::graphics
{

// Stands in for vertical sync on a host whose swap does not block, such as
// WSLg, where a swap interval of 1 is granted but presents return at once.
// A guest that paces itself by its presents (EZ2DJ 1st SE flips once per
// frame and counts frames) would then run faster than the refresh rate.
//
// The pacer watches the spacing of presents. Once a window of kProbeFrames
// intervals has a median below three quarters of the refresh period, the
// swap is taken not to block and the pacer engages for the rest of the run:
// each present then returns no earlier than one period after the previous
// one. A late present waits for nothing and the schedule restarts from it,
// so a slow frame is never followed by a burst of catching up.
// On a host whose swap does block, the spacing stays at the period and the
// pacer never engages; a guest slower than the refresh rate never engages
// it either.
class PresentPacer
{
public:
    static constexpr std::size_t kProbeFrames = 60;
    static constexpr double kDefaultRefreshHz = 60.0;

    // A refresh rate of 0 or less (unknown) uses kDefaultRefreshHz; enabled
    // false (a policy that asks presents not to block) never engages.
    PresentPacer(double refresh_hz = kDefaultRefreshHz, bool enabled = true);

    // Called as each present returns, with a monotonic clock in nanoseconds.
    // Returns how long to wait before handing control back to the guest.
    std::uint64_t AfterPresent(std::uint64_t now_ns);

    bool engaged() const { return engaged_; }
    std::uint64_t period_ns() const { return period_ns_; }

private:
    std::uint64_t period_ns_ = 0;
    bool enabled_ = true;
    bool engaged_ = false;
    bool have_previous_ = false;
    std::uint64_t previous_ns_ = 0;
    std::vector<std::uint64_t> intervals_;
    // The earliest time the next present may return, once engaged.
    std::uint64_t next_ns_ = 0;
};

}  // namespace re2dj::graphics

#endif  // RE2DJ_GRAPHICS_PRESENT_PACER_H_
