#ifndef RE2DJ_INPUT_EZ2DJ_KEYBOARD_MAP_H_
#define RE2DJ_INPUT_EZ2DJ_KEYBOARD_MAP_H_

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include "re2dj/input/ez2dj_io_board.h"
#include "re2dj/input/legacy_io_port_bus.h"

// How keys stand in for the EZ2DJ I/O board, for both hosts: each button and
// turntable direction has a name (the key an INI file binds it under) and a
// built-in default key, and held turntable keys turn the turntable.
namespace re2dj::input
{

struct Ez2DjButtonBinding
{
    std::string_view name;
    Ez2DjButton button;
    // Written as an INI would write it; config/ez2dj-io.example.ini lists the
    // same keys, which a unit test enforces.
    std::string_view default_key;
};
std::span<const Ez2DjButtonBinding> Ez2DjButtonBindings();

// p1_negative, p1_positive, p2_negative, p2_positive, in that order.
struct Ez2DjTurntableBinding
{
    std::string_view name;
    std::string_view default_key;
};
std::span<const Ez2DjTurntableBinding> Ez2DjTurntableBindings();

inline constexpr std::uint8_t kEz2DjDefaultTurntableStep = 4;

// The turntables' positions: each starts centred at 0x80 and, at most every
// 8 ms, moves by the step toward the held direction key (both or neither
// hold it still), wrapping as the board's 8-bit counter does.
class Ez2DjTurntables
{
public:
    // held: the four turntable keys in Ez2DjTurntableBindings order.
    void Update(LegacyIoPortBus* bus, std::uint64_t now_ms, const std::array<bool, 4>& held, std::uint8_t step);

private:
    std::array<std::uint8_t, 2> positions_ = {0x80, 0x80};
    std::uint64_t last_update_ms_ = 0;
};

}  // namespace re2dj::input

#endif  // RE2DJ_INPUT_EZ2DJ_KEYBOARD_MAP_H_
