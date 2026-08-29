#include <cstdio>
#include <string>
#include <vector>

#include "re2dj/platform/windows/original_process_backend.h"

int main()
{
    re2dj::platform::windows::OriginalProcessOptions options;
    options.hdd_directory = "asset-free-hdd";
    options.target_id = "ez2dj1stse";
    std::vector<std::string> arguments;
    std::string error;
    const bool canonical =
        re2dj::platform::windows::BuildOriginalProcessArguments(
            options, &arguments, &error) &&
        arguments.size() == 18 &&
        arguments[1] == "--hdd" &&
        arguments[2] == "asset-free-hdd" &&
        arguments[3] == "--target" &&
        arguments[4] == "ez2dj1stse" &&
        arguments[10] == "--audio-gain-db" &&
        arguments[11] == "0.000000" &&
        arguments[12] == "--demo-volume" &&
        arguments[13] == "3" &&
        arguments[15] == "--run-detached" &&
        arguments[16] == "--device-mock-lptdi-target-state" &&
        arguments[17] == "0900000000000000";

    options.audio_gain_db = 12.0f;
    const bool custom_gain =
        re2dj::platform::windows::BuildOriginalProcessArguments(
            options, &arguments, &error) &&
        arguments[11] == "12.000000";

    options.demo_volume = 2;
    const bool custom_demo_volume =
        re2dj::platform::windows::BuildOriginalProcessArguments(
            options, &arguments, &error) &&
        arguments[13] == "2";

    options.audio_volume_trace = true;
    const bool audio_trace =
        re2dj::platform::windows::BuildOriginalProcessArguments(
            options, &arguments, &error) &&
        arguments.size() == 19 && arguments.back() == "--audio-volume-trace";

    options.audio_volume_trace = false;
    options.fullscreen = true;
    const bool fullscreen =
        re2dj::platform::windows::BuildOriginalProcessArguments(
            options, &arguments, &error) &&
        arguments.size() == 19 && arguments.back() == "--fullscreen";

    options.audio_gain_db = 19.0f;
    const bool invalid_gain =
        !re2dj::platform::windows::BuildOriginalProcessArguments(
            options, &arguments, &error) &&
        error.find("+18") != std::string::npos;

    options.audio_gain_db = 0.0f;
    options.demo_volume = 4;
    const bool invalid_demo_volume =
        !re2dj::platform::windows::BuildOriginalProcessArguments(
            options, &arguments, &error) &&
        error.find("between 0 and 3") != std::string::npos;

    options.audio_gain_db = 12.0f;
    options.demo_volume = 3;
    options.fullscreen = false;
    options.io_config = "keyboard.ini";
    const bool io_config =
        re2dj::platform::windows::BuildOriginalProcessArguments(
            options, &arguments, &error) &&
        arguments.size() == 20 && arguments[18] == "--io-config" &&
        arguments[19] == "keyboard.ini";

    options.target_id = "ez2dj1stse_unpacked";
    const bool rejected =
        !re2dj::platform::windows::BuildOriginalProcessArguments(
            options, &arguments, &error) &&
        error.find("ez2dj1stse") != std::string::npos;
    if (!canonical || !custom_gain || !custom_demo_volume || !audio_trace || !fullscreen ||
        !invalid_gain || !invalid_demo_volume || !io_config || !rejected)
    {
        std::fprintf(stderr, "windows-product-loader-probe: %s\n", error.c_str());
        return 1;
    }
    std::printf("windows-product-loader-probe: canonical-policy=ok unsupported-target=ok\n");
    return 0;
}
