#include <cstdio>
#include <string>
#include <vector>

#include "re2dj/platform/windows/original_process_backend.h"

int main()
{
    const re2dj::target::BuiltInTargetProfile* first_profile =
        re2dj::target::FindBuiltInTargetProfileById("ez2dj1stse");
    const re2dj::target::BuiltInTargetProfile* third_profile =
        re2dj::target::FindBuiltInTargetProfileById("ez2dj3rd");
    re2dj::platform::windows::OriginalProcessOptions options;
    options.hdd_directory = "asset-free-hdd";
    options.target_id = "ez2dj1stse";
    if (first_profile != nullptr)
    {
        options.hle_profile_id = first_profile->profile.hle_profile_id;
        options.profile_defaults = first_profile->profile.run_defaults;
    }
    std::vector<std::string> arguments;
    std::string error;
    const bool canonical =
        first_profile != nullptr &&
        re2dj::platform::windows::BuildOriginalProcessArguments(options, &arguments, &error) &&
        arguments.size() == 21 &&
        arguments[1] == "--hdd" &&
        arguments[2] == "asset-free-hdd" &&
        arguments[3] == "--target" &&
        arguments[4] == "ez2dj1stse" &&
        arguments[10] == "--audio-gain-db" &&
        arguments[11] == "0.000000" &&
        arguments[12] == "--demo-volume" &&
        arguments[13] == "3" &&
        arguments[14] == "--hle-io-ports" &&
        arguments[15] == "--run-detached" &&
        arguments[16] == "--device-mock-lptdi" &&
        arguments[17] == "--device-mock-lptdi-path-prefix" &&
        arguments[18] == "\\\\.\\LPTDI" &&
        arguments[19] == "--device-mock-lptdi-target-state" &&
        arguments[20] == "0900000000000000";

    options.profile_defaults.audio_gain_db = 12.0f;
    const bool custom_gain =
        re2dj::platform::windows::BuildOriginalProcessArguments(
            options, &arguments, &error) &&
        arguments[11] == "12.000000";

    options.profile_defaults.demo_volume = 2;
    const bool custom_demo_volume =
        re2dj::platform::windows::BuildOriginalProcessArguments(
            options, &arguments, &error) &&
        arguments[13] == "2";

    options.audio_volume_trace = true;
    const bool audio_trace =
        re2dj::platform::windows::BuildOriginalProcessArguments(
            options, &arguments, &error) &&
        arguments.size() == 22 && arguments.back() == "--audio-volume-trace";

    options.audio_volume_trace = false;
    options.profile_defaults.fullscreen = true;
    const bool fullscreen =
        re2dj::platform::windows::BuildOriginalProcessArguments(
            options, &arguments, &error) &&
        arguments.size() == 22 && arguments.back() == "--fullscreen";

    options.profile_defaults.audio_gain_db = 19.0f;
    const bool invalid_gain =
        !re2dj::platform::windows::BuildOriginalProcessArguments(
            options, &arguments, &error) &&
        error.find("+18") != std::string::npos;

    options.profile_defaults.audio_gain_db = 0.0f;
    options.profile_defaults.demo_volume = 4;
    const bool invalid_demo_volume =
        !re2dj::platform::windows::BuildOriginalProcessArguments(
            options, &arguments, &error) &&
        error.find("between 0 and 3") != std::string::npos;

    options.profile_defaults.audio_gain_db = 12.0f;
    options.profile_defaults.demo_volume = 3;
    options.profile_defaults.fullscreen = false;
    options.io_config = "keyboard.ini";
    const bool io_config =
        re2dj::platform::windows::BuildOriginalProcessArguments(
            options, &arguments, &error) &&
        arguments.size() == 23 && arguments[21] == "--io-config" &&
        arguments[22] == "keyboard.ini";

    options.io_config.clear();
    options.profile_defaults.lptdi.device_mock_enabled = false;
    const bool invalid_lptdi_policy =
        !re2dj::platform::windows::BuildOriginalProcessArguments(
            options, &arguments, &error) &&
        error.find("without an LPTDI device policy") != std::string::npos;

    const bool third_defaults = [&]() {
        if (third_profile == nullptr)
        {
            return false;
        }
        options.io_config.clear();
        options.target_id = "ez2dj3rd";
        options.hle_profile_id = third_profile->profile.hle_profile_id;
        options.profile_defaults = third_profile->profile.run_defaults;
        if (!re2dj::platform::windows::BuildOriginalProcessArguments(
                options, &arguments, &error))
        {
            return false;
        }
        bool has_demo_volume = false;
        bool has_io_ports = false;
        bool has_lptdi_path = false;
        bool has_lptdi_state = false;
        for (const std::string& argument : arguments)
        {
            has_demo_volume = has_demo_volume || argument == "--demo-volume";
            has_io_ports = has_io_ports || argument == "--hle-io-ports";
            has_lptdi_path = has_lptdi_path ||
                             argument == "--device-mock-lptdi-path-prefix";
            has_lptdi_state = has_lptdi_state ||
                              argument == "--device-mock-lptdi-target-state";
        }
        return arguments.size() == 15 && arguments[5] == "--hle-vfs" &&
               arguments[6] == "--hle-directsound" && arguments[9] == "--run-detached" &&
               arguments[10] == "--device-mock-lptdi" &&
               arguments[11] == "--device-mock-lptdi-path-prefix" &&
               arguments[12] == "\\\\.\\FEnteDev" &&
               arguments[13] == "--device-mock-lptdi-target-state" &&
               arguments[14] == "0000000000000000" &&
               !has_demo_volume && !has_io_ports && has_lptdi_path && has_lptdi_state &&
               !third_profile->profile.run_defaults.lptdi.legacy_io_ports &&
               third_profile->profile.run_defaults.lptdi.device_mock_enabled &&
               third_profile->profile.run_defaults.lptdi.device_mock_path_prefix ==
                   "\\\\.\\FEnteDev" &&
               third_profile->profile.run_defaults.lptdi.device_mock_target_state_hex ==
                   "0000000000000000";
    }();

    options.target_id = "ez2dj1stse_unpacked";
    options.hle_profile_id = "ez2dj1stse_unpacked";
    options.profile_defaults = {};
    const bool rejected =
        !re2dj::platform::windows::BuildOriginalProcessArguments(
            options, &arguments, &error) &&
        error.find("invalid Windows original-process options") != std::string::npos;
    if (!canonical || !custom_gain || !custom_demo_volume || !audio_trace || !fullscreen ||
        !invalid_gain || !invalid_demo_volume || !io_config || !invalid_lptdi_policy ||
        !third_defaults || !rejected)
    {
        std::fprintf(stderr, "windows-product-loader-probe: %s\n", error.c_str());
        return 1;
    }
    std::printf("windows-product-loader-probe: profile-defaults=ok unsupported-target=ok\n");
    return 0;
}
