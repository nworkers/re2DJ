#ifndef RE2DJ_INPUT_PAD_EXIT_CHORD_H_
#define RE2DJ_INPUT_PAD_EXIT_CHORD_H_

#include <cstdint>
#include <vector>

#include "re2dj/input/gamepad.h"

// The gamepad way to quit (#20, as rePIU #52 has it), for a Steam Deck with no
// keyboard or close button: both triggers and both stick clicks held together
// on one pad for a second. Four controls no song uses at once, held that long,
// cannot happen by accident in play. The controls still reach the game as
// usual while they are held.
namespace re2dj::input
{

// True when one pad holds both triggers (past half their travel) and both
// stick clicks.
inline bool IsPadExitChordDown(const GamepadControls& pad)
{
    return GamepadControlHeld(pad, static_cast<int>(GamepadControl::kLeftTrigger)) &&
           GamepadControlHeld(pad, static_cast<int>(GamepadControl::kRightTrigger)) &&
           GamepadControlHeld(pad, static_cast<int>(GamepadControl::kLeftStick)) &&
           GamepadControlHeld(pad, static_cast<int>(GamepadControl::kRightStick));
}

// True when any one pad holds the chord; the four split across two pads do
// not count.
inline bool IsPadExitChordDown(const std::vector<GamepadControls>& pads)
{
    for (const GamepadControls& pad : pads)
    {
        if (IsPadExitChordDown(pad))
        {
            return true;
        }
    }
    return false;
}

// Fires once when the chord has been held for kHoldMilliseconds without a
// break, and not again until it is released.
class PadExitChordTimer
{
public:
    static constexpr std::uint64_t kHoldMilliseconds = 1000;

    // now_ms is any monotonic millisecond clock. True on the one update at
    // which the hold reaches a second.
    bool Update(bool down, std::uint64_t now_ms)
    {
        if (!down)
        {
            down_ = false;
            fired_ = false;
            return false;
        }
        if (!down_)
        {
            down_ = true;
            since_ms_ = now_ms;
        }
        if (fired_ || now_ms - since_ms_ < kHoldMilliseconds)
        {
            return false;
        }
        fired_ = true;
        return true;
    }

private:
    bool down_ = false;
    bool fired_ = false;
    std::uint64_t since_ms_ = 0;
};

}  // namespace re2dj::input

#endif  // RE2DJ_INPUT_PAD_EXIT_CHORD_H_
