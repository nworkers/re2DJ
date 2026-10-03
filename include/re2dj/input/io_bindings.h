#ifndef RE2DJ_INPUT_IO_BINDINGS_H_
#define RE2DJ_INPUT_IO_BINDINGS_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "re2dj/input/ez2dancer_io_board.h"
#include "re2dj/input/ez2dj_io_board.h"
#include "re2dj/input/ez2dj_keyboard_map.h"

// The I/O board bindings as a run uses them (task 444): every entry resolved
// to a virtual-key code and a gamepad control number, 0 for unbound, from the
// built-in defaults and the --io-config INI's overrides. The INI's sections
// are [buttons] and [turntables] for keys and [gamepad] for controls, under
// the entries' names; an entry the file omits keeps its default, NONE unbinds
// it, an unknown name is an error (task 306). The loaders read INI text with
// the private profile rules both hosts share, so a file reads the same on
// Linux as Windows reads it for its own keyboard input.
namespace re2dj::input
{

struct Ez2DjIoBindings
{
    static constexpr std::size_t kButtonCount = static_cast<std::size_t>(Ez2DjButton::kCount);

    std::array<int, kButtonCount> button_keys = {};
    std::array<int, kButtonCount> button_gamepad = {};
    // In Ez2DjTurntableBindings order.
    std::array<int, 4> turntable_keys = {};
    std::array<int, 4> turntable_gamepad = {};
    std::uint8_t turntable_step = kEz2DjDefaultTurntableStep;
};

struct Ez2DancerIoBindings
{
    static constexpr std::size_t kButtonCount = static_cast<std::size_t>(Ez2DancerButton::kCount);

    std::array<int, kButtonCount> button_keys = {};
    std::array<int, kButtonCount> button_gamepad = {};
};

// Both games' bindings, for a run that learns its board's width later.
struct IoBindings
{
    Ez2DjIoBindings ez2dj;
    Ez2DancerIoBindings ez2dancer;
};

// The built-in defaults alone.
Ez2DjIoBindings DefaultEz2DjIoBindings();
Ez2DancerIoBindings DefaultEz2DancerIoBindings();
IoBindings DefaultIoBindings();

// The defaults with ini_text's entries applied over them; empty text gives
// the defaults. False, with error set and *bindings untouched, for an
// unknown key or control name or a turntables.step outside 1..32.
bool LoadEz2DjIoBindings(std::string_view ini_text, Ez2DjIoBindings* bindings, std::string* error);
bool LoadEz2DancerIoBindings(std::string_view ini_text, Ez2DancerIoBindings* bindings, std::string* error);

}  // namespace re2dj::input

#endif  // RE2DJ_INPUT_IO_BINDINGS_H_
