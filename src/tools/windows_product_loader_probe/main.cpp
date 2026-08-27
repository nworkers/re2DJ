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
        arguments.size() == 16 &&
        arguments[1] == "--hdd" &&
        arguments[2] == "asset-free-hdd" &&
        arguments[3] == "--target" &&
        arguments[4] == "ez2dj1stse" &&
        arguments[10] == "--audio-gain-db" &&
        arguments[11] == "6.000000" &&
        arguments[13] == "--run-detached" &&
        arguments[14] == "--device-mock-lptdi-target-state" &&
        arguments[15] == "0900000000000000";

    options.audio_gain_db = 12.0f;
    const bool custom_gain =
        re2dj::platform::windows::BuildOriginalProcessArguments(
            options, &arguments, &error) &&
        arguments[11] == "12.000000";

    options.audio_volume_trace = true;
    const bool audio_trace =
        re2dj::platform::windows::BuildOriginalProcessArguments(
            options, &arguments, &error) &&
        arguments.size() == 17 && arguments.back() == "--audio-volume-trace";

    options.audio_gain_db = 19.0f;
    const bool invalid_gain =
        !re2dj::platform::windows::BuildOriginalProcessArguments(
            options, &arguments, &error) &&
        error.find("+18") != std::string::npos;

    options.audio_gain_db = 12.0f;
    options.target_id = "ez2dj1stse_unpacked";
    const bool rejected =
        !re2dj::platform::windows::BuildOriginalProcessArguments(
            options, &arguments, &error) &&
        error.find("ez2dj1stse") != std::string::npos;
    if (!canonical || !custom_gain || !audio_trace || !invalid_gain || !rejected)
    {
        std::fprintf(stderr, "windows-product-loader-probe: %s\n", error.c_str());
        return 1;
    }
    std::printf("windows-product-loader-probe: canonical-policy=ok unsupported-target=ok\n");
    return 0;
}
