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
