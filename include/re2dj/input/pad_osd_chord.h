#ifndef RE2DJ_INPUT_PAD_OSD_CHORD_H_
#define RE2DJ_INPUT_PAD_OSD_CHORD_H_

#include <vector>

#include "re2dj/input/gamepad.h"

// Opening the OSD from a pad (#22, as rePIU #55 has it), for a Steam Deck with
// no backtick key, and keeping the pad away from the game while it is open.
namespace re2dj::input
{

// True when one pad holds both triggers (past half their travel) and Y
// (North). It shares neither Y nor the stick clicks with the exit chord.
inline bool IsPadOsdChordDown(const GamepadControls& pad)
{
    return GamepadControlHeld(pad, static_cast<int>(GamepadControl::kLeftTrigger)) &&
           GamepadControlHeld(pad, static_cast<int>(GamepadControl::kRightTrigger)) &&
           GamepadControlHeld(pad, static_cast<int>(GamepadControl::kNorth));
}

// True when any one pad holds the chord; split across two pads does not count.
inline bool IsPadOsdChordDown(const std::vector<GamepadControls>& pads)
{
    for (const GamepadControls& pad : pads)
    {
        if (IsPadOsdChordDown(pad))
        {
            return true;
        }
    }
    return false;
}

// True once, on the update at which `down` turns true.
class PadChordEdge
{
public:
    bool Update(bool down)
    {
        const bool rising = down && !down_;
        down_ = down;
        return rising;
    }

private:
    bool down_ = false;
};

// What the game may see of the pads. While the OSD is open the pad drives the
// OSD, so the game sees none of it; when it closes, a control still held (the
// B that closed it, the chord's triggers and Y) stays hidden until released,
// so closing the OSD never reaches the game as input.
class PadGameGate
{
public:
    // `held` is what the pads hold now.
    void SetSuppressed(bool suppressed, const GamepadControls& held)
    {
        if (suppressed_ && !suppressed)
        {
            latched_ = held;
        }
        suppressed_ = suppressed;
    }

    bool suppressed() const { return suppressed_; }

    // The state the game sees for `held`.
    GamepadControls Apply(const GamepadControls& held)
    {
        if (suppressed_)
        {
            return GamepadControls{};
        }
        // A latched control that has been let go is free again.
        latched_ &= held;
        return held & ~latched_;
    }

private:
    bool suppressed_ = false;
    GamepadControls latched_;
};

}  // namespace re2dj::input

#endif  // RE2DJ_INPUT_PAD_OSD_CHORD_H_
