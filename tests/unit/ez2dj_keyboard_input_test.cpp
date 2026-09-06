#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "src/platform/windows/ez2dj_keyboard_input.h"
#include "temporary_tree.h"
#include "test_support.h"

int main()
{
    re2dj::test::Context context;

    // Test 1: Initialize with the repository's example IO config.
    // This must succeed now that TAB and other key bindings are supported.
    {
        re2dj::platform::windows::Ez2DjKeyboardInput input;
        std::string error;
        const std::filesystem::path example_ini = "config/ez2dj-io.example.ini";
        const std::filesystem::path abs_path = std::filesystem::absolute(example_ini);
        const bool ok = input.Initialize(abs_path.string().c_str(), &error);
        RE2DJ_CHECK(context, ok);
        if (!ok)
        {
            std::fprintf(stderr, "Failed to initialize example INI: %s\n", error.c_str());
        }
    }

    // Test 2: Custom temporary INI testing various key names.
    {
        const re2dj::test::TemporaryTree tree;
        const std::string custom_ini =
            "[buttons]\n"
            "test=ESC\n"
            "service=F2\n"
            "coin=F5\n"
            "effector1=Q\n"
            "effector2=W\n"
            "effector3=E\n"
            "effector4=R\n"
            "p1_start=1\n"
            "p2_start=2\n"
            "p1_1=Z\n"
            "p1_2=S\n"
            "p1_3=X\n"
            "p1_4=D\n"
            "p1_5=C\n"
            "p1_pedal=SPACE\n"
            "p2_1=NUMPAD1\n"
            "p2_2=NUMPAD2\n"
            "p2_3=NUMPAD3\n"
            "p2_4=NUMPAD4\n"
            "p2_5=NUMPAD5\n"
            "p2_pedal=DECIMAL\n"
            "\n"
            "[turntables]\n"
            "p1_negative=LSHIFT\n"
            "p1_positive=TAB\n"
            "p2_negative=ENTER\n"
            "p2_positive=RSHIFT\n"
            "step=8\n";
        tree.WriteText("custom.ini", custom_ini);

        re2dj::platform::windows::Ez2DjKeyboardInput input;
        std::string error;
        const std::filesystem::path ini_path = tree.root() / "custom.ini";
        const bool ok = input.Initialize(ini_path.string().c_str(), &error);
        RE2DJ_CHECK(context, ok);

        re2dj::input::LegacyIoPortBus bus;
        input.Poll(&bus, 1000);
        // Ensure no crash or failure during poll.
        RE2DJ_CHECK(context, true);
    }

    // Test 3: Invalid step value (< 1 or > 32).
    {
        const re2dj::test::TemporaryTree tree;
        const std::string invalid_ini =
            "[buttons]\n"
            "test=F1\nservice=F2\ncoin=F5\neffector1=Q\neffector2=W\neffector3=E\neffector4=R\n"
            "p1_start=1\np2_start=2\np1_1=Z\np1_2=S\np1_3=X\np1_4=D\np1_5=C\np1_pedal=SPACE\n"
            "p2_1=NUMPAD1\np2_2=NUMPAD2\np2_3=NUMPAD3\np2_4=NUMPAD4\np2_5=NUMPAD5\np2_pedal=DECIMAL\n"
            "[turntables]\n"
            "p1_negative=LSHIFT\np1_positive=TAB\np2_negative=ENTER\np2_positive=RSHIFT\n"
            "step=99\n";
        tree.WriteText("invalid.ini", invalid_ini);

        re2dj::platform::windows::Ez2DjKeyboardInput input;
        std::string error;
        const std::filesystem::path ini_path = tree.root() / "invalid.ini";
        const bool ok = input.Initialize(ini_path.string().c_str(), &error);
        RE2DJ_CHECK(context, !ok);
        RE2DJ_CHECK(context, !error.empty());
    }

    // Test 4: Unknown key name reports error.
    {
        const re2dj::test::TemporaryTree tree;
        const std::string bad_key_ini =
            "[buttons]\n"
            "test=UNKNOWN_KEY_NAME_XYZ\nservice=F2\ncoin=F5\neffector1=Q\neffector2=W\neffector3=E\neffector4=R\n"
            "p1_start=1\np2_start=2\np1_1=Z\np1_2=S\np1_3=X\np1_4=D\np1_5=C\np1_pedal=SPACE\n"
            "p2_1=NUMPAD1\np2_2=NUMPAD2\np2_3=NUMPAD3\np2_4=NUMPAD4\np2_5=NUMPAD5\np2_pedal=DECIMAL\n"
            "[turntables]\n"
            "p1_negative=LSHIFT\np1_positive=TAB\np2_negative=ENTER\np2_positive=RSHIFT\n"
            "step=4\n";
        tree.WriteText("bad_key.ini", bad_key_ini);

        re2dj::platform::windows::Ez2DjKeyboardInput input;
        std::string error;
        const std::filesystem::path ini_path = tree.root() / "bad_key.ini";
        const bool ok = input.Initialize(ini_path.string().c_str(), &error);
        RE2DJ_CHECK(context, !ok);
        RE2DJ_CHECK(context, !error.empty());
    }

    std::printf("checks: %d, failures: %d\n", context.checks, context.failures);
    return context.failures == 0 ? 0 : 1;
}
