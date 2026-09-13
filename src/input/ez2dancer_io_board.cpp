#include "re2dj/input/ez2dancer_io_board.h"

namespace re2dj::input
{

namespace
{

constexpr std::uint16_t kPlayer1Pads = 0x300;
constexpr std::uint16_t kPlayer2Pads = 0x302;
constexpr std::uint16_t kCoinCounter = 0x304;
constexpr std::uint16_t kSensors = 0x306;

constexpr std::uint16_t kPadOutput = 0x308;
constexpr std::uint16_t kCabinetLights = 0x30a;
constexpr std::uint16_t kSensorLeds = 0x30c;

// Each pad reports three zones in the low twelve bits, four bits per zone.
// Inferred, like every bit position in this file.
struct ZoneBit
{
    Ez2DancerButton button;
    std::uint16_t mask;
};

constexpr ZoneBit kPlayer1Zones[] = {
    {Ez2DancerButton::kPlayer1Left, 0x00f},
    {Ez2DancerButton::kPlayer1Centre, 0x0f0},
    {Ez2DancerButton::kPlayer1Right, 0xf00},
};

constexpr ZoneBit kPlayer2Zones[] = {
    {Ez2DancerButton::kPlayer2Left, 0x00f},
    {Ez2DancerButton::kPlayer2Centre, 0x0f0},
    {Ez2DancerButton::kPlayer2Right, 0xf00},
};

// The hand sensors occupy the high byte of the sensor port.
constexpr ZoneBit kSensorBits[] = {
    {Ez2DancerButton::kPlayer2SensorTopRight, 0x0100},
    {Ez2DancerButton::kPlayer2SensorTopLeft, 0x0200},
    {Ez2DancerButton::kPlayer1SensorTopRight, 0x0400},
    {Ez2DancerButton::kPlayer1SensorTopLeft, 0x0800},
    {Ez2DancerButton::kPlayer1SensorBottomLeft, 0x1000},
    {Ez2DancerButton::kPlayer1SensorBottomRight, 0x2000},
    {Ez2DancerButton::kPlayer2SensorBottomLeft, 0x4000},
    {Ez2DancerButton::kPlayer2SensorBottomRight, 0x8000},
};

// The cabinet lights are active low: a zero bit is a lit lamp.
struct LightBit
{
    Ez2DancerLight light;
    std::uint16_t mask;
};

constexpr LightBit kLightBits[] = {
    {Ez2DancerLight::kRightMiddle, 0x0001},
    {Ez2DancerLight::kRightBottom, 0x0002},
    {Ez2DancerLight::kNeon, 0x0004},
    {Ez2DancerLight::kLeftTop, 0x0100},
    {Ez2DancerLight::kLeftMiddle, 0x0200},
    {Ez2DancerLight::kLeftBottom, 0x0400},
    {Ez2DancerLight::kRightTop, 0x0800},
};

constexpr std::uint16_t kTestMask = 1u << 5;
constexpr std::uint16_t kServiceMask = 1u << 4;

}  // namespace

bool Ez2DancerIoBoard::SetButton(Ez2DancerButton button, bool pressed)
{
    const std::size_t index = static_cast<std::size_t>(button);
    if (index >= kButtonCount)
    {
        return false;
    }
    if (button == Ez2DancerButton::kCoin && pressed && !buttons_[index])
    {
        ++coin_counter_;
    }
    buttons_[index] = pressed;
    return true;
}

bool Ez2DancerIoBoard::ReadPort(std::uint16_t port, std::uint16_t* value) const
{
    if (value == nullptr)
    {
        return false;
    }

    const auto held = [this](Ez2DancerButton button) {
        return buttons_[static_cast<std::size_t>(button)];
    };
    // A zone that is stepped on has its four-bit group cleared; the whole word
    // is then inverted, so the guest sees a press as ones. Idle is 0xf000.
    const auto pads = [&held](const ZoneBit (&zones)[3]) {
        std::uint16_t bits = 0x0fff;
        for (const ZoneBit& zone : zones)
        {
            if (held(zone.button))
            {
                bits = static_cast<std::uint16_t>(bits & ~zone.mask);
            }
        }
        return static_cast<std::uint16_t>(~bits);
    };

    switch (port)
    {
        case kPlayer1Pads:
            *value = pads(kPlayer1Zones);
            return true;
        case kPlayer2Pads:
            *value = pads(kPlayer2Zones);
            return true;
        case kCoinCounter:
            // The real register semantics remain unresolved. The compatibility
            // path models coin insertion as a rising-edge counter, matching
            // the way the other EZ board exposes coin events.
            *value = coin_counter_;
            return true;
        case kSensors:
        {
            std::uint16_t bits = 0xffff;
            for (const ZoneBit& sensor : kSensorBits)
            {
                if (held(sensor.button))
                {
                    bits = static_cast<std::uint16_t>(bits & ~sensor.mask);
                }
            }
            if (held(Ez2DancerButton::kTest))
            {
                bits = static_cast<std::uint16_t>(bits & (0xff00 | kTestMask));
            }
            if (held(Ez2DancerButton::kService))
            {
                bits = static_cast<std::uint16_t>(bits & (0xff00 | kServiceMask));
            }
            // The high byte is reported at the opposite polarity from the low
            // byte. Why the board does this is unresolved; the inversion is
            // reproduced because it is what the guest is known to expect.
            *value = static_cast<std::uint16_t>(bits ^ 0xff00);
            return true;
        }
        default:
            return false;
    }
}

bool Ez2DancerIoBoard::WritePort(std::uint16_t port, std::uint16_t value)
{
    switch (port)
    {
        case kCabinetLights:
            for (const LightBit& light : kLightBits)
            {
                lights_[static_cast<std::size_t>(light.light)] =
                    (value & light.mask) == 0;
            }
            return true;
        case kPadOutput:
        case kSensorLeds:
            // Accepted so the guest proceeds, but nothing is known about what
            // either word means, so no state is derived from it.
            return true;
        default:
            return false;
    }
}

bool Ez2DancerIoBoard::GetLight(Ez2DancerLight light, bool* enabled) const
{
    const std::size_t index = static_cast<std::size_t>(light);
    if (enabled == nullptr || index >= kLightCount)
    {
        return false;
    }
    *enabled = lights_[index];
    return true;
}

}  // namespace re2dj::input
