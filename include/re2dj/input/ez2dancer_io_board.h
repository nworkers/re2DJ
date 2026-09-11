#ifndef RE2DJ_INPUT_EZ2DANCER_IO_BOARD_H_
#define RE2DJ_INPUT_EZ2DANCER_IO_BOARD_H_

#include <array>
#include <cstddef>
#include <cstdint>

namespace re2dj::input
{

// The EZ2Dancer cabinet I/O board. This is not the EZ2DJ board: its ports sit
// at 0x300 through 0x30c rather than 0x100 through 0x106, and every access is
// word-wide. Ez2DjIoBoard therefore stays a separate model.
//
// Confirmation status differs per fact and is deliberately not smoothed over.
// The access width and that 0x30a is an output port are **confirmed** from the
// original executable: ez2d2m faults on `66 ef` writing 0x030a. Every port
// meaning and every bit position below is **inferred** from an independent
// public implementation, cross-checked in docs/analysis/ez2dancer-io-map.md.
// None of that code is copied; only the externally observable protocol is
// reimplemented here.

enum class Ez2DancerPlayer : std::uint8_t
{
    kPlayer1,
    kPlayer2,
};

enum class Ez2DancerButton : std::uint8_t
{
    kPlayer1Left,
    kPlayer1Centre,
    kPlayer1Right,
    kPlayer2Left,
    kPlayer2Centre,
    kPlayer2Right,
    kPlayer1SensorTopLeft,
    kPlayer1SensorTopRight,
    kPlayer1SensorBottomLeft,
    kPlayer1SensorBottomRight,
    kPlayer2SensorTopLeft,
    kPlayer2SensorTopRight,
    kPlayer2SensorBottomLeft,
    kPlayer2SensorBottomRight,
    kTest,
    kService,
    kCount,
};

enum class Ez2DancerLight : std::uint8_t
{
    kNeon,
    kLeftTop,
    kLeftMiddle,
    kLeftBottom,
    kRightTop,
    kRightMiddle,
    kRightBottom,
    kCount,
};

class Ez2DancerIoBoard
{
public:
    bool SetButton(Ez2DancerButton button, bool pressed);
    bool ReadPort(std::uint16_t port, std::uint16_t* value) const;
    bool WritePort(std::uint16_t port, std::uint16_t value);
    bool GetLight(Ez2DancerLight light, bool* enabled) const;

private:
    static constexpr std::size_t kButtonCount =
        static_cast<std::size_t>(Ez2DancerButton::kCount);
    static constexpr std::size_t kLightCount =
        static_cast<std::size_t>(Ez2DancerLight::kCount);

    std::array<bool, kButtonCount> buttons_ = {};
    std::array<bool, kLightCount> lights_ = {};
};

}  // namespace re2dj::input

#endif  // RE2DJ_INPUT_EZ2DANCER_IO_BOARD_H_
