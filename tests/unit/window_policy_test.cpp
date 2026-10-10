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

// Keeping the aspect is the 4:3 fit with bars; not keeping it fills the
// window whatever its shape (#14).
void CheckPresentRect(re2dj::test::Context& context)
{
    const graphics::PresentRect kept = graphics::ComputePresentRect(1920, 1080, 640, 480, true);
    const graphics::PresentRect fit = graphics::FitPresentation(1920, 1080, 640, 480);
    RE2DJ_CHECK_EQ(context, kept.x, fit.x);
    RE2DJ_CHECK_EQ(context, kept.width, fit.width);
    RE2DJ_CHECK_EQ(context, kept.x, 240);
    RE2DJ_CHECK_EQ(context, kept.width, 1440);
    RE2DJ_CHECK_EQ(context, kept.height, 1080);

    const graphics::PresentRect stretched = graphics::ComputePresentRect(1920, 1080, 640, 480, false);
    RE2DJ_CHECK_EQ(context, stretched.x, 0);
    RE2DJ_CHECK_EQ(context, stretched.y, 0);
    RE2DJ_CHECK_EQ(context, stretched.width, 1920);
    RE2DJ_CHECK_EQ(context, stretched.height, 1080);

    const graphics::PresentRect tall = graphics::ComputePresentRect(640, 1000, 640, 480, true);
    RE2DJ_CHECK_EQ(context, tall.y, 260);
    RE2DJ_CHECK_EQ(context, tall.height, 480);
    RE2DJ_CHECK_EQ(context, graphics::ComputePresentRect(640, 1000, 640, 480, false).height, 1000);
}

}  // namespace

void RunWindowPolicyTests(re2dj::test::Context& context)
{
    CheckScales(context);
    CheckFrameRate(context);
    CheckPresentRect(context);
}
