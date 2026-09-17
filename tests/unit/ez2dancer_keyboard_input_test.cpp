#include <cstdio>
#include <filesystem>
#include <string>

#include "src/platform/windows/ez2dancer_keyboard_input.h"
#include "temporary_tree.h"
#include "test_support.h"

namespace
{

constexpr char kIdleBindings[] =
    "[buttons]\n"
    "test=NONE\n"
    "service=NONE\n"
    "p1_left=NONE\n"
    "p1_center=NONE\n"
    "p1_right=NONE\n"
    "p2_left=NONE\n"
    "p2_center=NONE\n"
    "p2_right=NONE\n"
    "p1_sensor_top_left=NONE\n"
    "p1_sensor_top_right=NONE\n"
    "p1_sensor_bottom_left=NONE\n"
    "p1_sensor_bottom_right=NONE\n"
    "p2_sensor_top_left=NONE\n"
    "p2_sensor_top_right=NONE\n"
    "p2_sensor_bottom_left=NONE\n"
    "p2_sensor_bottom_right=NONE\n"
    "coin=NONE\n";

}  // namespace

int main()
{
    re2dj::test::Context context;

    {
        re2dj::platform::windows::Ez2DancerKeyboardInput input;
        std::string error;
        const std::filesystem::path path =
            std::filesystem::absolute("config/ez2dancer-io.example.ini");
        const bool ok = input.Initialize(path.string().c_str(), &error);
        RE2DJ_CHECK(context, ok);
        if (!ok)
        {
            std::fprintf(stderr, "Failed to initialize example INI: %s\n", error.c_str());
        }
    }

    {
        const re2dj::test::TemporaryTree tree;
        tree.WriteText("idle.ini", kIdleBindings);
        re2dj::platform::windows::Ez2DancerKeyboardInput input;
        std::string error;
        const std::filesystem::path path = tree.root() / "idle.ini";
        RE2DJ_CHECK(context, input.Initialize(path.string().c_str(), &error));

        re2dj::input::Ez2DancerIoPortBus bus;
        input.Poll(&bus);
        std::uint16_t value = 0;
        RE2DJ_CHECK(context, bus.ReadWord(0x300, &value));
        RE2DJ_CHECK(context, value == 0xf000);
        RE2DJ_CHECK(context, bus.ReadWord(0x306, &value));
        RE2DJ_CHECK(context, value == 0x00ff);
    }

    // With no configuration file the built-in mapping binds everything, and it
    // is the example INI's mapping: the two must not drift apart.
    {
        re2dj::platform::windows::Ez2DancerKeyboardInput defaults;
        re2dj::platform::windows::Ez2DancerKeyboardInput from_example;
        std::string error;
        RE2DJ_CHECK(context, defaults.Initialize(nullptr, &error));
        const std::filesystem::path path =
            std::filesystem::absolute("config/ez2dancer-io.example.ini");
        RE2DJ_CHECK(context, from_example.Initialize(path.string().c_str(), &error));
        for (std::size_t index = 0;
             index < static_cast<std::size_t>(re2dj::input::Ez2DancerButton::kCount); ++index)
        {
            const auto button = static_cast<re2dj::input::Ez2DancerButton>(index);
            RE2DJ_CHECK(context, defaults.button_key(button) != 0);
            RE2DJ_CHECK_EQ(context, defaults.button_key(button), from_example.button_key(button));
        }
        // An empty path means the same as none at all.
        re2dj::platform::windows::Ez2DancerKeyboardInput empty_path;
        RE2DJ_CHECK(context, empty_path.Initialize("", &error));
        RE2DJ_CHECK_EQ(context, empty_path.button_key(re2dj::input::Ez2DancerButton::kCoin),
                       defaults.button_key(re2dj::input::Ez2DancerButton::kCoin));
    }

    // A file overrides the entries it lists and leaves the rest at their
    // defaults, and NONE is how an entry gives up its default.
    {
        const re2dj::test::TemporaryTree tree;
        tree.WriteText("partial.ini", "[buttons]\np1_left=G\ncoin=NONE\n");
        re2dj::platform::windows::Ez2DancerKeyboardInput input;
        re2dj::platform::windows::Ez2DancerKeyboardInput defaults;
        std::string error;
        const std::filesystem::path path = tree.root() / "partial.ini";
        RE2DJ_CHECK(context, input.Initialize(path.string().c_str(), &error));
        RE2DJ_CHECK(context, defaults.Initialize(nullptr, &error));
        RE2DJ_CHECK_EQ(context, input.button_key(re2dj::input::Ez2DancerButton::kPlayer1Left),
                       static_cast<int>('G'));
        RE2DJ_CHECK_EQ(context, input.button_key(re2dj::input::Ez2DancerButton::kCoin), 0);
        RE2DJ_CHECK_EQ(context, input.button_key(re2dj::input::Ez2DancerButton::kPlayer2Right),
                       defaults.button_key(re2dj::input::Ez2DancerButton::kPlayer2Right));
    }

    {
        const re2dj::test::TemporaryTree tree;
        std::string invalid = kIdleBindings;
        invalid.replace(invalid.find("p1_left=NONE"), 12, "p1_left=UNKNOWN_KEY");
        tree.WriteText("invalid.ini", invalid);
        re2dj::platform::windows::Ez2DancerKeyboardInput input;
        std::string error;
        const std::filesystem::path path = tree.root() / "invalid.ini";
        RE2DJ_CHECK(context, !input.Initialize(path.string().c_str(), &error));
        RE2DJ_CHECK(context, !error.empty());
    }

    std::printf("checks: %d, failures: %d\n", context.checks, context.failures);
    return context.failures == 0 ? 0 : 1;
}
