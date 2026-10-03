// Drives Sdl3GamepadReader with an SDL virtual joystick: no hardware and no
// display are needed, and the gamepad subsystem alone is started.
#include <cstdio>
#include <string>

#include <SDL3/SDL.h>

#include "re2dj/input/gamepad.h"
#include "re2dj/input/sdl3_gamepad_reader.h"
#include "test_support.h"

namespace
{

namespace input = re2dj::input;

bool Held(const input::GamepadControls& controls, input::GamepadControl control)
{
    return input::GamepadControlHeld(controls, static_cast<int>(control));
}

// Pumps events into the reader, logging what it reports; the count of
// device events it took.
int Pump(input::Sdl3GamepadReader& reader)
{
    int device_events = 0;
    SDL_Event event = {};
    while (SDL_PollEvent(&event))
    {
        std::string name;
        bool added = false;
        if (reader.HandleEvent(&event, &name, &added))
        {
            ++device_events;
            std::printf("%s: %s\n", added ? "added" : "removed", name.c_str());
        }
    }
    return device_events;
}

}  // namespace

int main()
{
    re2dj::test::Context context;

    input::Sdl3GamepadReader reader;
    std::string error;
    if (!reader.Initialize(&error))
    {
        std::fprintf(stderr, "cannot initialize the reader: %s\n", error.c_str());
        return 1;
    }
    RE2DJ_CHECK(context, reader.initialized());
    // Real pads may be connected to the machine running this: their device
    // events are drained, and their count and idle controls are the baseline
    // the virtual pad is checked against.
    Pump(reader);
    const std::size_t baseline_count = reader.open_count();
    const input::GamepadControls baseline = reader.Read();
    std::printf("baseline: %zu pad(s) connected\n", baseline_count);
    // Reads what a virtual pad adds over the baseline.
    const auto read_added = [&] { return reader.Read() & ~baseline; };
    RE2DJ_CHECK(context, read_added().none());

    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    desc.name = "re2DJ virtual gamepad";
    const SDL_JoystickID instance = SDL_AttachVirtualJoystick(&desc);
    if (instance == 0)
    {
        std::fprintf(stderr, "cannot attach a virtual joystick: %s\n", SDL_GetError());
        return 1;
    }
    // The device event opens the pad.
    RE2DJ_CHECK_EQ(context, Pump(reader), 1);
    RE2DJ_CHECK_EQ(context, reader.open_count(), baseline_count + 1);

    SDL_Joystick* const joystick = SDL_GetJoystickFromID(instance);
    RE2DJ_CHECK(context, joystick != nullptr);
    if (joystick == nullptr)
    {
        return 1;
    }

    // Buttons by their SDL gamepad index, which a virtual gamepad maps one
    // to one.
    SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_WEST, true);
    SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_START, true);
    SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_LEFT_PADDLE1, true);
    Pump(reader);
    input::GamepadControls controls = read_added();
    RE2DJ_CHECK(context, Held(controls, input::GamepadControl::kWest));
    RE2DJ_CHECK(context, Held(controls, input::GamepadControl::kStart));
    RE2DJ_CHECK(context, Held(controls, input::GamepadControl::kPaddle2));
    RE2DJ_CHECK(context, !Held(controls, input::GamepadControl::kSouth));
    RE2DJ_CHECK_EQ(context, controls.count(), std::size_t{3});

    SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_WEST, false);
    SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_START, false);
    SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_LEFT_PADDLE1, false);
    Pump(reader);
    RE2DJ_CHECK(context, read_added().none());

    // A stick past half its travel is a direction; short of it is nothing.
    SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_LEFTX, -16384);
    SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_RIGHTY, 16383);
    SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 32767);
    Pump(reader);
    controls = read_added();
    RE2DJ_CHECK(context, Held(controls, input::GamepadControl::kLeftStickLeft));
    RE2DJ_CHECK(context, !Held(controls, input::GamepadControl::kLeftStickRight));
    RE2DJ_CHECK(context, !Held(controls, input::GamepadControl::kRightStickDown));
    RE2DJ_CHECK(context, Held(controls, input::GamepadControl::kRightTrigger));
    RE2DJ_CHECK(context, !Held(controls, input::GamepadControl::kLeftTrigger));
    RE2DJ_CHECK_EQ(context, controls.count(), std::size_t{2});

    SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_LEFTX, 0);
    SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_RIGHTY, 0);
    SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 0);
    Pump(reader);
    RE2DJ_CHECK(context, read_added().none());

    // Removing the pad closes it.
    RE2DJ_CHECK(context, SDL_DetachVirtualJoystick(instance));
    RE2DJ_CHECK_EQ(context, Pump(reader), 1);
    RE2DJ_CHECK_EQ(context, reader.open_count(), baseline_count);
    RE2DJ_CHECK(context, read_added().none());

    reader.Shutdown();
    RE2DJ_CHECK(context, !reader.initialized());
    RE2DJ_CHECK_EQ(context, SDL_WasInit(SDL_INIT_GAMEPAD) & SDL_INIT_GAMEPAD, 0u);

    std::printf("checks: %d, failures: %d\n", context.checks, context.failures);
    return context.failures == 0 ? 0 : 1;
}
