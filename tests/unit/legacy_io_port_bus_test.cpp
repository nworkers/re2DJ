#include "re2dj/input/legacy_io_port_bus.h"

#include <cstdint>

#include "test_support.h"

void RunLegacyIoPortBusTests(re2dj::test::Context& context)
{
    re2dj::input::LegacyIoPortBus bus;
    std::uint8_t value = 0;

    RE2DJ_CHECK(context, !bus.ReadByte(0x100, &value));
    RE2DJ_CHECK(context, bus.ReadByte(0x101, &value));
    RE2DJ_CHECK_EQ(context, value, std::uint8_t{0xff});
    RE2DJ_CHECK(context, bus.ReadByte(0x102, &value));
    RE2DJ_CHECK_EQ(context, value, std::uint8_t{0xff});
    RE2DJ_CHECK(context, bus.ReadByte(0x103, &value));
    RE2DJ_CHECK_EQ(context, value, std::uint8_t{0x80});
    RE2DJ_CHECK(context, bus.ReadByte(0x104, &value));
    RE2DJ_CHECK_EQ(context, value, std::uint8_t{0x80});
    RE2DJ_CHECK(context, bus.ReadByte(0x105, &value));
    RE2DJ_CHECK_EQ(context, value, std::uint8_t{0x00});
    RE2DJ_CHECK(context, bus.ReadByte(0x106, &value));
    RE2DJ_CHECK_EQ(context, value, std::uint8_t{0xff});
    RE2DJ_CHECK(context, !bus.ReadByte(0x107, &value));
    RE2DJ_CHECK(context, !bus.ReadByte(0x101, nullptr));

    RE2DJ_CHECK(context, bus.SetInputByte(0x103, 0x7a));
    RE2DJ_CHECK(context, bus.ReadByte(0x103, &value));
    RE2DJ_CHECK_EQ(context, value, std::uint8_t{0x7a});
    RE2DJ_CHECK(context, bus.ClearInputOverride(0x103));
    RE2DJ_CHECK(context, bus.ReadByte(0x103, &value));
    RE2DJ_CHECK_EQ(context, value, std::uint8_t{0x80});
    RE2DJ_CHECK(context, !bus.SetInputByte(0x100, 1));

    RE2DJ_CHECK(context, !bus.GetLastOutputByte(0x100, &value));
    RE2DJ_CHECK(context, bus.WriteByte(0x100, 0x5a));
    RE2DJ_CHECK(context, bus.GetLastOutputByte(0x100, &value));
    RE2DJ_CHECK_EQ(context, value, std::uint8_t{0x5a});
    RE2DJ_CHECK(context, bus.WriteByte(0x103, 0xa5));
    bool enabled = false;
    RE2DJ_CHECK(context, bus.GetLight(re2dj::input::Ez2DjLight::kPlayer2Key1, &enabled));
    RE2DJ_CHECK(context, enabled);
    RE2DJ_CHECK(context, bus.WriteByte(0x106, 0x01));
    RE2DJ_CHECK(context, !bus.WriteByte(0x104, 0));
    RE2DJ_CHECK(context, !bus.WriteByte(0x105, 0));
    RE2DJ_CHECK(context, !bus.WriteByte(0x107, 0));
}
