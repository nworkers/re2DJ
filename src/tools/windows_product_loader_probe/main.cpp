#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "re2dj/exe/pe_image.h"
#include "re2dj/platform/windows/original_process_backend.h"
#include "../windows_original_process_probe/iat_verifier.h"

namespace
{

void PutU16(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint16_t value)
{
    bytes[offset + 0] = static_cast<std::uint8_t>(value & 0xFF);
    bytes[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
}

void PutU32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value)
{
    for (std::size_t index = 0; index < 4; ++index)
    {
        bytes[offset + index] = static_cast<std::uint8_t>((value >> (index * 8)) & 0xFF);
    }
}

std::vector<std::uint8_t> MakeSyntheticPeWithImports()
{
    std::vector<std::uint8_t> bytes(0x600, 0);
    bytes[0] = 'M';
    bytes[1] = 'Z';
    PutU32(bytes, 0x3C, 0x80);

    bytes[0x80] = 'P';
    bytes[0x81] = 'E';

    PutU16(bytes, 0x84, re2dj::exe::kMachineI386);
    PutU16(bytes, 0x86, 1);
    PutU16(bytes, 0x94, 224);
    PutU16(bytes, 0x96, 0x010E);

    PutU16(bytes, 0x98, static_cast<std::uint16_t>(re2dj::exe::PeMagic::kPe32));
    PutU32(bytes, 0x98 + 16, 0x1000);
    PutU32(bytes, 0x98 + 28, 0x00400000);
    PutU32(bytes, 0x98 + 32, 0x1000);
    PutU32(bytes, 0x98 + 36, 0x200);
    PutU32(bytes, 0x98 + 56, 0x4000);
    PutU32(bytes, 0x98 + 60, 0x400);
    PutU16(bytes, 0x98 + 68, re2dj::exe::kSubsystemWindowsGui);
    PutU32(bytes, 0x98 + 92, 16);

    const std::size_t import_dir = 0x98 + 96 + 8;
    PutU32(bytes, import_dir + 0, 0x1000);
    PutU32(bytes, import_dir + 4, 0x100);

    const std::size_t sec = 0x98 + 224;
    std::memcpy(&bytes[sec], ".rdata\0\0", 8);
    PutU32(bytes, sec + 8, 0x1000);
    PutU32(bytes, sec + 12, 0x1000);
    PutU32(bytes, sec + 16, 0x200);
    PutU32(bytes, sec + 20, 0x400);
    PutU32(bytes, sec + 36, 0xC0000040);

    PutU32(bytes, 0x400 + 0, 0x1030);
    PutU32(bytes, 0x400 + 12, 0x1050);
    PutU32(bytes, 0x400 + 16, 0x1040);

    PutU32(bytes, 0x430 + 0, 0x1060);
    PutU32(bytes, 0x430 + 4, 0x80000005);
    PutU32(bytes, 0x430 + 8, 0);

    PutU32(bytes, 0x440 + 0, 0x1060);
    PutU32(bytes, 0x440 + 4, 0x80000005);
    PutU32(bytes, 0x440 + 8, 0);

    std::memcpy(&bytes[0x450], "sample.dll\0", 11);

    PutU16(bytes, 0x460, 1);
    std::memcpy(&bytes[0x462], "SampleFunc\0", 11);

    return bytes;
}

bool TestResolveIatSlot()
{
    const std::vector<std::uint8_t> pe_bytes = MakeSyntheticPeWithImports();
    re2dj::exe::PeImageInfo info;
    std::string err;
    if (!re2dj::exe::ReadPeImageInfo(pe_bytes.data(), pe_bytes.size(), &info, &err))
    {
        return false;
    }
    re2dj::tools::windows_original_process_probe::IatSlotResolution res1;
    if (!re2dj::tools::windows_original_process_probe::ResolveIatSlot(
            info, pe_bytes.data(), pe_bytes.size(), 0x1040, &res1, &err) ||
        res1.module != "sample.dll" || res1.function != "SampleFunc" || res1.is_ordinal)
    {
        return false;
    }
    re2dj::tools::windows_original_process_probe::IatSlotResolution res2;
    if (!re2dj::tools::windows_original_process_probe::ResolveIatSlot(
            info, pe_bytes.data(), pe_bytes.size(), 0x1044, &res2, &err) ||
        res2.module != "sample.dll" || !res2.is_ordinal || res2.ordinal != 5)
    {
        return false;
    }
    re2dj::tools::windows_original_process_probe::IatSlotResolution res3;
    if (re2dj::tools::windows_original_process_probe::ResolveIatSlot(
            info, pe_bytes.data(), pe_bytes.size(), 0x9999, &res3, &err))
    {
        return false;
    }
    return true;
}

}  // namespace

int main()
{
    const re2dj::target::BuiltInTargetProfile* first_profile =
        re2dj::target::FindBuiltInTargetProfileById("ez2dj1stse");
    const re2dj::target::BuiltInTargetProfile* third_profile =
        re2dj::target::FindBuiltInTargetProfileById("ez2dj3rd");
    const re2dj::target::BuiltInTargetProfile* fourth_profile =
        re2dj::target::FindBuiltInTargetProfileById("ez2dj4th");
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
        arguments[20] == "0900000000000000" &&
        first_profile->profile.run_defaults.lptdi.legacy_io_ports &&
        first_profile->profile.run_defaults.lptdi.legacy_io_ports_default &&
        first_profile->profile.run_defaults.lptdi.legacy_io_in_byte_rva == 0x00038987 &&
        first_profile->profile.run_defaults.lptdi.legacy_io_out_byte_rva == 0x000389ab;

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
        return arguments.size() == 16 && arguments[5] == "--hle-vfs" &&
               arguments[6] == "--hle-directsound" && arguments[9] == "--run-detached" &&
               arguments[10] == "--device-mock-lptdi" &&
               arguments[11] == "--device-mock-lptdi-path-prefix" &&
               arguments[12] == "\\\\.\\FEnteDev" &&
               arguments[13] == "--device-mock-wts-console-session" &&
               arguments[14] == "--device-mock-lptdi-target-state" &&
               arguments[15] == "0000000000000000" &&
               !has_demo_volume && !has_io_ports && has_lptdi_path && has_lptdi_state &&
               !third_profile->profile.run_defaults.lptdi.legacy_io_ports &&
               third_profile->profile.run_defaults.lptdi.device_mock_enabled &&
               third_profile->profile.run_defaults.lptdi.device_mock_path_prefix ==
                   "\\\\.\\FEnteDev" &&
               third_profile->profile.run_defaults.lptdi.device_mock_target_state_hex ==
                   "0000000000000000";
    }();

    options.hdd_directory = "staged-chd";
    options.chd_image = "4thTrax.chd";
    options.executable_relative_path = "EZ2DJ/EZ2DJ.EXE";
    options.target_id = "ez2dj4th";
    options.hle_profile_id = fourth_profile == nullptr
                                 ? ""
                                 : fourth_profile->profile.hle_profile_id;
    options.profile_defaults = fourth_profile == nullptr
                                   ? re2dj::target::TargetRunDefaults{}
                                   : fourth_profile->profile.run_defaults;
    const bool chd_handoff =
        fourth_profile != nullptr &&
        fourth_profile->profile.run_defaults.hle_dynamic_vfs &&
        re2dj::platform::windows::BuildOriginalProcessArguments(
            options, &arguments, &error) &&
        arguments.size() == 17 && arguments[1] == "--hdd" &&
        arguments[2] == "staged-chd" && arguments[3] == "--target" &&
        arguments[4] == "ez2dj4th" && arguments[5] == "--chd" &&
        arguments[6] == "4thTrax.chd" && arguments[7] == "--target-executable" &&
        arguments[8] == "EZ2DJ/EZ2DJ.EXE" && arguments[9] == "--hle-vfs" &&
        arguments[10] == "--hle-d3d3" &&
        arguments[11] == "--hle-io-ports" &&
        arguments[12] == "--run-detached" &&
        arguments[13] == "--device-mock-lptdi" &&
        arguments[14] == "--device-mock-lptdi-path-prefix" &&
        arguments[15] == "\\\\.\\FEnteDev" &&
        arguments[16] == "--device-mock-wts-console-session" &&
        fourth_profile->profile.run_defaults.lptdi.legacy_io_ports &&
        fourth_profile->profile.run_defaults.lptdi.legacy_io_ports_default &&
        fourth_profile->profile.run_defaults.lptdi.legacy_io_in_byte_rva == 0x000c3817 &&
        fourth_profile->profile.run_defaults.lptdi.legacy_io_out_byte_rva == 0x000c384b;

    // Hardlock material is resolved inside the launcher from cfg, so no
    // Hardlock option may appear on the product command line.
    const bool no_diagnostic_options = [&]() {
        std::vector<std::string> product;
        if (!re2dj::platform::windows::BuildOriginalProcessArguments(
                options, &product, &error))
        {
            return false;
        }
        for (const std::string& argument : product)
        {
            if (argument.find("--hardlock-device") != std::string::npos ||
                argument.find("--hardlock-config") != std::string::npos ||
                argument.find("--hardlock-transform") != std::string::npos ||
                argument.find("--device-mock-hardlock") != std::string::npos)
            {
                return false;
            }
        }
        return true;
    }();

    // An active-console policy without a device policy is rejected rather than
    // silently forwarded, because the launcher option turns on both.
    const bool invalid_console_policy = [&]() {
        re2dj::platform::windows::OriginalProcessOptions console_options = options;
        console_options.profile_defaults.lptdi.legacy_io_ports = false;
        console_options.profile_defaults.lptdi.legacy_io_ports_default = false;
        console_options.profile_defaults.lptdi.legacy_io_in_byte_rva = 0;
        console_options.profile_defaults.lptdi.legacy_io_out_byte_rva = 0;
        console_options.profile_defaults.lptdi.device_mock_enabled = false;
        console_options.profile_defaults.lptdi.device_mock_path_prefix.clear();
        // Cleared so this isolates the console policy: Hardlock material also
        // requires the device boundary and would report its own rejection.
        console_options.profile_defaults.lptdi.hardlock_cfg_material_default = false;
        console_options.profile_defaults.hle_wts_active_console = true;
        std::vector<std::string> rejected_arguments;
        return !re2dj::platform::windows::BuildOriginalProcessArguments(
                   console_options, &rejected_arguments, &error) &&
               error.find("active console without a device policy") != std::string::npos;
    }();

    // Hardlock material is applied at the device boundary, so a profile that
    // expects it without that boundary is rejected rather than run without it.
    const bool invalid_material_policy = [&]() {
        re2dj::platform::windows::OriginalProcessOptions material_options = options;
        material_options.profile_defaults.lptdi.legacy_io_ports = false;
        material_options.profile_defaults.lptdi.legacy_io_ports_default = false;
        material_options.profile_defaults.lptdi.legacy_io_in_byte_rva = 0;
        material_options.profile_defaults.lptdi.legacy_io_out_byte_rva = 0;
        material_options.profile_defaults.lptdi.device_mock_enabled = false;
        material_options.profile_defaults.lptdi.device_mock_path_prefix.clear();
        material_options.profile_defaults.hle_wts_active_console = false;
        material_options.profile_defaults.lptdi.hardlock_cfg_material_default = true;
        std::vector<std::string> rejected_arguments;
        return !re2dj::platform::windows::BuildOriginalProcessArguments(
                   material_options, &rejected_arguments, &error) &&
               error.find("Hardlock material without a device policy") != std::string::npos;
    }();

    options.target_id = "ez2dj1stse_unpacked";
    options.hle_profile_id = "ez2dj1stse_unpacked";
    options.profile_defaults = {};
    options.chd_image.clear();
    options.executable_relative_path.clear();
    const bool rejected =
        !re2dj::platform::windows::BuildOriginalProcessArguments(
            options, &arguments, &error) &&
        error.find("invalid Windows original-process options") != std::string::npos;
    const bool resolve_iat_slot = TestResolveIatSlot();
    if (!canonical || !custom_gain || !custom_demo_volume || !audio_trace || !fullscreen ||
        !invalid_gain || !invalid_demo_volume || !io_config || !invalid_lptdi_policy ||
        !third_defaults || !chd_handoff || !no_diagnostic_options ||
        !invalid_console_policy || !invalid_material_policy || !rejected || !resolve_iat_slot)
    {
        // Naming the failed checks keeps a policy change from producing an
        // error message that describes only the last call made.
        std::fprintf(stderr,
                     "windows-product-loader-probe: failed%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s (last error: %s)\n",
                     canonical ? "" : " canonical",
                     custom_gain ? "" : " custom-gain",
                     custom_demo_volume ? "" : " custom-demo-volume",
                     audio_trace ? "" : " audio-trace",
                     fullscreen ? "" : " fullscreen",
                     invalid_gain ? "" : " invalid-gain",
                     invalid_demo_volume ? "" : " invalid-demo-volume",
                     io_config ? "" : " io-config",
                     invalid_lptdi_policy ? "" : " invalid-lptdi-policy",
                     third_defaults ? "" : " third-defaults",
                     chd_handoff ? "" : " chd-handoff",
                     no_diagnostic_options ? "" : " no-diagnostic-options",
                     invalid_console_policy ? "" : " invalid-console-policy",
                     invalid_material_policy ? "" : " invalid-material-policy",
                     rejected ? "" : " rejected",
                     resolve_iat_slot ? "" : " resolve-iat-slot",
                     error.c_str());
        return 1;
    }
    std::printf("windows-product-loader-probe: profile-defaults=ok unsupported-target=ok resolve-iat-slot=ok\n");
    return 0;
}
