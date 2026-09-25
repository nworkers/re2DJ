#include "re2dj/directx/directdraw_display.h"

#include <cstdint>

#include "test_support.h"

namespace
{

namespace dx = re2dj::directx;

// SetCooperativeLevel records the window and flags once the host's policy
// accepts; no window or a refusing host leaves the display as it was.
void CheckCooperativeLevel(re2dj::test::Context& context)
{
    dx::DirectDrawDisplay display;
    std::uint32_t policy_calls = 0;
    const auto accept = [&policy_calls](std::uint32_t, const dx::DisplayMode& mode) {
        ++policy_calls;
        return mode.width == 640;
    };
    RE2DJ_CHECK_EQ(context, dx::SetCooperativeLevel(&display, 0, 0x813, accept), dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, policy_calls, 0U);
    RE2DJ_CHECK_EQ(context, dx::SetCooperativeLevel(&display, 0x10014, 0x813, accept), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, display.window, 0x10014U);
    RE2DJ_CHECK_EQ(context, display.cooperative_flags, 0x813U);
    const auto refuse = [](std::uint32_t, const dx::DisplayMode&) { return false; };
    RE2DJ_CHECK_EQ(context, dx::SetCooperativeLevel(&display, 0x10020, 0x8, refuse), dx::kDdErrGeneric);
    RE2DJ_CHECK_EQ(context, display.window, 0x10014U);
    RE2DJ_CHECK_EQ(context, display.cooperative_flags, 0x813U);
    // A host without a policy of its own accepts.
    RE2DJ_CHECK_EQ(context, dx::SetCooperativeLevel(&display, 0x10020, 0x8, {}), dx::kDdOk);
}

// Only 640x480x16 is accepted; other modes, even enumerated ones, are
// DDERR_UNSUPPORTEDMODE and leave the mode unchanged.
void CheckDisplayMode(re2dj::test::Context& context)
{
    dx::DirectDrawDisplay display;
    RE2DJ_CHECK_EQ(context, display.mode.width, 640U);
    RE2DJ_CHECK_EQ(context, dx::SetDisplayMode(&display, {800, 600, 16}), dx::kDdErrUnsupportedMode);
    RE2DJ_CHECK_EQ(context, dx::SetDisplayMode(&display, {640, 480, 32}), dx::kDdErrUnsupportedMode);
    RE2DJ_CHECK_EQ(context, display.mode.bits_per_pixel, 16U);
    RE2DJ_CHECK_EQ(context, dx::SetDisplayMode(&display, {640, 480, 16}), dx::kDdOk);
    RE2DJ_CHECK(context, dx::IsSupportedDisplayMode(dx::kDefaultDisplayMode));
}

}  // namespace

void RunDirectXDisplayTests(re2dj::test::Context& context)
{
    CheckCooperativeLevel(context);
    CheckDisplayMode(context);
}
