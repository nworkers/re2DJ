#ifndef RE2DJ_INPUT_EZ2DJ_IO_BOARD_H_
#define RE2DJ_INPUT_EZ2DJ_IO_BOARD_H_

#include <array>
#include <cstddef>
#include <cstdint>

namespace re2dj::input
{

enum class Ez2DjPlayer : std::uint8_t
{
    kPlayer1,
    kPlayer2,
};

enum class Ez2DjButton : std::uint8_t
{
    kPlayer1Start,
    kPlayer2Start,
    kEffector1,
    kEffector2,
    kEffector3,
    kEffector4,
    kService,
    kTest,
    kCoin,
    kPlayer1Key1,
    kPlayer1Key2,
    kPlayer1Key3,
    kPlayer1Key4,
    kPlayer1Key5,
    kPlayer1Pedal,
    kPlayer2Key1,
    kPlayer2Key2,
    kPlayer2Key3,
    kPlayer2Key4,
    kPlayer2Key5,
    kPlayer2Pedal,
    kCount,
};

enum class Ez2DjLight : std::uint8_t
{
    kRedLeft,
    kRedRight,
    kBlueLeft,
    kBlueRight,
    kNeon,
    kPlayer1Start,
    kPlayer2Start,
    kEffector1,
    kEffector2,
    kEffector3,
    kEffector4,
    kPlayer1Key1,
    kPlayer1Key2,
    kPlayer1Key3,
    kPlayer1Key4,
    kPlayer1Key5,
    kPlayer1Turntable,
    kPlayer2Key1,
    kPlayer2Key2,
    kPlayer2Key3,
    kPlayer2Key4,
    kPlayer2Key5,
    kPlayer2Turntable,
    kCount,
};

class Ez2DjIoBoard
{
public:
    bool SetButton(Ez2DjButton button, bool pressed);
    bool SetTurntable(Ez2DjPlayer player, std::uint8_t position);
    bool ReadPort(std::uint16_t port, std::uint8_t* value);
    bool WritePort(std::uint16_t port, std::uint8_t value);
    bool GetLight(Ez2DjLight light, bool* enabled) const;

private:
    static constexpr std::size_t kButtonCount = static_cast<std::size_t>(Ez2DjButton::kCount);
    static constexpr std::size_t kLightCount = static_cast<std::size_t>(Ez2DjLight::kCount);

    std::array<bool, kButtonCount> buttons_ = {};
    std::array<bool, kLightCount> lights_ = {};
    std::array<std::uint8_t, 2> turntables_ = {0x80, 0x80};
    std::uint8_t coin_counter_ = 0;
};

}  // namespace re2dj::input

#endif  // RE2DJ_INPUT_EZ2DJ_IO_BOARD_H_
