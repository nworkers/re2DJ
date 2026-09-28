#include "re2dj/graphics/window_policy.h"

#include <optional>

#include "test_support.h"

namespace
{

namespace graphics = re2dj::graphics;

// Scales 1 to 3, starting at 2, as the Windows host has always offered.
void CheckScales(re2dj::test::Context& context)
{
    RE2DJ_CHECK_EQ(context, graphics::kDefaultWindowScale, 2U);
    RE2DJ_CHECK(context, !graphics::IsWindowScale(0));
    RE2DJ_CHECK(context, graphics::IsWindowScale(1));
    RE2DJ_CHECK(context, graphics::IsWindowScale(3));
    RE2DJ_CHECK(context, !graphics::IsWindowScale(4));
}

// The rate appears once at least a second has passed, counts every present
// since the interval began, and then starts again.
void CheckFrameRate(re2dj::test::Context& context)
{
    graphics::FrameRateMeter meter;
    constexpr std::uint64_t kFrequency = 1000;
    RE2DJ_CHECK(context, !meter.Present(5000, kFrequency).has_value());
    for (std::uint64_t frame = 1; frame < 60; ++frame)
    {
        RE2DJ_CHECK(context, !meter.Present(5000 + frame * 16, kFrequency).has_value());
    }
    const std::optional<double> rate = meter.Present(6000, kFrequency);
    RE2DJ_CHECK(context, rate.has_value());
    if (rate.has_value())
    {
        RE2DJ_CHECK_EQ(context, *rate, 61.0);
    }
    RE2DJ_CHECK(context, !meter.Present(6500, kFrequency).has_value());
    const std::optional<double> next = meter.Present(8000, kFrequency);
    RE2DJ_CHECK(context, next.has_value());
    if (next.has_value())
    {
        RE2DJ_CHECK_EQ(context, *next, 1.0);
    }
    RE2DJ_CHECK(context, !meter.Present(9000, 0).has_value());
}

}  // namespace

void RunWindowPolicyTests(re2dj::test::Context& context)
{
    CheckScales(context);
    CheckFrameRate(context);
}
