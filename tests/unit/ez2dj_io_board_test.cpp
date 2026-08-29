#include "re2dj/input/ez2dj_io_board.h"

#include <cstdint>

#include "test_support.h"

void RunEz2DjIoBoardTests(re2dj::test::Context& context)
{
    using re2dj::input::Ez2DjButton;
    using re2dj::input::Ez2DjIoBoard;
    using re2dj::input::Ez2DjLight;
    using re2dj::input::Ez2DjPlayer;

    Ez2DjIoBoard board;
    std::uint8_t value = 0;
    RE2DJ_CHECK(context, board.ReadPort(0x101, &value));
    RE2DJ_CHECK_EQ(context, value, std::uint8_t{0xff});
    RE2DJ_CHECK(context, board.ReadPort(0x103, &value));
    RE2DJ_CHECK_EQ(context, value, std::uint8_t{0x80});
    RE2DJ_CHECK(context, board.ReadPort(0x104, &value));
    RE2DJ_CHECK_EQ(context, value, std::uint8_t{0x80});
    RE2DJ_CHECK(context, board.ReadPort(0x105, &value));
    RE2DJ_CHECK_EQ(context, value, std::uint8_t{0x00});

    RE2DJ_CHECK(context, board.SetButton(Ez2DjButton::kPlayer1Start, true));
    RE2DJ_CHECK(context, board.SetButton(Ez2DjButton::kEffector3, true));
    RE2DJ_CHECK(context, board.SetButton(Ez2DjButton::kTest, true));
    RE2DJ_CHECK(context, board.ReadPort(0x101, &value));
    RE2DJ_CHECK_EQ(context, value, std::uint8_t{0x6e});

    RE2DJ_CHECK(context, board.SetButton(Ez2DjButton::kPlayer1Key2, true));
    RE2DJ_CHECK(context, board.SetButton(Ez2DjButton::kPlayer1Pedal, true));
    RE2DJ_CHECK(context, board.ReadPort(0x102, &value));
    RE2DJ_CHECK_EQ(context, value, std::uint8_t{0x7d});

    RE2DJ_CHECK(context, board.SetButton(Ez2DjButton::kPlayer2Key5, true));
    RE2DJ_CHECK(context, board.SetButton(Ez2DjButton::kPlayer2Pedal, true));
    RE2DJ_CHECK(context, board.ReadPort(0x106, &value));
    RE2DJ_CHECK_EQ(context, value, std::uint8_t{0x6f});

    RE2DJ_CHECK(context, board.SetTurntable(Ez2DjPlayer::kPlayer1, 0xfc));
    RE2DJ_CHECK(context, board.SetTurntable(Ez2DjPlayer::kPlayer2, 0x03));
    RE2DJ_CHECK(context, board.ReadPort(0x103, &value));
    RE2DJ_CHECK_EQ(context, value, std::uint8_t{0xfc});
    RE2DJ_CHECK(context, board.ReadPort(0x104, &value));
    RE2DJ_CHECK_EQ(context, value, std::uint8_t{0x03});

    RE2DJ_CHECK(context, board.SetButton(Ez2DjButton::kCoin, true));
    RE2DJ_CHECK(context, board.ReadPort(0x105, &value));
    RE2DJ_CHECK_EQ(context, value, std::uint8_t{0x01});
    RE2DJ_CHECK(context, board.ReadPort(0x105, &value));
    RE2DJ_CHECK_EQ(context, value, std::uint8_t{0x01});
    RE2DJ_CHECK(context, board.SetButton(Ez2DjButton::kCoin, true));
    RE2DJ_CHECK(context, board.ReadPort(0x105, &value));
    RE2DJ_CHECK_EQ(context, value, std::uint8_t{0x01});
    RE2DJ_CHECK(context, board.SetButton(Ez2DjButton::kCoin, false));
    RE2DJ_CHECK(context, board.ReadPort(0x105, &value));
    RE2DJ_CHECK_EQ(context, value, std::uint8_t{0x01});
    RE2DJ_CHECK(context, board.SetButton(Ez2DjButton::kCoin, true));
    RE2DJ_CHECK(context, board.ReadPort(0x105, &value));
    RE2DJ_CHECK_EQ(context, value, std::uint8_t{0x02});
    for (int press = 0; press < 254; ++press)
    {
        RE2DJ_CHECK(context, board.SetButton(Ez2DjButton::kCoin, false));
        RE2DJ_CHECK(context, board.SetButton(Ez2DjButton::kCoin, true));
    }
    RE2DJ_CHECK(context, board.ReadPort(0x105, &value));
    RE2DJ_CHECK_EQ(context, value, std::uint8_t{0x00});

    bool enabled = false;
    RE2DJ_CHECK(context, board.WritePort(0x100, 0x11));
    RE2DJ_CHECK(context, board.GetLight(Ez2DjLight::kRedLeft, &enabled));
    RE2DJ_CHECK(context, enabled);
    RE2DJ_CHECK(context, board.GetLight(Ez2DjLight::kNeon, &enabled));
    RE2DJ_CHECK(context, enabled);
    RE2DJ_CHECK(context, board.WritePort(0x103, 0x20));
    RE2DJ_CHECK(context, board.GetLight(Ez2DjLight::kPlayer2Turntable, &enabled));
    RE2DJ_CHECK(context, enabled);
    RE2DJ_CHECK(context, !board.WritePort(0x106, 0));
}
