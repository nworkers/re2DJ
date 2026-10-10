#ifndef RE2DJ_GRAPHICS_WINDOW_POLICY_H_
#define RE2DJ_GRAPHICS_WINDOW_POLICY_H_

#include <cstdint>
#include <optional>

// The product window's policy, which both hosts follow: the window shows the
// guest's display at a whole-number scale, twice its size to begin with,
// Alt+1..3 (top row or keypad, not auto-repeated) choose the scale, and a
// left double click switches between the window and monitor-sized borderless
// fullscreen. A scale chosen while in fullscreen applies on leaving it. The
// title shows the frame rate measured over at least a second of presents.
namespace re2dj::graphics
{

inline constexpr std::uint32_t kDefaultWindowScale = 2;
inline constexpr std::uint32_t kMinimumWindowScale = 1;
inline constexpr std::uint32_t kMaximumWindowScale = 3;

constexpr bool IsWindowScale(std::uint32_t scale)
{
    return scale >= kMinimumWindowScale && scale <= kMaximumWindowScale;
}

// Where the display is drawn in a window: the largest rectangle of the
// display's shape that fits, centred, with any rounding going to the bars.
struct PresentRect
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

constexpr PresentRect FitPresentation(int window_width,
                                      int window_height,
                                      std::uint32_t logical_width,
                                      std::uint32_t logical_height)
{
    PresentRect rect;
    rect.width = window_width;
    rect.height = window_height;
    if (logical_width == 0 || logical_height == 0)
    {
        return rect;
    }
    if (static_cast<std::int64_t>(window_width) * logical_height >
        static_cast<std::int64_t>(window_height) * logical_width)
    {
        rect.width = static_cast<int>((static_cast<std::int64_t>(window_height) * logical_width) / logical_height);
    }
    else
    {
        rect.height = static_cast<int>((static_cast<std::int64_t>(window_width) * logical_height) / logical_width);
    }
    rect.x = (window_width - rect.width) / 2;
    rect.y = (window_height - rect.height) / 2;
    return rect;
}

// Where the display is drawn with the keep-aspect choice (#14): the largest
// rectangle of its shape when keeping it, otherwise the whole window, so the
// picture is stretched to fill it.
constexpr PresentRect ComputePresentRect(int window_width,
                                         int window_height,
                                         std::uint32_t logical_width,
                                         std::uint32_t logical_height,
                                         bool keep_aspect)
{
    if (keep_aspect)
    {
        return FitPresentation(window_width, window_height, logical_width, logical_height);
    }
    PresentRect rect;
    rect.width = window_width;
    rect.height = window_height;
    return rect;
}

// Counts presents and, once at least a second has passed since the interval
// began, returns the rate over it and starts the next one.
class FrameRateMeter
{
public:
    std::optional<double> Present(std::uint64_t counter, std::uint64_t frequency)
    {
        if (!started_)
        {
            started_ = true;
            interval_start_ = counter;
        }
        ++frames_;
        const std::uint64_t elapsed = counter - interval_start_;
        if (frequency == 0 || elapsed < frequency)
        {
            return std::nullopt;
        }
        const double rate = static_cast<double>(frames_) * static_cast<double>(frequency) /
                            static_cast<double>(elapsed);
        interval_start_ = counter;
        frames_ = 0;
        return rate;
    }

private:
    bool started_ = false;
    std::uint64_t interval_start_ = 0;
    std::uint64_t frames_ = 0;
};

}  // namespace re2dj::graphics

#endif  // RE2DJ_GRAPHICS_WINDOW_POLICY_H_
