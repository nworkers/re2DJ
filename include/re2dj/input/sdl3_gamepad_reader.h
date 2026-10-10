#ifndef RE2DJ_INPUT_SDL3_GAMEPAD_READER_H_
#define RE2DJ_INPUT_SDL3_GAMEPAD_READER_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "re2dj/input/gamepad.h"

// The host's gamepads through SDL3's gamepad subsystem (task 444), as the
// GamepadControls the I/O board bindings read. Every pad SDL recognises is
// opened, those plugged in later through the device events the host's event
// pump hands over, and Read merges them all: a pad belongs to nobody, as the
// keyboard does. A stick direction or trigger is held past half its travel.
// SDL itself appears only in the implementation.
namespace re2dj::input
{

class Sdl3GamepadReader
{
public:
    Sdl3GamepadReader();
    ~Sdl3GamepadReader();

    Sdl3GamepadReader(const Sdl3GamepadReader&) = delete;
    Sdl3GamepadReader& operator=(const Sdl3GamepadReader&) = delete;

    // Starts the gamepad subsystem (unless already started by the host) and
    // opens the pads present. False, with error, when SDL refuses; the reader
    // then reads nothing and Shutdown is harmless.
    bool Initialize(std::string* error);
    // Closes the pads and leaves the subsystem the way it was found.
    void Shutdown();

    // Acts on an SDL_Event the host took from the queue: a pad added or
    // removed is opened or closed. True when the event was one of those, so
    // the host can log it; `name` then holds the pad's name, when it has one.
    bool HandleEvent(const void* sdl_event, std::string* name, bool* added);

    // Every open pad's controls, merged; the caller pumps events first so
    // SDL's state is current.
    GamepadControls Read() const;
    // Each open pad's controls on its own, for what must happen on one pad,
    // such as the exit chord (#20).
    std::vector<GamepadControls> ReadEach() const;

    std::size_t open_count() const { return pads_.size(); }
    bool initialized() const { return initialized_; }

private:
    struct Pad
    {
        std::uint32_t instance = 0;
        void* gamepad = nullptr;
    };

    void Open(std::uint32_t instance);
    void Close(std::uint32_t instance);

    std::vector<Pad> pads_;
    bool initialized_ = false;
    bool owns_subsystem_ = false;
};

}  // namespace re2dj::input

#endif  // RE2DJ_INPUT_SDL3_GAMEPAD_READER_H_
