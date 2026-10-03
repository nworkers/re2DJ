#include "re2dj/input/gamepad.h"
#include "re2dj/input/io_bindings.h"

#include <fstream>
#include <map>
#include <sstream>
#include <string>

#include "re2dj/input/ez2dancer_keyboard_map.h"
#include "re2dj/input/ez2dj_keyboard_map.h"
#include "re2dj/input/virtual_keys.h"
#include "test_support.h"

namespace
{

namespace input = re2dj::input;

// Control names as the INI files write them, under ParseKeyName's contract.
void CheckControlNames(re2dj::test::Context& context)
{
    RE2DJ_CHECK_EQ(context, input::ParseGamepadControlName("A"), static_cast<int>(input::GamepadControl::kSouth));
    RE2DJ_CHECK_EQ(context, input::ParseGamepadControlName("y"), static_cast<int>(input::GamepadControl::kNorth));
    RE2DJ_CHECK_EQ(context, input::ParseGamepadControlName("dpad_left"),
                   static_cast<int>(input::GamepadControl::kDpadLeft));
    RE2DJ_CHECK_EQ(context, input::ParseGamepadControlName("LSTICK_RIGHT"),
                   static_cast<int>(input::GamepadControl::kLeftStickRight));
    RE2DJ_CHECK_EQ(context, input::ParseGamepadControlName("rt"), static_cast<int>(input::GamepadControl::kRightTrigger));
    RE2DJ_CHECK_EQ(context, input::ParseGamepadControlName("PADDLE4"), static_cast<int>(input::GamepadControl::kPaddle4));
    RE2DJ_CHECK_EQ(context, input::ParseGamepadControlName("NONE"), 0);
    RE2DJ_CHECK_EQ(context, input::ParseGamepadControlName(""), 0);
    RE2DJ_CHECK_EQ(context, input::ParseGamepadControlName("Z"), -1);
    RE2DJ_CHECK_EQ(context, input::ParseGamepadControlName("LSTICK_"), -1);
    // Every control has a name that parses back to it.
    for (int number = 1; number < static_cast<int>(input::GamepadControl::kCount); ++number)
    {
        const auto control = static_cast<input::GamepadControl>(number);
        const std::string_view name = input::GamepadControlName(control);
        RE2DJ_CHECK(context, !name.empty());
        RE2DJ_CHECK_EQ(context, input::ParseGamepadControlName(name), number);
    }
    RE2DJ_CHECK(context, input::GamepadControlName(input::GamepadControl::kNone).empty());
    input::GamepadControls controls;
    controls.set(static_cast<std::size_t>(input::GamepadControl::kEast));
    RE2DJ_CHECK(context, input::GamepadControlHeld(controls, static_cast<int>(input::GamepadControl::kEast)));
    RE2DJ_CHECK(context, !input::GamepadControlHeld(controls, 0));
    RE2DJ_CHECK(context, !input::GamepadControlHeld(controls, -1));
    RE2DJ_CHECK(context, !input::GamepadControlHeld(controls, 31));
}

// Reads `section.name=value` entries, enough for the example files.
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

std::string ReadText(const std::string& path)
{
    std::ifstream stream(path);
    std::stringstream text;
    text << stream.rdbuf();
    return text.str();
}

// Every default gamepad control is a control name, and the example
// configurations' [gamepad] sections list the same ones.
void CheckGamepadDefaults(re2dj::test::Context& context)
{
    const std::map<std::string, std::string> ez2dj =
        ReadIni(std::string(RE2DJ_TEST_SOURCE_DIR) + "/config/ez2dj-io.example.ini");
    for (const input::Ez2DjButtonBinding& binding : input::Ez2DjButtonBindings())
    {
        RE2DJ_CHECK(context, input::ParseGamepadControlName(binding.default_gamepad) >= 0);
        const auto found = ez2dj.find("gamepad." + std::string(binding.name));
        RE2DJ_CHECK(context, found != ez2dj.end() && found->second == binding.default_gamepad);
    }
    for (const input::Ez2DjTurntableBinding& binding : input::Ez2DjTurntableBindings())
    {
        RE2DJ_CHECK(context, input::ParseGamepadControlName(binding.default_gamepad) >= 0);
        const auto found = ez2dj.find("gamepad." + std::string(binding.name));
        RE2DJ_CHECK(context, found != ez2dj.end() && found->second == binding.default_gamepad);
    }
    const std::map<std::string, std::string> ez2dancer =
        ReadIni(std::string(RE2DJ_TEST_SOURCE_DIR) + "/config/ez2dancer-io.example.ini");
    for (const input::Ez2DancerButtonBinding& binding : input::Ez2DancerButtonBindings())
    {
        RE2DJ_CHECK(context, input::ParseGamepadControlName(binding.default_gamepad) >= 0);
        const auto found = ez2dancer.find("gamepad." + std::string(binding.name));
        RE2DJ_CHECK(context, found != ez2dancer.end() && found->second == binding.default_gamepad);
    }
    // Player 1's five keys and the turntable are on the pad by default.
    const input::Ez2DjIoBindings defaults = input::DefaultEz2DjIoBindings();
    RE2DJ_CHECK_EQ(context, defaults.button_gamepad[static_cast<std::size_t>(input::Ez2DjButton::kPlayer1Key1)],
                   static_cast<int>(input::GamepadControl::kWest));
    RE2DJ_CHECK_EQ(context, defaults.button_gamepad[static_cast<std::size_t>(input::Ez2DjButton::kPlayer2Key1)], 0);
    RE2DJ_CHECK_EQ(context, defaults.turntable_gamepad[0], static_cast<int>(input::GamepadControl::kLeftStickLeft));
    RE2DJ_CHECK_EQ(context, defaults.turntable_keys[0], input::kVkLShift);
    RE2DJ_CHECK_EQ(context, defaults.turntable_step, input::kEz2DjDefaultTurntableStep);
}

// The loader: empty text is the defaults, the example files are the
// defaults too, a partial file overrides only what it lists, NONE unbinds,
// and bad names or steps are errors that leave the bindings alone.
void CheckLoader(re2dj::test::Context& context)
{
    const input::Ez2DjIoBindings defaults = input::DefaultEz2DjIoBindings();
    input::Ez2DjIoBindings loaded;
    std::string error;
    RE2DJ_CHECK(context, input::LoadEz2DjIoBindings("", &loaded, &error));
    RE2DJ_CHECK(context, loaded.button_keys == defaults.button_keys);
    RE2DJ_CHECK(context, loaded.button_gamepad == defaults.button_gamepad);
    RE2DJ_CHECK(context, loaded.turntable_keys == defaults.turntable_keys);
    RE2DJ_CHECK(context, loaded.turntable_gamepad == defaults.turntable_gamepad);
    RE2DJ_CHECK_EQ(context, loaded.turntable_step, defaults.turntable_step);

    const std::string example = ReadText(std::string(RE2DJ_TEST_SOURCE_DIR) + "/config/ez2dj-io.example.ini");
    RE2DJ_CHECK(context, !example.empty());
    RE2DJ_CHECK(context, input::LoadEz2DjIoBindings(example, &loaded, &error));
    RE2DJ_CHECK(context, loaded.button_keys == defaults.button_keys);
    RE2DJ_CHECK(context, loaded.button_gamepad == defaults.button_gamepad);
    RE2DJ_CHECK(context, loaded.turntable_keys == defaults.turntable_keys);
    RE2DJ_CHECK(context, loaded.turntable_gamepad == defaults.turntable_gamepad);
    RE2DJ_CHECK_EQ(context, loaded.turntable_step, defaults.turntable_step);

    const std::string partial =
        "[buttons]\n"
        "p1_1 = NONE\n"
        "coin=f6\n"
        "[gamepad]\n"
        "p1_1=lb\n"
        "p1_pedal=NONE\n"
        "p2_start=\"RSTICK\"\n"
        "p1_negative=LT\n"
        "[turntables]\n"
        "p1_positive=RCTRL\n"
        "step=8\n";
    RE2DJ_CHECK(context, input::LoadEz2DjIoBindings(partial, &loaded, &error));
    const auto p1_1 = static_cast<std::size_t>(input::Ez2DjButton::kPlayer1Key1);
    const auto coin = static_cast<std::size_t>(input::Ez2DjButton::kCoin);
    const auto pedal = static_cast<std::size_t>(input::Ez2DjButton::kPlayer1Pedal);
    const auto p2_start = static_cast<std::size_t>(input::Ez2DjButton::kPlayer2Start);
    const auto p1_2 = static_cast<std::size_t>(input::Ez2DjButton::kPlayer1Key2);
    RE2DJ_CHECK_EQ(context, loaded.button_keys[p1_1], 0);
    RE2DJ_CHECK_EQ(context, loaded.button_keys[coin], input::kVkF1 + 5);
    RE2DJ_CHECK_EQ(context, loaded.button_keys[p1_2], defaults.button_keys[p1_2]);
    RE2DJ_CHECK_EQ(context, loaded.button_gamepad[p1_1], static_cast<int>(input::GamepadControl::kLeftShoulder));
    RE2DJ_CHECK_EQ(context, loaded.button_gamepad[pedal], 0);
    RE2DJ_CHECK_EQ(context, loaded.button_gamepad[p2_start], static_cast<int>(input::GamepadControl::kRightStick));
    RE2DJ_CHECK_EQ(context, loaded.button_gamepad[p1_2], defaults.button_gamepad[p1_2]);
    RE2DJ_CHECK_EQ(context, loaded.turntable_gamepad[0], static_cast<int>(input::GamepadControl::kLeftTrigger));
    RE2DJ_CHECK_EQ(context, loaded.turntable_gamepad[1], defaults.turntable_gamepad[1]);
    RE2DJ_CHECK_EQ(context, loaded.turntable_keys[1], input::kVkRControl);
    RE2DJ_CHECK_EQ(context, loaded.turntable_keys[0], defaults.turntable_keys[0]);
    RE2DJ_CHECK_EQ(context, loaded.turntable_step, std::uint8_t{8});

    // Errors leave the previous bindings in place.
    const input::Ez2DjIoBindings before = loaded;
    RE2DJ_CHECK(context, !input::LoadEz2DjIoBindings("[gamepad]\np1_3=Z\n", &loaded, &error));
    RE2DJ_CHECK(context, error.find("gamepad.p1_3") != std::string::npos);
    RE2DJ_CHECK(context, !input::LoadEz2DjIoBindings("[buttons]\np1_3=bogus\n", &loaded, &error));
    RE2DJ_CHECK(context, error.find("buttons.p1_3") != std::string::npos);
    RE2DJ_CHECK(context, !input::LoadEz2DjIoBindings("[turntables]\nstep=0\n", &loaded, &error));
    RE2DJ_CHECK(context, !input::LoadEz2DjIoBindings("[turntables]\nstep=33\n", &loaded, &error));
    RE2DJ_CHECK(context, !input::LoadEz2DjIoBindings("[turntables]\nstep=fast\n", &loaded, &error));
    RE2DJ_CHECK(context, loaded.button_gamepad == before.button_gamepad);
    RE2DJ_CHECK_EQ(context, loaded.turntable_step, before.turntable_step);
    // An empty step is the default, as GetPrivateProfileIntA reads it.
    RE2DJ_CHECK(context, input::LoadEz2DjIoBindings("[turntables]\nstep=\n", &loaded, &error));
    RE2DJ_CHECK_EQ(context, loaded.turntable_step, input::kEz2DjDefaultTurntableStep);

    // EZ2Dancer the same way.
    const input::Ez2DancerIoBindings dancer_defaults = input::DefaultEz2DancerIoBindings();
    input::Ez2DancerIoBindings dancer;
    RE2DJ_CHECK(context, input::LoadEz2DancerIoBindings("", &dancer, &error));
    RE2DJ_CHECK(context, dancer.button_keys == dancer_defaults.button_keys);
    RE2DJ_CHECK(context, dancer.button_gamepad == dancer_defaults.button_gamepad);
    const std::string dancer_example =
        ReadText(std::string(RE2DJ_TEST_SOURCE_DIR) + "/config/ez2dancer-io.example.ini");
    RE2DJ_CHECK(context, input::LoadEz2DancerIoBindings(dancer_example, &dancer, &error));
    RE2DJ_CHECK(context, dancer.button_keys == dancer_defaults.button_keys);
    RE2DJ_CHECK(context, dancer.button_gamepad == dancer_defaults.button_gamepad);
    RE2DJ_CHECK(context, input::LoadEz2DancerIoBindings("[gamepad]\np2_left=DPAD_UP\n", &dancer, &error));
    RE2DJ_CHECK_EQ(context, dancer.button_gamepad[static_cast<std::size_t>(input::Ez2DancerButton::kPlayer2Left)],
                   static_cast<int>(input::GamepadControl::kDpadUp));
    RE2DJ_CHECK_EQ(context, dancer.button_gamepad[static_cast<std::size_t>(input::Ez2DancerButton::kPlayer1Left)],
                   static_cast<int>(input::GamepadControl::kDpadLeft));
    RE2DJ_CHECK(context, !input::LoadEz2DancerIoBindings("[gamepad]\ncoin=F5\n", &dancer, &error));
}

}  // namespace

void RunGamepadBindingsTests(re2dj::test::Context& context)
{
    CheckControlNames(context);
    CheckGamepadDefaults(context);
    CheckLoader(context);
}
