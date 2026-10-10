#include "re2dj/input/sdl3_gamepad_reader.h"

#include <SDL3/SDL.h>

#include <algorithm>

namespace re2dj::input
{
namespace
{

// Half of an axis's travel; past it a direction or trigger is held.
constexpr Sint16 kAxisThreshold = 16384;

struct ButtonControl
{
    SDL_GamepadButton button;
    GamepadControl control;
};

constexpr ButtonControl kButtons[] = {
    {SDL_GAMEPAD_BUTTON_SOUTH, GamepadControl::kSouth},
    {SDL_GAMEPAD_BUTTON_EAST, GamepadControl::kEast},
    {SDL_GAMEPAD_BUTTON_WEST, GamepadControl::kWest},
    {SDL_GAMEPAD_BUTTON_NORTH, GamepadControl::kNorth},
    {SDL_GAMEPAD_BUTTON_BACK, GamepadControl::kBack},
    {SDL_GAMEPAD_BUTTON_GUIDE, GamepadControl::kGuide},
    {SDL_GAMEPAD_BUTTON_START, GamepadControl::kStart},
    {SDL_GAMEPAD_BUTTON_LEFT_STICK, GamepadControl::kLeftStick},
    {SDL_GAMEPAD_BUTTON_RIGHT_STICK, GamepadControl::kRightStick},
    {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, GamepadControl::kLeftShoulder},
    {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, GamepadControl::kRightShoulder},
    {SDL_GAMEPAD_BUTTON_DPAD_UP, GamepadControl::kDpadUp},
    {SDL_GAMEPAD_BUTTON_DPAD_DOWN, GamepadControl::kDpadDown},
    {SDL_GAMEPAD_BUTTON_DPAD_LEFT, GamepadControl::kDpadLeft},
    {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, GamepadControl::kDpadRight},
    {SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1, GamepadControl::kPaddle1},
    {SDL_GAMEPAD_BUTTON_LEFT_PADDLE1, GamepadControl::kPaddle2},
    {SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2, GamepadControl::kPaddle3},
    {SDL_GAMEPAD_BUTTON_LEFT_PADDLE2, GamepadControl::kPaddle4},
};

// An axis's negative and positive directions, kNone for a trigger's unused
// negative side.
struct AxisControl
{
    SDL_GamepadAxis axis;
    GamepadControl negative;
    GamepadControl positive;
};

constexpr AxisControl kAxes[] = {
    {SDL_GAMEPAD_AXIS_LEFTX, GamepadControl::kLeftStickLeft, GamepadControl::kLeftStickRight},
    {SDL_GAMEPAD_AXIS_LEFTY, GamepadControl::kLeftStickUp, GamepadControl::kLeftStickDown},
    {SDL_GAMEPAD_AXIS_RIGHTX, GamepadControl::kRightStickLeft, GamepadControl::kRightStickRight},
    {SDL_GAMEPAD_AXIS_RIGHTY, GamepadControl::kRightStickUp, GamepadControl::kRightStickDown},
    {SDL_GAMEPAD_AXIS_LEFT_TRIGGER, GamepadControl::kNone, GamepadControl::kLeftTrigger},
    {SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, GamepadControl::kNone, GamepadControl::kRightTrigger},
};

void Hold(GamepadControls* controls, GamepadControl control)
{
    if (control != GamepadControl::kNone)
    {
        controls->set(static_cast<std::size_t>(control));
    }
}

}  // namespace

Sdl3GamepadReader::Sdl3GamepadReader() = default;

Sdl3GamepadReader::~Sdl3GamepadReader()
{
    Shutdown();
}

bool Sdl3GamepadReader::Initialize(std::string* error)
{
    if (error == nullptr)
    {
        return false;
    }
    if (initialized_)
    {
        error->clear();
        return true;
    }
    owns_subsystem_ = (SDL_WasInit(SDL_INIT_GAMEPAD) & SDL_INIT_GAMEPAD) == 0;
    if (owns_subsystem_ && !SDL_InitSubSystem(SDL_INIT_GAMEPAD))
    {
        owns_subsystem_ = false;
        *error = std::string("cannot initialize SDL3 gamepads: ") + SDL_GetError();
        return false;
    }
    initialized_ = true;
    int count = 0;
    SDL_JoystickID* const ids = SDL_GetGamepads(&count);
    if (ids != nullptr)
    {
        for (int index = 0; index < count; ++index)
        {
            Open(ids[index]);
        }
        SDL_free(ids);
    }
    error->clear();
    return true;
}

void Sdl3GamepadReader::Shutdown()
{
    if (!initialized_)
    {
        return;
    }
    for (const Pad& pad : pads_)
    {
        SDL_CloseGamepad(static_cast<SDL_Gamepad*>(pad.gamepad));
    }
    pads_.clear();
    if (owns_subsystem_)
    {
        SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
        owns_subsystem_ = false;
    }
    initialized_ = false;
}

void Sdl3GamepadReader::Open(std::uint32_t instance)
{
    const auto known = std::find_if(pads_.begin(), pads_.end(),
                                    [instance](const Pad& pad) { return pad.instance == instance; });
    if (known != pads_.end())
    {
        return;
    }
    SDL_Gamepad* const gamepad = SDL_OpenGamepad(instance);
    if (gamepad == nullptr)
    {
        return;
    }
    pads_.push_back({instance, gamepad});
}

void Sdl3GamepadReader::Close(std::uint32_t instance)
{
    const auto known = std::find_if(pads_.begin(), pads_.end(),
                                    [instance](const Pad& pad) { return pad.instance == instance; });
    if (known == pads_.end())
    {
        return;
    }
    SDL_CloseGamepad(static_cast<SDL_Gamepad*>(known->gamepad));
    pads_.erase(known);
}

bool Sdl3GamepadReader::HandleEvent(const void* sdl_event, std::string* name, bool* added)
{
    if (!initialized_ || sdl_event == nullptr || name == nullptr || added == nullptr)
    {
        return false;
    }
    const auto* event = static_cast<const SDL_Event*>(sdl_event);
    switch (event->type)
    {
    case SDL_EVENT_GAMEPAD_ADDED:
    {
        const SDL_JoystickID instance = event->gdevice.which;
        const char* const found = SDL_GetGamepadNameForID(instance);
        *name = found == nullptr ? "" : found;
        *added = true;
        Open(instance);
        return true;
    }
    case SDL_EVENT_GAMEPAD_REMOVED:
    {
        const SDL_JoystickID instance = event->gdevice.which;
        const auto known = std::find_if(pads_.begin(), pads_.end(),
                                        [instance](const Pad& pad) { return pad.instance == instance; });
        const char* found = nullptr;
        if (known != pads_.end())
        {
            found = SDL_GetGamepadName(static_cast<SDL_Gamepad*>(known->gamepad));
        }
        *name = found == nullptr ? "" : found;
        *added = false;
        Close(instance);
        return true;
    }
    default:
        return false;
    }
}

GamepadControls Sdl3GamepadReader::Read() const
{
    GamepadControls controls;
    for (const GamepadControls& pad : ReadEach())
    {
        controls |= pad;
    }
    return controls;
}

std::vector<GamepadControls> Sdl3GamepadReader::ReadEach() const
{
    std::vector<GamepadControls> pads;
    pads.reserve(pads_.size());
    for (const Pad& pad : pads_)
    {
        GamepadControls controls;
        auto* const gamepad = static_cast<SDL_Gamepad*>(pad.gamepad);
        for (const ButtonControl& entry : kButtons)
        {
            if (SDL_GetGamepadButton(gamepad, entry.button))
            {
                Hold(&controls, entry.control);
            }
        }
        for (const AxisControl& entry : kAxes)
        {
            const Sint16 value = SDL_GetGamepadAxis(gamepad, entry.axis);
            if (value >= kAxisThreshold)
            {
                Hold(&controls, entry.positive);
            }
            else if (value <= -kAxisThreshold)
            {
                Hold(&controls, entry.negative);
            }
        }
        pads.push_back(controls);
    }
    return pads;
}

}  // namespace re2dj::input
