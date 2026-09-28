#include "re2dj/input/ez2dj_keyboard_map.h"

#include <array>
#include <cstdint>
#include <fstream>
#include <map>
#include <string>

#include "re2dj/input/legacy_io_port_bus.h"
#include "re2dj/input/virtual_keys.h"
#include "test_support.h"

namespace
{

namespace input = re2dj::input;

// Key names as the Windows host has always read them.
void CheckKeyNames(re2dj::test::Context& context)
{
    RE2DJ_CHECK_EQ(context, input::ParseKeyName("z"), static_cast<int>('Z'));
    RE2DJ_CHECK_EQ(context, input::ParseKeyName("1"), static_cast<int>('1'));
    RE2DJ_CHECK_EQ(context, input::ParseKeyName("F5"), 0x74);
    RE2DJ_CHECK_EQ(context, input::ParseKeyName("f24"), 0x87);
    RE2DJ_CHECK_EQ(context, input::ParseKeyName("F1X"), 0x70);
    RE2DJ_CHECK_EQ(context, input::ParseKeyName("F25"), -1);
    RE2DJ_CHECK_EQ(context, input::ParseKeyName("numpad3"), 0x63);
    RE2DJ_CHECK_EQ(context, input::ParseKeyName("DECIMAL"), 0x6E);
    RE2DJ_CHECK_EQ(context, input::ParseKeyName("LSHIFT"), 0xA0);
    RE2DJ_CHECK_EQ(context, input::ParseKeyName("Enter"), 0x0D);
    RE2DJ_CHECK_EQ(context, input::ParseKeyName("pgdn"), 0x22);
    RE2DJ_CHECK_EQ(context, input::ParseKeyName("NONE"), 0);
    RE2DJ_CHECK_EQ(context, input::ParseKeyName(""), 0);
    RE2DJ_CHECK_EQ(context, input::ParseKeyName("bogus"), -1);
}

// Reads `section.name=value` entries, enough for the example file.
std::map<std::string, std::string> ReadIni(const std::string& path)
{
    std::map<std::string, std::string> entries;
    std::ifstream stream(path);
    std::string line;
    std::string section;
    while (std::getline(stream, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == ';') continue;
        if (line[0] == '[')
        {
            section = line.substr(1, line.find(']') - 1);
            continue;
        }
        const std::size_t equals = line.find('=');
        if (equals != std::string::npos)
        {
            entries[section + "." + line.substr(0, equals)] = line.substr(equals + 1);
        }
    }
    return entries;
}

// Every default is a key, and the example configuration lists the same ones.
void CheckDefaults(re2dj::test::Context& context)
{
    const std::map<std::string, std::string> example =
        ReadIni(std::string(RE2DJ_TEST_SOURCE_DIR) + "/config/ez2dj-io.example.ini");
    RE2DJ_CHECK(context, !example.empty());
    RE2DJ_CHECK_EQ(context, input::Ez2DjButtonBindings().size(), static_cast<std::size_t>(input::Ez2DjButton::kCount));
    for (const input::Ez2DjButtonBinding& binding : input::Ez2DjButtonBindings())
    {
        RE2DJ_CHECK(context, input::ParseKeyName(binding.default_key) > 0);
        const auto found = example.find("buttons." + std::string(binding.name));
        RE2DJ_CHECK(context, found != example.end() && found->second == binding.default_key);
    }
    RE2DJ_CHECK_EQ(context, input::Ez2DjTurntableBindings().size(), std::size_t{4});
    for (const input::Ez2DjTurntableBinding& binding : input::Ez2DjTurntableBindings())
    {
        RE2DJ_CHECK(context, input::ParseKeyName(binding.default_key) > 0);
        const auto found = example.find("turntables." + std::string(binding.name));
        RE2DJ_CHECK(context, found != example.end() && found->second == binding.default_key);
    }
}

// The turntable moves by the step toward the held key at most every 8 ms,
// holds still with both or neither, and wraps.
void CheckTurntables(re2dj::test::Context& context)
{
    input::LegacyIoPortBus bus;
    input::Ez2DjTurntables turntables;
    // Player 1's turntable reads at port 0x103.
    const auto position = [&] {
        std::uint8_t value = 0;
        bus.ReadByte(0x103, &value);
        return value;
    };
    turntables.Update(&bus, 1000, {false, true, false, false}, 4);
    RE2DJ_CHECK_EQ(context, position(), std::uint8_t{0x84});
    turntables.Update(&bus, 1004, {false, true, false, false}, 4);
    RE2DJ_CHECK_EQ(context, position(), std::uint8_t{0x84});
    turntables.Update(&bus, 1008, {false, true, false, false}, 4);
    RE2DJ_CHECK_EQ(context, position(), std::uint8_t{0x88});
    turntables.Update(&bus, 1016, {true, true, false, false}, 4);
    RE2DJ_CHECK_EQ(context, position(), std::uint8_t{0x88});
    turntables.Update(&bus, 1024, {false, false, false, false}, 4);
    RE2DJ_CHECK_EQ(context, position(), std::uint8_t{0x88});
    for (std::uint64_t step = 0; step < 40; ++step)
    {
        turntables.Update(&bus, 1032 + step * 8, {true, false, false, false}, 4);
    }
    // 0x88 less 40 steps of 4 wraps below zero to 0xE8.
    RE2DJ_CHECK_EQ(context, position(), std::uint8_t{0xE8});
}

}  // namespace

void RunEz2DjKeyboardMapTests(re2dj::test::Context& context)
{
    CheckKeyNames(context);
    CheckDefaults(context);
    CheckTurntables(context);
}
