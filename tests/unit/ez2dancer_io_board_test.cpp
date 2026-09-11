#include "re2dj/input/ez2dancer_io_port_bus.h"

#include <cstdint>

#include "test_support.h"

namespace
{

using re2dj::input::Ez2DancerButton;
using re2dj::input::Ez2DancerIoPortBus;
using re2dj::input::Ez2DancerLight;

std::uint16_t ReadPort(re2dj::test::Context& context,
                       Ez2DancerIoPortBus& bus,
                       std::uint16_t port)
{
    std::uint16_t value = 0;
    RE2DJ_CHECK(context, bus.ReadWord(port, &value));
    return value;
}

}  // namespace

void RunEz2DancerIoBoardTests(re2dj::test::Context& context)
{
    // Idle values. These are the board's resting state, so a guest that reads
    // before anything is pressed must see exactly these.
    {
        Ez2DancerIoPortBus bus;
        RE2DJ_CHECK_EQ(context, ReadPort(context, bus, 0x300), std::uint16_t{0xf000});
        RE2DJ_CHECK_EQ(context, ReadPort(context, bus, 0x302), std::uint16_t{0xf000});
        RE2DJ_CHECK_EQ(context, ReadPort(context, bus, 0x304), std::uint16_t{0x0000});
        RE2DJ_CHECK_EQ(context, ReadPort(context, bus, 0x306), std::uint16_t{0x00ff});
    }

    // Only the board's own even ports answer. An odd address inside the range
    // and anything outside it must be refused rather than answered with zero,
    // so an unexpected access is reported instead of silently satisfied.
    {
        Ez2DancerIoPortBus bus;
        std::uint16_t value = 0;
        RE2DJ_CHECK(context, !bus.ReadWord(0x301, &value));
        RE2DJ_CHECK(context, !bus.ReadWord(0x2fe, &value));
        RE2DJ_CHECK(context, !bus.ReadWord(0x30e, &value));
        RE2DJ_CHECK(context, !bus.ReadWord(0x300, nullptr));
        // The EZ2DJ byte board's range is a different board entirely.
        RE2DJ_CHECK(context, !bus.ReadWord(0x101, &value));
    }

    // A pad zone clears its own four-bit group and the word is inverted, so a
    // press reads as ones in that group and nothing else moves.
    {
        Ez2DancerIoPortBus bus;
        RE2DJ_CHECK(context, bus.SetButton(Ez2DancerButton::kPlayer1Left, true));
        RE2DJ_CHECK_EQ(context, ReadPort(context, bus, 0x300), std::uint16_t{0xf00f});
        RE2DJ_CHECK_EQ(context, ReadPort(context, bus, 0x302), std::uint16_t{0xf000});
        RE2DJ_CHECK(context, bus.SetButton(Ez2DancerButton::kPlayer1Right, true));
        RE2DJ_CHECK_EQ(context, ReadPort(context, bus, 0x300), std::uint16_t{0xff0f});
        RE2DJ_CHECK(context, bus.SetButton(Ez2DancerButton::kPlayer1Left, false));
        RE2DJ_CHECK_EQ(context, ReadPort(context, bus, 0x300), std::uint16_t{0xff00});
        // Player 2 is a separate port with the same zone layout.
        RE2DJ_CHECK(context, bus.SetButton(Ez2DancerButton::kPlayer2Centre, true));
        RE2DJ_CHECK_EQ(context, ReadPort(context, bus, 0x302), std::uint16_t{0xf0f0});
    }

    // The sensor port reports its high byte at the opposite polarity from its
    // low byte, so a held sensor reads as a one there while TEST and SERVICE
    // read as zeros below.
    {
        Ez2DancerIoPortBus bus;
        RE2DJ_CHECK(context,
                    bus.SetButton(Ez2DancerButton::kPlayer1SensorTopLeft, true));
        RE2DJ_CHECK_EQ(context, ReadPort(context, bus, 0x306), std::uint16_t{0x08ff});
        RE2DJ_CHECK(context,
                    bus.SetButton(Ez2DancerButton::kPlayer1SensorTopLeft, false));
        RE2DJ_CHECK(context, bus.SetButton(Ez2DancerButton::kTest, true));
        RE2DJ_CHECK_EQ(context, ReadPort(context, bus, 0x306), std::uint16_t{0x0020});
        RE2DJ_CHECK(context, bus.SetButton(Ez2DancerButton::kTest, false));
        RE2DJ_CHECK(context, bus.SetButton(Ez2DancerButton::kService, true));
        RE2DJ_CHECK_EQ(context, ReadPort(context, bus, 0x306), std::uint16_t{0x0010});
    }

    // The cabinet lights are active low, and 0x0004 is the value the original
    // executable was observed writing to port 0x30a.
    {
        Ez2DancerIoPortBus bus;
        bool lit = true;
        RE2DJ_CHECK(context, bus.GetLight(Ez2DancerLight::kNeon, &lit));
        RE2DJ_CHECK(context, !lit);
        RE2DJ_CHECK(context, bus.WriteWord(0x30a, 0x0004));
        RE2DJ_CHECK(context, bus.GetLight(Ez2DancerLight::kNeon, &lit));
        RE2DJ_CHECK(context, !lit);
        RE2DJ_CHECK(context, bus.GetLight(Ez2DancerLight::kLeftTop, &lit));
        RE2DJ_CHECK(context, lit);
        // Every lamp lit is all zeros.
        RE2DJ_CHECK(context, bus.WriteWord(0x30a, 0x0000));
        RE2DJ_CHECK(context, bus.GetLight(Ez2DancerLight::kNeon, &lit));
        RE2DJ_CHECK(context, lit);
        RE2DJ_CHECK(context, bus.GetLight(Ez2DancerLight::kRightBottom, &lit));
        RE2DJ_CHECK(context, lit);

        std::uint16_t last = 0;
        RE2DJ_CHECK(context, bus.GetLastOutputWord(0x30a, &last));
        RE2DJ_CHECK_EQ(context, last, std::uint16_t{0x0000});
    }

    // The two output ports whose meaning is unknown are accepted so the guest
    // proceeds, but a port outside the board is still refused.
    {
        Ez2DancerIoPortBus bus;
        RE2DJ_CHECK(context, bus.WriteWord(0x308, 0x1234));
        RE2DJ_CHECK(context, bus.WriteWord(0x30c, 0x5678));
        RE2DJ_CHECK(context, !bus.WriteWord(0x30e, 0x0000));
        RE2DJ_CHECK(context, !bus.WriteWord(0x100, 0x0000));
        std::uint16_t last = 0;
        RE2DJ_CHECK(context, bus.GetLastOutputWord(0x308, &last));
        RE2DJ_CHECK_EQ(context, last, std::uint16_t{0x1234});
        // An input port was never written, so it reports no last output.
        RE2DJ_CHECK(context, !bus.GetLastOutputWord(0x300, &last));
    }

    // An override replaces what the board would report, and clearing it puts
    // the board back in charge.
    {
        Ez2DancerIoPortBus bus;
        RE2DJ_CHECK(context, bus.SetInputWord(0x306, 0xdead));
        RE2DJ_CHECK_EQ(context, ReadPort(context, bus, 0x306), std::uint16_t{0xdead});
        RE2DJ_CHECK(context, bus.ClearInputOverride(0x306));
        RE2DJ_CHECK_EQ(context, ReadPort(context, bus, 0x306), std::uint16_t{0x00ff});
        RE2DJ_CHECK(context, !bus.SetInputWord(0x301, 0));
    }
}
