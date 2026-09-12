#include "re2dj/graphics/presentation_filter.h"

#include "test_support.h"

void RunPresentationFilterTests(re2dj::test::Context& context)
{
    using re2dj::graphics::PresentationFilter;
    using re2dj::graphics::SelectPresentationFilter;

    RE2DJ_CHECK_EQ(context,
                   SelectPresentationFilter(640, 480, 640, 480),
                   PresentationFilter::kNearest);
    RE2DJ_CHECK_EQ(context,
                   SelectPresentationFilter(640, 480, 1280, 960),
                   PresentationFilter::kNearest);
    RE2DJ_CHECK_EQ(context,
                   SelectPresentationFilter(640, 480, 1920, 1440),
                   PresentationFilter::kNearest);
    RE2DJ_CHECK_EQ(context,
                   SelectPresentationFilter(640, 480, 2560, 1920),
                   PresentationFilter::kNearest);

    RE2DJ_CHECK_EQ(context,
                   SelectPresentationFilter(640, 480, 1280, 720),
                   PresentationFilter::kLinear);
    RE2DJ_CHECK_EQ(context,
                   SelectPresentationFilter(640, 480, 1920, 1080),
                   PresentationFilter::kLinear);
    RE2DJ_CHECK_EQ(context,
                   SelectPresentationFilter(640, 480, 960, 720),
                   PresentationFilter::kLinear);
    RE2DJ_CHECK_EQ(context,
                   SelectPresentationFilter(640, 480, 1280, 1440),
                   PresentationFilter::kLinear);
    RE2DJ_CHECK_EQ(context,
                   SelectPresentationFilter(0, 480, 1280, 960),
                   PresentationFilter::kLinear);
}
