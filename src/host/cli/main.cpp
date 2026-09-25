// Command-line host for re2DJ.
//
// The original HDD contents arrive as a directory path, never as a disk image
// and never from a fixed location inside the repository. Everything this entry
// point does is orchestration: it validates the directory, scans it, resolves a
// target profile, and enters an available platform execution backend.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <spdlog/logger.h>

#include "re2dj/exe/pe_image.h"
#include "re2dj/hdd/hdd_root.h"
#include "re2dj/hdd/hdd_scan.h"
#include "re2dj/storage/guest_path.h"
#include "re2dj/storage/fat32_chd.h"
#include "re2dj/graphics/present_sync.h"
#include "re2dj/logging/logging.h"
#include "re2dj/target/target_profile.h"
#include "re2dj/version.h"

#if defined(__linux__)
#include "re2dj/config/hardlock_secret_config.h"
#include "re2dj/hle/guest_devices.h"
#include "re2dj/hle/hardlock/device_material.h"
#include "re2dj/platform/linux/host_presentation.h"
#include "re2dj/platform/linux/original_runner.h"
#elif defined(_WIN32)
#include "re2dj/platform/windows/original_process_backend.h"
#endif

namespace
{

// Exit codes. Kept distinct so scripts can tell a bad invocation from a bad
// dump from a feature that simply is not built yet.
constexpr int kExitOk = 0;
constexpr int kExitUsage = 1;
constexpr int kExitHddError = 2;
constexpr int kExitNotImplemented = 3;
constexpr int kExitLoggingError = 4;

struct LoggingLifetime
{
    ~LoggingLifetime()
    {
        re2dj::logging::Shutdown();
    }
};

std::string FormatLogMessage(const char* format, va_list arguments)
{
    va_list sizing_arguments;
    va_copy(sizing_arguments, arguments);
    const int length = std::vsnprintf(nullptr, 0, format, sizing_arguments);
    va_end(sizing_arguments);
    if (length <= 0)
    {
        return {};
    }
    std::string message(static_cast<std::size_t>(length) + 1, '\0');
    std::vsnprintf(message.data(), message.size(), format, arguments);
    message.resize(static_cast<std::size_t>(length));
    while (!message.empty() && (message.back() == '\n' || message.back() == '\r'))
    {
        message.pop_back();
    }
    return message;
}

void LogError(const char* format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    const std::string message = FormatLogMessage(format, arguments);
    va_end(arguments);
    re2dj::logging::GetLogger()->error("{}", message);
}

// Run output goes through the logger, so it reaches the console and the log
// file alike; only --help, --version, and usage text stay on stdout.
void LogInfo(const char* format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    const std::string message = FormatLogMessage(format, arguments);
    va_end(arguments);
    re2dj::logging::GetLogger()->info("{}", message);
}

// Lowercase hex of bytes, as one run.
std::string HexBytes(const std::uint8_t* bytes, std::size_t count)
{
    std::string text;
    text.reserve(count * 2);
    char pair[3] = {};
    for (std::size_t index = 0; index < count; ++index)
    {
        std::snprintf(pair, sizeof(pair), "%02x", static_cast<unsigned>(bytes[index]));
        text += pair;
    }
    return text;
}

void LogFatal(const char* classification, const char* format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    const std::string message = FormatLogMessage(format, arguments);
    va_end(arguments);
    re2dj::logging::Fatal(classification, message);
}

struct Options
{
    std::filesystem::path hdd_directory;
    bool linux_in_process_first_import = false;
    bool linux_in_process_first_resolver = false;
    bool linux_in_process_getversion_call = false;
    bool linux_in_process_createfile_call = false;
    bool linux_in_process_continue = false;
    bool hold_window = false;
    std::filesystem::path io_config;
    std::string target_id;
    std::string resolve_path;
    float audio_gain_db = 0.0f;
    unsigned demo_volume = 3;
    bool audio_gain_explicit = false;
    bool demo_volume_explicit = false;
    bool fullscreen_explicit = false;
    bool present_sync_explicit = false;
    bool audio_volume_trace = false;
    bool guest_wait_trace = false;
    bool image_dump = false;
    unsigned image_dump_delay_ms = 0;
    bool fullscreen = false;
    re2dj::graphics::PresentSync present_sync = re2dj::graphics::PresentSync::kVerticalSync;
    bool list_targets = false;
    bool run = false;
    bool positional_target = false;
    bool target_option_explicit = false;
    bool show_help = false;
    bool show_version = false;
};

bool FindChdImage(const std::filesystem::path& input,
                  std::filesystem::path* image,
                  std::string* error)
{
    if (image == nullptr || error == nullptr || input.empty())
    {
        if (error != nullptr)
        {
            *error = "CHD path is empty";
        }
        return false;
    }
    std::error_code code;
    if (std::filesystem::is_regular_file(input, code))
    {
        if (re2dj::storage::EqualsIgnoreAsciiCase(input.extension().string(), ".chd"))
        {
            *image = std::filesystem::weakly_canonical(input, code);
            if (code)
            {
                *image = input;
            }
            return true;
        }
        *error = "CHD input is a regular file but does not have a .chd extension";
        return false;
    }
    if (code || !std::filesystem::is_directory(input, code))
    {
        *error = "CHD input directory does not exist: " + input.string();
        return false;
    }
    std::vector<std::filesystem::path> candidates;
    for (std::filesystem::directory_iterator iterator(input, code), end;
         !code && iterator != end;
         iterator.increment(code))
    {
        if (!iterator->is_regular_file(code) || code)
        {
            continue;
        }
        const std::string extension = iterator->path().extension().string();
        if (re2dj::storage::EqualsIgnoreAsciiCase(extension, ".chd"))
        {
            candidates.push_back(iterator->path());
        }
    }
    if (code || candidates.empty())
    {
        *error = "no .chd image was found under " + input.string();
        return false;
    }
    std::sort(candidates.begin(), candidates.end());
    if (candidates.size() > 1)
    {
        *error = "more than one .chd image was found under " + input.string();
        return false;
    }
    *image = std::filesystem::weakly_canonical(candidates.front(), code);
    if (code)
    {
        *image = candidates.front();
    }
    return true;
}

bool PrepareChdStaging(const re2dj::storage::Fat32Volume& volume,
                       std::string_view profile_id,
                       const std::string& executable_relative_path,
                       std::filesystem::path* staging_root,
                       std::string* error)
{
    if (staging_root == nullptr || error == nullptr)
    {
        if (error != nullptr)
        {
            *error = "invalid CHD staging output";
        }
        return false;
    }
    std::error_code code;
    const auto base = std::filesystem::temp_directory_path(code);
    if (code)
    {
        *error = "cannot determine temporary directory for CHD staging";
        return false;
    }
    const std::filesystem::path root = base / "re2dj" / "chd" / profile_id;
    re2dj::storage::GuestPath parsed;
    if (!re2dj::storage::ParseGuestPath(executable_relative_path, &parsed) ||
        parsed.kind != re2dj::storage::GuestPathKind::kRelative ||
        !re2dj::storage::NormalizeGuestPath(&parsed) || parsed.components.empty())
    {
        *error = "invalid CHD executable path: " + executable_relative_path;
        return false;
    }
    std::filesystem::path executable_output = root;
    for (const std::string& component : parsed.components)
    {
        executable_output /= component;
    }
    std::string materialize_error;
    if (!volume.MaterializeFile(
            executable_relative_path, executable_output, &materialize_error))
    {
        *error = executable_relative_path + ": " + materialize_error;
        return false;
    }
    if (profile_id == "ez2dj6th")
    {
        const std::string child_rel = "EZ2DJ/EZ2DJ6th.EXE";
        const std::filesystem::path child_output = root / "EZ2DJ" / "EZ2DJ6th.EXE";
        if (!volume.MaterializeFile(child_rel, child_output, &materialize_error))
        {
            *error = child_rel + ": " + materialize_error;
            return false;
        }
    }
    *staging_root = root;
    return true;
}

int ResolveOneChdPath(const re2dj::storage::Fat32Volume& volume,
                      const std::string& text)
{
    re2dj::storage::GuestPath parsed;
    if (!re2dj::storage::ParseGuestPath(text, &parsed) ||
        !re2dj::storage::NormalizeGuestPath(&parsed))
    {
        LogError("cannot parse guest path '%s'", text.c_str());
        return kExitUsage;
    }
    if (parsed.drive_letter != 'D' || parsed.components.empty() ||
        !re2dj::storage::EqualsIgnoreAsciiCase(parsed.components.front(), "ez2dj"))
    {
        LogError("CHD guest path must be under D:\\ez2dj");
        return kExitUsage;
    }
    const std::string guest_path = re2dj::storage::GuestPathToString(parsed);
    parsed.components.erase(parsed.components.begin());
    const std::string relative = "EZ2DJ/" +
                                  re2dj::storage::GuestPathToRelativeString(parsed);
    re2dj::storage::Fat32Entry entry;
    std::string error;
    const bool found = volume.Find(relative, &entry, &error);
    LogInfo("guest path : %s", guest_path.c_str());
    LogInfo("relative   : %s", relative.c_str());
    LogInfo("chd path   : %s", found ? relative.c_str() : "<not found>");
    return found ? kExitOk : kExitHddError;
}

void PrintUsage()
{
    std::printf(
        "%s - run the original EZ2DJ executable on modern hosts\n"
        "\n"
        "Usage:\n"
        "  re2dj <profile-id> [options]\n"
        "  re2dj --hdd <directory> [options]\n"
        "\n"
        "Options:\n"
        "  --hdd <directory>   Extracted original HDD contents. For a CHD profile,\n"
        "                      this may instead be a directory containing one .chd.\n"
        "  --target <id>       Target profile to select. Defaults to the first\n"
        "                      detected candidate.\n"
        "  --list-targets      List target profiles found in the directory.\n"
        "  --resolve <path>    Resolve one guest path (for example\n"
        "                      \"C:\\\\EZ2DJ\\\\DATA\\\\SONG.EZ\") and exit.\n"
        "  --run               Start the selected guest executable.\n"
        "  --hold-window       Linux: keep the guest's window open after the run stops,\n"
        "                      until it is closed.\n"
        "  --linux-in-process-first-import\n"
        "                      Linux diagnostic: complete only the first import in-process.\n"
        "  --linux-in-process-first-resolver\n"
        "                      Linux diagnostic: observe first dynamic GetProcAddress request.\n"
        "  --linux-in-process-getversion-call\n"
        "                      Linux diagnostic: observe the resolved GetVersion thunk call.\n"
        "  --linux-in-process-createfile-call\n"
        "                      Linux diagnostic: observe the resolved CreateFileA thunk call.\n"
        "  --linux-in-process-continue\n"
        "                      Linux diagnostic: run on the kernel32 facade until the\n"
        "                      first unhandled import, unresolved lookup, or fault.\n"
        "  --audio-gain-db <dB>\n"
        "                      Windows output gain (-24..+18, default 0).\n"
        "  --demo-volume <0..3>\n"
        "                      Windows title/demo profile (default 3 = 0 dB).\n"
        "  --guest-wait-trace  Account the guest's Sleep, WaitForSingleObject, and\n"
        "                      timeGetTime calls per frame window (diagnostic).\n"
        "  --image-dump        Save the decrypted main image at the restored entry and\n"
        "                      again after the guest has run (diagnostic; tens of MB).\n"
        "  --image-dump-delay <milliseconds>\n"
        "                      Wait before the second image dump (default 5000).\n"
        "  --audio-volume-trace\n"
        "                      Record bounded DirectSound/WINMM volume evidence.\n"
        "  --fullscreen        Use monitor-sized borderless fullscreen on Windows.\n"
        "  --windowed          Override a profile's fullscreen default on Windows.\n"
        "  --vsync <on|off|adaptive>\n"
        "                      When a present returns. 'on' waits for the display's\n"
        "                      refresh (default), 'off' never waits and allows\n"
        "                      tearing, 'adaptive' waits only for frames that met\n"
        "                      the deadline. A driver may refuse 'adaptive'.\n"
        "  --io-config <path>  Windows keyboard I/O mapping INI for the selected target.\n"
        "                      Overrides only the entries it lists; the built-in\n"
        "                      mapping covers the rest.\n"
        "  --version           Print the version and exit.\n"
        "  --help              Print this message and exit.\n"
        "\n"
        "The HDD directory is read only. Supported execution paths route\n"
        "guest writes to a separate overlay directory.\n",
        re2dj::VersionBanner("re2DJ", re2dj::VersionString()).c_str());
}

#if defined(__linux__)
void PrintFaultObservation(const re2dj::platform::linux::OriginalRunResult& result)
{
    const auto& fault = result.fault_observation;
    if (!fault.observed)
    {
        return;
    }
    LogInfo("fault address   : 0x%08x, signal code=%u, cpu error=0x%08x",
                fault.fault_address.value(), fault.signal_code, fault.cpu_error_code);
    LogInfo("fault registers : eax=%08x ebx=%08x ecx=%08x edx=%08x",
                fault.eax, fault.ebx, fault.ecx, fault.edx);
    LogInfo("                  esi=%08x edi=%08x ebp=%08x eflags=%08x",
                fault.esi, fault.edi, fault.ebp, fault.eflags);
    if (fault.instruction_window_observed)
    {
        const std::uint32_t instruction_offset =
            result.instruction_pointer.value() - fault.instruction_window_address.value();
        LogInfo("fault code      : 0x%08x (EIP +%u)",
                    fault.instruction_window_address.value(),
                    instruction_offset);
        for (std::size_t index = 0; index < fault.instruction_window.size(); index += 16)
        {
            const std::size_t count = std::min<std::size_t>(16, fault.instruction_window.size() - index);
            LogInfo("                  %s",
                    HexBytes(fault.instruction_window.data() + index, count).c_str());
        }
    }
    if (fault.stack_words_observed)
    {
        std::string words;
        char word[10] = {};
        for (std::uint32_t index = 0; index < fault.stack_word_count; ++index)
        {
            std::snprintf(word, sizeof(word), " %08x", fault.stack_words[index]);
            words += word;
        }
        LogInfo("fault stack     :%s", words.c_str());
    }
    if (fault.fs_base.value() != 0)
    {
        if (fault.seh_frame_observed)
        {
            LogInfo("fault seh       : teb=0x%08x frame=0x%08x handler=0x%08x next=0x%08x",
                        fault.fs_base.value(),
                        fault.seh_frame_address.value(),
                        fault.seh_handler.value(),
                        fault.seh_next);
            if (fault.seh_handler_window_observed)
            {
                LogInfo("seh handler code: 0x%08x", fault.seh_handler.value());
                for (std::size_t index = 0; index < fault.seh_handler_window.size(); index += 16)
                {
                    const std::size_t count =
                        std::min<std::size_t>(16, fault.seh_handler_window.size() - index);
                    LogInfo("                  %s",
                            HexBytes(fault.seh_handler_window.data() + index, count).c_str());
                }
            }
        }
        else
        {
            LogInfo("fault seh       : teb=0x%08x frame=0x%08x (no frame registered)",
                        fault.fs_base.value(),
                        fault.seh_frame_address.value());
        }
    }
}

void PrintInstructionTrace(const re2dj::platform::linux::OriginalRunResult& result)
{
    const auto& trace = result.instruction_trace;
    if (!trace.armed)
    {
        return;
    }
    LogInfo("instruction trace: breakpoint=0x%08x started=%u frames=%u limit=%u",
                trace.breakpoint.value(),
                trace.started ? 1U : 0U,
                trace.frame_count,
                trace.limit_reached ? 1U : 0U);
    for (std::uint32_t index = 0; index < trace.frame_count; ++index)
    {
        const auto& frame = trace.frames[index];
        LogInfo("  #%03u eip=%08x esp=%08x eax=%08x ebx=%08x ecx=%08x edx=%08x",
                    index,
                    frame.instruction_pointer.value(),
                    frame.stack_pointer.value(),
                    frame.eax,
                    frame.ebx,
                    frame.ecx,
                    frame.edx);
        LogInfo("       esi=%08x edi=%08x ebp=%08x eflags=%08x",
                    frame.esi,
                    frame.edi,
                    frame.ebp,
                    frame.eflags);
    }
}

void PrintCreateFileObservation(const re2dj::platform::linux::OriginalRunResult& result)
{
    const auto& observation = result.create_file_observation;
    if (!observation.observed)
    {
        return;
    }
    LogInfo("CreateFileA name : %s (0x%08x)",
                observation.file_name_observed ? observation.file_name.c_str() : "<unavailable>",
                observation.file_name_address.value());
    LogInfo("access / share   : 0x%08x / 0x%08x",
                observation.desired_access,
                observation.share_mode);
    LogInfo("security / disp  : 0x%08x / 0x%08x",
                observation.security_attributes.value(),
                observation.creation_disposition);
    LogInfo("flags / template : 0x%08x / 0x%08x",
                observation.flags_and_attributes,
                observation.template_file.value());
}

const char* DescribeIdentity(re2dj::runtime::GuestAddress registry,
                             re2dj::runtime::GuestAddress guest)
{
    return guest.value() == 0 ? " (never requested)"
           : guest == registry ? " (match)"
                               : " (MISMATCH)";
}

void PrintResolverIdentity(const re2dj::platform::linux::OriginalRunResult& result)
{
    const auto& identity = result.resolver_identity;
    if (identity.prepared)
    {
        LogInfo("kernel32 facade  : registry 0x%08x, guest saw 0x%08x%s",
                    identity.registry_kernel32_base.value(),
                    identity.kernel32_base.value(),
                    DescribeIdentity(identity.registry_kernel32_base, identity.kernel32_base));
        LogInfo("CreateFileA addr : registry 0x%08x, static IAT 0x%08x, guest saw 0x%08x%s%s",
                    identity.registry_create_file.value(),
                    identity.static_create_file_slot.value(),
                    identity.create_file_address.value(),
                    DescribeIdentity(identity.registry_create_file, identity.create_file_address),
                    identity.static_imports_rebound ? "" : " (no static rebind)");
        LogInfo("GetVersion addr  : registry 0x%08x, guest saw 0x%08x%s",
                    identity.registry_get_version.value(),
                    identity.get_version_address.value(),
                    DescribeIdentity(identity.registry_get_version, identity.get_version_address));
    }
    if (result.unhandled_import_observed)
    {
        LogInfo("unhandled import : %s", result.unhandled_import.c_str());
    }
    if (result.unhandled_dynamic_request_observed)
    {
        LogInfo("unresolved lookup: %s",
                    result.unhandled_dynamic_request.c_str());
    }
}

// A guest ANSI string in quotes, preceded by a space. Guest text may be CP949
// rather than UTF-8, so bytes outside printable ASCII appear as \xNN, keeping
// the original bytes recoverable instead of guessing an encoding.
std::string QuoteGuestText(const std::string& text)
{
    std::string quoted = " \"";
    char escape[5] = {};
    for (const char character : text)
    {
        const auto byte = static_cast<unsigned char>(character);
        if (byte < 0x20 || byte > 0x7E)
        {
            std::snprintf(escape, sizeof(escape), "\\x%02x", byte);
            quoted += escape;
        }
        else
        {
            quoted.push_back(character);
        }
    }
    quoted.push_back('"');
    return quoted;
}

void PrintApiCalls(const re2dj::platform::linux::OriginalRunResult& result)
{
    if (result.api_call_count > result.api_calls.size())
    {
        LogInfo("api calls       : %u (first and last %zu logged)",
                result.api_call_count,
                result.api_calls.size());
    }
    else
    {
        LogInfo("api calls       : %u", result.api_call_count);
    }
    std::uint32_t previous_sequence = 0;
    char part[48] = {};
    for (const auto& call : result.api_calls)
    {
        if (call.sequence > previous_sequence + 1)
        {
            LogInfo("  ... %u calls omitted", call.sequence - previous_sequence - 1);
        }
        previous_sequence = call.sequence;
        std::string arguments;
        for (std::uint32_t index = 0; index < call.argument_count; ++index)
        {
            std::snprintf(part, sizeof(part), index == 0 ? " args=%08x" : ",%08x", call.arguments[index]);
            arguments += part;
        }
        if (call.text_observed)
        {
            arguments += QuoteGuestText(call.text);
        }
        if (call.second_text_observed)
        {
            arguments += QuoteGuestText(call.second_text);
        }
        if (call.handled)
        {
            std::snprintf(part, sizeof(part), " -> eax=%08x", call.eax);
        }
        else
        {
            std::snprintf(part, sizeof(part), " -> UNHANDLED");
        }
        LogInfo("  #%04u %-32s ret=%08x%s%s",
                call.sequence,
                call.name.c_str(),
                call.return_address,
                arguments.c_str(),
                part);
    }
}

bool PrintContinuationBoundary(const re2dj::platform::linux::OriginalRunResult& result)
{
    using re2dj::platform::linux::OriginalRunBoundary;
    switch (result.boundary)
    {
    case OriginalRunBoundary::kContinuationUnhandledImport:
        LogInfo("continuation    : stopped at unhandled import %s, return 0x%08x",
                    result.continuation_stop_detail.c_str(),
                    result.import_return_address);
        break;
    case OriginalRunBoundary::kContinuationUnresolvedLookup:
        LogInfo("continuation    : stopped at unresolved lookup %s, return 0x%08x",
                    result.continuation_stop_detail.c_str(),
                    result.import_return_address);
        break;
    case OriginalRunBoundary::kContinuationCallLimit:
        LogInfo("continuation    : stopped at the call limit after %s, return 0x%08x",
                    result.continuation_stop_detail.c_str(),
                    result.import_return_address);
        break;
    case OriginalRunBoundary::kContinuationFault:
        LogInfo("continuation    : guest fault signal %u, EIP 0x%08x",
                    result.status_code,
                    result.instruction_pointer.value());
        PrintFaultObservation(result);
        break;
    case OriginalRunBoundary::kProcessExit:
        LogInfo("continuation    : process exit, %s",
                    result.continuation_stop_detail.c_str());
        break;
    default:
        return false;
    }
    PrintResolverIdentity(result);
    return true;
}

// Whether a Linux --run takes the in-process continuation: named explicitly,
// or by default when no other in-process diagnostic was requested.
bool IsLinuxContinuationRun(const Options& options)
{
    return options.linux_in_process_continue ||
           (!options.linux_in_process_first_import && !options.linux_in_process_first_resolver &&
            !options.linux_in_process_getversion_call && !options.linux_in_process_createfile_call);
}

// The devices a Linux in-process run provides: the profile's device path and,
// when the profile allows it, the user's Hardlock material from cfg. Nothing
// derived from that material is printed or logged.
bool BuildLinuxGuestDevices(const re2dj::target::TargetProfile& profile,
                            re2dj::hle::GuestDeviceConfig* config,
                            std::string* error)
{
    namespace hardlock = re2dj::hle::hardlock;
    *config = {};
    const re2dj::target::TargetLptdiPolicy& lptdi = profile.run_defaults.lptdi;
    if (!lptdi.device_mock_enabled)
    {
        return true;
    }
    config->device_path_prefix = lptdi.device_mock_path_prefix;
    if (!lptdi.hardlock_cfg_material_default)
    {
        return true;
    }
    hardlock::HardlockMaterialSources sources;
    sources.profile_id = profile.id;
    sources.use_profile_cfg = true;
    sources.config_path = re2dj::config::DefaultHardlockSecretConfigPath();
    sources.default_map_path = re2dj::config::DefaultHardlockTransformMapPath(profile.id);
    hardlock::HardlockDeviceMaterial material;
    if (!hardlock::ResolveHardlockDeviceMaterial(sources, &material, error))
    {
        return false;
    }
    if (material.device_enabled)
    {
        config->hardlock = hardlock::MakeHardlockDeviceOptions(material);
    }
    return true;
}

void PrintDeviceActivity(const re2dj::platform::linux::OriginalRunResult& result)
{
    const re2dj::hle::hardlock::HardlockDeviceActivity& activity = result.device_activity;
    LogInfo("hardlock material: %s", result.hardlock_material_applied ? "applied" : "none");
    LogInfo("hardlock requests: total=%u initialize=%u handshake=%u descriptor=%u "
                "transform=%u other=%u rejected=%u last=%s/%s",
                activity.total,
                activity.initialize,
                activity.handshake,
                activity.descriptor,
                activity.transform,
                activity.other,
                activity.rejected,
                activity.last_kind,
                activity.last_outcome);
}

// The host window a Linux run shows the guest's window in, made when the
// guest takes the display and kept for the process's life.
std::unique_ptr<re2dj::platform::linux::LinuxHostPresentation> g_linux_presentation;

// Keeps a Linux run's window on screen after everything else is reported,
// whichever way main returns, when --hold-window asked for it.
struct LinuxWindowHold
{
    bool armed = false;
    ~LinuxWindowHold()
    {
        if (armed && g_linux_presentation != nullptr && g_linux_presentation->opened())
        {
            LogInfo("host window     : kept open until it is closed (--hold-window)");
            g_linux_presentation->HoldUntilClosed();
        }
    }
};

// Runs the guest on Linux. Both host widths execute it in this process on the
// guest facades.
bool RunLinuxOriginal(const Options& options,
                      const re2dj::target::TargetProfile& profile,
                      const std::filesystem::path& executable_path,
                      const re2dj::exe::PeImageInfo& image_info,
                      const std::filesystem::path& chd_image,
                      re2dj::platform::linux::OriginalRunResult* result,
                      std::string* error)
{
    namespace linux_platform = re2dj::platform::linux;
    if (IsLinuxContinuationRun(options))
    {
        linux_platform::OriginalRunEnvironment environment;
        if (!BuildLinuxGuestDevices(profile, &environment.devices, error))
        {
            return false;
        }
        environment.module_path = re2dj::target::GuestExecutablePath(profile);
        if (g_linux_presentation == nullptr)
        {
            g_linux_presentation = std::make_unique<linux_platform::LinuxHostPresentation>();
        }
        environment.presentation = g_linux_presentation.get();
        // Guest files come from the CHD, with writes in overlays/<profile>
        // as on the Windows path; a directory dump provides none yet.
        if (!chd_image.empty())
        {
            environment.files.chd_image = chd_image;
            environment.files.chd_root =
                std::filesystem::path(profile.executable_relative_path).parent_path().generic_string();
            environment.files.guest_root = re2dj::target::GuestRootPath(profile);
            environment.files.overlay_root = std::filesystem::current_path() / "overlays" / profile.id;
        }
        return linux_platform::RunOriginalInProcessContinuation(
            executable_path, image_info, environment, result, error);
    }
    if (options.linux_in_process_createfile_call)
    {
        return linux_platform::RunOriginalInProcessCreateFileCall(
            executable_path, image_info, result, error);
    }
    if (options.linux_in_process_getversion_call)
    {
        return linux_platform::RunOriginalInProcessGetVersionCall(
            executable_path, image_info, result, error);
    }
    if (options.linux_in_process_first_resolver)
    {
        return linux_platform::RunOriginalInProcessFirstResolver(
            executable_path, image_info, result, error);
    }
    return linux_platform::RunOriginalInProcessFirstImport(executable_path, image_info, result, error);
}
#endif

bool TakeValue(int argc, char** argv, int* index, std::string_view name, std::string* out)
{
    if (*index + 1 >= argc)
    {
        LogError("%s requires a value", std::string(name).c_str());
        return false;
    }
    ++(*index);
    *out = argv[*index];
    return true;
}

bool ParseOptions(int argc, char** argv, Options* options)
{
    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument = argv[index];
        if (argument == "--help" || argument == "-h")
        {
            options->show_help = true;
        }
        else if (argument == "--version")
        {
            options->show_version = true;
        }
        else if (argument == "--list-targets")
        {
            options->list_targets = true;
        }
        else if (argument == "--run")
        {
            options->run = true;
        }
        else if (argument == "--linux-in-process-first-import")
        {
            options->linux_in_process_first_import = true;
        }
        else if (argument == "--linux-in-process-first-resolver")
        {
            options->linux_in_process_first_resolver = true;
        }
        else if (argument == "--linux-in-process-getversion-call")
        {
            options->linux_in_process_getversion_call = true;
        }
        else if (argument == "--linux-in-process-createfile-call")
        {
            options->linux_in_process_createfile_call = true;
        }
        else if (argument == "--linux-in-process-continue")
        {
            options->linux_in_process_continue = true;
        }
        else if (argument == "--hold-window")
        {
            options->hold_window = true;
        }
        else if (argument == "--hdd")
        {
            std::string value;
            if (!TakeValue(argc, argv, &index, argument, &value))
            {
                return false;
            }
            options->hdd_directory = std::filesystem::path(value);
        }
        else if (argument == "--audio-gain-db")
        {
            std::string value;
            if (!TakeValue(argc, argv, &index, argument, &value))
            {
                return false;
            }
            try
            {
                std::size_t parsed = 0;
                options->audio_gain_db = std::stof(value, &parsed);
                if (parsed != value.size() || !std::isfinite(options->audio_gain_db) ||
                    options->audio_gain_db < -24.0f || options->audio_gain_db > 18.0f)
                {
                    throw std::out_of_range("audio gain");
                }
            }
            catch (const std::exception&)
            {
                LogError("--audio-gain-db must be between -24 and +18 dB");
                return false;
            }
            options->audio_gain_explicit = true;
        }
        else if (argument == "--audio-volume-trace")
        {
            options->audio_volume_trace = true;
        }
        else if (argument == "--demo-volume")
        {
            std::string value;
            if (!TakeValue(argc, argv, &index, argument, &value))
            {
                return false;
            }
            try
            {
                std::size_t parsed = 0;
                const unsigned long parsed_value = std::stoul(value, &parsed);
                if (parsed != value.size() || parsed_value > 3)
                {
                    throw std::out_of_range("demo volume");
                }
                options->demo_volume = static_cast<unsigned>(parsed_value);
            }
            catch (const std::exception&)
            {
                LogError("--demo-volume must be between 0 and 3");
                return false;
            }
            options->demo_volume_explicit = true;
        }
        else if (argument == "--guest-wait-trace")
        {
            options->guest_wait_trace = true;
        }
        else if (argument == "--image-dump")
        {
            options->image_dump = true;
        }
        else if (argument == "--image-dump-delay" && index + 1 < argc)
        {
            options->image_dump_delay_ms =
                static_cast<unsigned>(std::strtoul(argv[++index], nullptr, 10));
            options->image_dump = true;
        }
        else if (argument == "--fullscreen")
        {
            options->fullscreen = true;
            options->fullscreen_explicit = true;
        }
        else if (argument == "--windowed")
        {
            options->fullscreen = false;
            options->fullscreen_explicit = true;
        }
        else if (argument == "--vsync")
        {
            std::string value;
            if (!TakeValue(argc, argv, &index, argument, &value))
            {
                return false;
            }
            if (value == "on")
            {
                options->present_sync = re2dj::graphics::PresentSync::kVerticalSync;
            }
            else if (value == "off")
            {
                options->present_sync = re2dj::graphics::PresentSync::kImmediate;
            }
            else if (value == "adaptive")
            {
                options->present_sync = re2dj::graphics::PresentSync::kAdaptive;
            }
            else
            {
                LogError("--vsync must be on, off or adaptive");
                return false;
            }
            options->present_sync_explicit = true;
        }
        else if (argument == "--io-config")
        {
            std::string value;
            if (!TakeValue(argc, argv, &index, argument, &value))
            {
                return false;
            }
            std::error_code ec;
            const auto absolute_path = std::filesystem::absolute(value, ec);
            options->io_config = ec ? std::filesystem::path(value) : absolute_path;
        }
        else if (argument == "--target")
        {
            if (!TakeValue(argc, argv, &index, argument, &options->target_id))
            {
                return false;
            }
            options->target_option_explicit = true;
        }
        else if (argument == "--resolve")
        {
            if (!TakeValue(argc, argv, &index, argument, &options->resolve_path))
            {
                return false;
            }
        }
        else if (!argument.empty() && argument.front() != '-')
        {
            if (options->positional_target)
            {
                LogError("only one profile id may be specified");
                return false;
            }
            options->positional_target = true;
            if (!options->target_option_explicit)
            {
                options->target_id = std::string(argument);
            }
            options->run = true;
        }
        else
        {
            LogError("unknown argument '%s'", argv[index]);
            return false;
        }
    }
    return true;
}

void PrintProfile(const re2dj::target::TargetProfile& profile, bool selected)
{
    LogInfo("  %c %-22s %-24s %s%s",
                selected ? '*' : ' ',
                profile.id.c_str(),
                profile.executable_relative_path.c_str(),
                profile.detected ? "detected" : "built-in",
                profile.bring_up_target ? ", bring-up only" : "");
}

#if defined(_WIN32)
std::filesystem::path NormalizeIoConfigForProfile(
    const std::filesystem::path& io_config,
    const re2dj::target::TargetRunDefaults& defaults,
    std::string_view profile_id)
{
    if (!io_config.empty() && !defaults.lptdi.legacy_io_ports)
    {
        re2dj::logging::GetLogger()->warn(
            "--io-config is ignored for profile '{}' because legacy I/O is disabled",
            profile_id);
        return {};
    }
    return io_config;
}

#endif

int ResolveOnePath(const re2dj::hdd::HddRoot& root, const std::string& text)
{
    re2dj::storage::GuestPath parsed;
    if (!re2dj::storage::ParseGuestPath(text, &parsed))
    {
        LogError("cannot parse guest path '%s'", text.c_str());
        return kExitUsage;
    }
    if (!re2dj::storage::NormalizeGuestPath(&parsed))
    {
        LogError("guest path '%s' escapes the HDD directory", text.c_str());
        return kExitUsage;
    }

    const std::string relative = re2dj::storage::GuestPathToRelativeString(parsed);
    std::filesystem::path resolved;
    if (!root.Resolve(relative, &resolved))
    {
        LogInfo("guest path : %s", re2dj::storage::GuestPathToString(parsed).c_str());
        LogInfo("relative   : %s", relative.c_str());
        LogInfo("host path  : <not found>");
        return kExitHddError;
    }

    LogInfo("guest path : %s", re2dj::storage::GuestPathToString(parsed).c_str());
    LogInfo("relative   : %s", relative.c_str());
    LogInfo("host path  : %s", resolved.string().c_str());
    return kExitOk;
}

int RunChdTarget(const Options& options,
                 const std::filesystem::path& chd_path,
                 const re2dj::target::BuiltInTargetProfile& built_in)
{
    std::unique_ptr<re2dj::storage::Fat32Volume> volume;
    std::string error;
    if (!re2dj::storage::Fat32Volume::Open(chd_path, &volume, &error))
    {
        LogError("%s", error.c_str());
        return kExitHddError;
    }
    const std::string_view executable_path = built_in.profile.executable_relative_path;
    if (executable_path.empty())
    {
        LogError("CHD profile has no executable path");
        return kExitHddError;
    }
    re2dj::storage::Fat32Entry executable_entry;
    if (!volume->Find(executable_path, &executable_entry, &error) || executable_entry.directory)
    {
        LogError("CHD does not contain %.*s: %s",
                 static_cast<int>(executable_path.size()),
                 executable_path.data(),
                 error.c_str());
        return kExitHddError;
    }
    std::vector<std::uint8_t> executable_bytes;
    if (!volume->ReadFile(executable_path, &executable_bytes, &error))
    {
        LogError("cannot read %.*s from CHD: %s",
                 static_cast<int>(executable_path.size()),
                 executable_path.data(),
                 error.c_str());
        return kExitHddError;
    }
    re2dj::exe::PeImageInfo executable_info;
    if (!re2dj::exe::ReadPeImageInfo(executable_bytes.data(),
                                     executable_bytes.size(),
                                     &executable_info,
                                     &error) ||
        !re2dj::exe::IsGuestExecutable(executable_info))
    {
        LogError("CHD executable is not a PE32 guest image: %s",
                 error.empty() ? "unexpected executable format" : error.c_str());
        return kExitHddError;
    }

    re2dj::target::TargetProfile profile = built_in.profile;
    profile.working_directory_relative_path =
        std::filesystem::path(profile.executable_relative_path).parent_path().generic_string();
    profile.detected = false;

    LogInfo("chd image   : %s", chd_path.string().c_str());
    LogInfo("filesystem  : FAT32 label=%s data_lba=%llu clusters=%u",
                volume->info().volume_label.c_str(),
                static_cast<unsigned long long>(volume->info().data_lba),
                volume->info().cluster_count);
    LogInfo("scanned     : CHD-backed FAT32 lookup (directory materialization deferred)");
    LogInfo("executables : 1 selected profile executable");
    if (!options.resolve_path.empty())
    {
        return ResolveOneChdPath(*volume, options.resolve_path);
    }

    LogInfo("targets:");
    PrintProfile(profile, true);
    if (options.list_targets)
    {
        return kExitOk;
    }
    LogInfo("selected target : %s", profile.id.c_str());
    LogInfo("display name    : %s", profile.display_name.c_str());
    LogInfo("executable      : %s", profile.executable_relative_path.c_str());
    LogInfo("working dir     : %s", profile.working_directory_relative_path.c_str());
    LogInfo("guest path      : <not known for this dump>");
    LogInfo("format hint     : %s",
                std::string(re2dj::target::ExecutableFormatHintName(profile.format_hint)).c_str());
    LogInfo("machine         : %s",
                std::string(re2dj::exe::MachineName(executable_info.machine)).c_str());
    LogInfo("magic           : %s",
                std::string(re2dj::exe::MagicName(executable_info.magic)).c_str());
    LogInfo("subsystem       : %s",
                std::string(re2dj::exe::SubsystemName(executable_info.subsystem)).c_str());
    LogInfo("image base      : 0x%08llx",
                static_cast<unsigned long long>(executable_info.image_base));
    LogInfo("entry point rva : 0x%08x", executable_info.entry_point_rva);
    const std::string entry_section(re2dj::exe::EntryPointSectionName(executable_info));
    LogInfo("entry section   : %s%s",
                entry_section.empty() ? "<outside every section>" : entry_section.c_str(),
                re2dj::exe::HasEntryPointOutsideTextSection(executable_info)
                    ? "  (outside .text - likely a protection stub)"
                    : "");
    LogInfo("sections        : %u",
                static_cast<unsigned>(executable_info.sections.size()));
    if (!profile.note.empty())
    {
        LogInfo("note: %s", profile.note.c_str());
    }
    if (!options.run)
    {
        LogInfo("Nothing was executed. Pass --run to stage the PE and enter the available execution backend.");
        return kExitOk;
    }

#if defined(_WIN32)
    std::filesystem::path staging_root;
    if (!PrepareChdStaging(
            *volume, profile.id, profile.executable_relative_path, &staging_root, &error))
    {
        LogError("cannot stage CHD executable: %s", error.c_str());
        return kExitHddError;
    }
    re2dj::platform::windows::OriginalProcessOptions run_options;
    run_options.hdd_directory = staging_root;
    run_options.chd_image = chd_path;
    run_options.target_id = profile.id;
    run_options.executable_relative_path = profile.executable_relative_path;
    run_options.hle_profile_id = profile.hle_profile_id;
    run_options.profile_defaults = profile.run_defaults;
    if (options.audio_gain_explicit)
    {
        run_options.profile_defaults.audio_gain_db = options.audio_gain_db;
    }
    if (options.demo_volume_explicit)
    {
        run_options.profile_defaults.demo_volume = options.demo_volume;
    }
    if (options.fullscreen_explicit)
    {
        run_options.profile_defaults.fullscreen = options.fullscreen;
    }
    if (options.present_sync_explicit)
    {
        run_options.profile_defaults.present_sync = options.present_sync;
    }
    if (options.guest_wait_trace)
    {
        run_options.profile_defaults.guest_wait_trace = true;
    }
    if (options.image_dump)
    {
        run_options.profile_defaults.image_dump = true;
        run_options.profile_defaults.image_dump_delay_ms = options.image_dump_delay_ms;
    }
    run_options.audio_volume_trace = options.audio_volume_trace;
    run_options.io_config = NormalizeIoConfigForProfile(
        options.io_config, run_options.profile_defaults, profile.id);
    const int result = re2dj::platform::windows::RunOriginalProcess(run_options, &error);
    if (result < 0)
    {
        LogFatal("EXECUTION_FAILED", "Windows execution failed: %s", error.c_str());
        return kExitNotImplemented;
    }
    return result;
#else
#if defined(__linux__)
    std::filesystem::path staging_root;
    if (!PrepareChdStaging(
            *volume, profile.id, profile.executable_relative_path, &staging_root, &error))
    {
        LogError("cannot stage CHD executable: %s", error.c_str());
        return kExitHddError;
    }
    const std::filesystem::path staged_executable_path =
        staging_root / profile.executable_relative_path;
    re2dj::platform::linux::OriginalRunResult run_result;
    const bool executed = RunLinuxOriginal(
        options, profile, staged_executable_path, executable_info, chd_path, &run_result, &error);
    if (!executed)
    {
        LogFatal("EXECUTION_FAILED", "Linux execution failed: %s", error.c_str());
        return kExitNotImplemented;
    }
    LogInfo("load base       : 0x%08x", run_result.load_base.value());
    LogInfo("entry point     : 0x%08x", run_result.entry_point.value());
    if (IsLinuxContinuationRun(options))
    {
        if (run_result.seh_dispatch_count > 0)
        {
            LogInfo("seh dispatched  : count=%u last_handler=0x%08x resumed_eip=0x%08x",
                        run_result.seh_dispatch_count,
                        run_result.last_seh_handler.value(),
                        run_result.last_seh_resumed_eip.value());
        }
        PrintApiCalls(run_result);
        PrintDeviceActivity(run_result);
    }
    if (run_result.boundary == re2dj::platform::linux::OriginalRunBoundary::kImportGate)
    {
        if (run_result.by_ordinal)
        {
            LogInfo("first boundary  : import %s!#%u",
                        run_result.module.c_str(),
                        static_cast<unsigned>(run_result.ordinal));
        }
        else
        {
            LogInfo("first boundary  : import %s!%s",
                        run_result.module.c_str(),
                        run_result.name.c_str());
        }
        if (run_result.import_stack_observed)
        {
            LogInfo("stack ret / arg0: 0x%08x / 0x%08x",
                        run_result.import_return_address,
                        run_result.import_first_argument);
        }
        if (run_result.import_first_argument_text_observed)
        {
            LogInfo("arg0 text       : %s", run_result.import_first_argument_text.c_str());
        }
    }
    else if (run_result.boundary ==
             re2dj::platform::linux::OriginalRunBoundary::kFirstImportCompleted)
    {
        LogInfo("first completion : return 0x%08x, SIGTRAP EIP 0x%08x",
                    run_result.import_return_address,
                    run_result.instruction_pointer.value());
    }
    else if (run_result.boundary ==
             re2dj::platform::linux::OriginalRunBoundary::kFirstResolverObserved)
    {
        LogInfo("first resolver completion: %s, return 0x%08x, SIGTRAP EIP 0x%08x",
                    run_result.import_first_argument_text.c_str(),
                    run_result.import_return_address,
                    run_result.instruction_pointer.value());
        PrintResolverIdentity(run_result);
    }
    else if (run_result.boundary ==
             re2dj::platform::linux::OriginalRunBoundary::kGetVersionCalled)
    {
        LogInfo("GetVersion call completion: return 0x%08x, SIGTRAP EIP 0x%08x",
                    run_result.import_return_address,
                    run_result.instruction_pointer.value());
        PrintResolverIdentity(run_result);
        PrintInstructionTrace(run_result);
    }
    else if (run_result.boundary ==
             re2dj::platform::linux::OriginalRunBoundary::kGetVersionCallNotReached)
    {
        LogInfo("GetVersion thunk not reached: signal %u, EIP 0x%08x",
                    run_result.status_code,
                    run_result.instruction_pointer.value());
        PrintFaultObservation(run_result);
        PrintInstructionTrace(run_result);
        PrintResolverIdentity(run_result);
        if (run_result.unhandled_dynamic_request_observed)
        {
            LogFatal("HLE_UNIMPLEMENTED",
                     "dynamic request=%s",
                     run_result.unhandled_dynamic_request.c_str());
        }
    }
    else if (run_result.boundary ==
             re2dj::platform::linux::OriginalRunBoundary::kCreateFileCalled)
    {
        LogInfo("CreateFileA call completion: return 0x%08x, SIGTRAP EIP 0x%08x",
                    run_result.import_return_address,
                    run_result.instruction_pointer.value());
        PrintCreateFileObservation(run_result);
        PrintResolverIdentity(run_result);
    }
    else if (run_result.boundary ==
             re2dj::platform::linux::OriginalRunBoundary::kCreateFileCallNotReached)
    {
        LogInfo("CreateFileA thunk not reached: signal %u, EIP 0x%08x",
                    run_result.status_code,
                    run_result.instruction_pointer.value());
        PrintFaultObservation(run_result);
    }
    else if (!PrintContinuationBoundary(run_result))
    {
        LogInfo("first boundary  : terminal status=%u", run_result.status_code);
    }
    return kExitOk;
#else
    LogFatal("EXECUTION_UNSUPPORTED",
             "CHD-backed original-process execution is currently connected only to the Windows x86 launcher");
    return kExitNotImplemented;
#endif
#endif
}

}  // namespace

int main(int argc, char** argv)
{
    LoggingLifetime logging_lifetime;
    re2dj::logging::LoggerOptions logger_options;
    std::filesystem::path log_file;
    std::string logging_error;
    if (!re2dj::logging::Initialize(logger_options, &log_file, &logging_error))
    {
        std::fprintf(stderr, "fatal: cannot initialize logging: %s\n", logging_error.c_str());
        return kExitLoggingError;
    }
    re2dj::logging::GetLogger()->info("{} starting", re2dj::VersionBanner("re2DJ", re2dj::VersionString()));
    re2dj::logging::GetLogger()->info("Log file: {}", log_file.string());
    {
        std::filesystem::path api_log = log_file;
        api_log.replace_extension(".api.log");
        re2dj::logging::GetLogger()->info("API log : {}", api_log.string());
    }

    Options options;
    if (!ParseOptions(argc, argv, &options))
    {
        return kExitUsage;
    }
#if defined(__linux__)
    LinuxWindowHold window_hold;
    window_hold.armed = options.hold_window;
#endif
    if (options.show_version)
    {
        std::printf("%s\n", re2dj::VersionBanner("re2DJ", re2dj::VersionString()).c_str());
        return kExitOk;
    }
    if (options.show_help || argc == 1)
    {
        PrintUsage();
        return options.show_help ? kExitOk : kExitUsage;
    }
#if !defined(_WIN32)
    if (options.audio_gain_explicit || options.demo_volume_explicit ||
        options.audio_volume_trace || options.guest_wait_trace || options.image_dump ||
        options.fullscreen_explicit ||
        options.present_sync_explicit ||
        !options.io_config.empty())
    {
        LogFatal("EXECUTION_UNSUPPORTED",
                 "selected execution options are currently supported only on Windows");
        return kExitNotImplemented;
    }
#endif
    const re2dj::target::BuiltInTargetProfile* shortcut =
        options.target_id.empty()
            ? nullptr
            : re2dj::target::FindBuiltInTargetProfileById(options.target_id);
    if (shortcut != nullptr &&
        shortcut->profile.run_defaults.hdd_input_kind ==
            re2dj::target::HddInputKind::kMameChd)
    {
        const std::filesystem::path input =
            options.hdd_directory.empty()
                ? std::filesystem::current_path() /
                      shortcut->profile.run_defaults.default_hdd_image_relative_path
                : options.hdd_directory;
        std::filesystem::path chd_path;
        std::string chd_error;
        if (!FindChdImage(input, &chd_path, &chd_error))
        {
            LogError("%s", chd_error.c_str());
            return kExitHddError;
        }
        return RunChdTarget(options, chd_path, *shortcut);
    }
    if (options.hdd_directory.empty() && !options.target_id.empty())
    {
        const re2dj::target::BuiltInTargetProfile* directory_shortcut =
            re2dj::target::FindBuiltInTargetProfileById(options.target_id);
        if (directory_shortcut == nullptr ||
            directory_shortcut->profile.run_defaults.default_hdd_directory_relative_path.empty())
        {
            LogError("profile '%s' has no default HDD directory; pass --hdd <directory>",
                     options.target_id.c_str());
            return kExitUsage;
        }
        options.hdd_directory = std::filesystem::current_path() /
                                directory_shortcut->profile.run_defaults
                                    .default_hdd_directory_relative_path;
    }
    if (options.hdd_directory.empty())
    {
        LogError("--hdd <directory> is required");
        return kExitUsage;
    }

    re2dj::hdd::HddRoot root;
    std::string error;
    if (!re2dj::hdd::HddRoot::Open(options.hdd_directory, &root, &error))
    {
        LogError("%s", error.c_str());
        return kExitHddError;
    }

    if (!options.resolve_path.empty())
    {
        return ResolveOnePath(root, options.resolve_path);
    }

    const re2dj::hdd::HddScanResult scan = re2dj::hdd::ScanHdd(root);
    const std::vector<re2dj::target::TargetProfile> profiles =
        re2dj::target::BuildTargetProfiles(root, scan);

    LogInfo("hdd root   : %s", scan.root.string().c_str());
    LogInfo("scanned    : %zu directories, %zu files%s",
                scan.directory_count,
                scan.file_count,
                scan.truncated ? " (truncated)" : "");
    LogInfo("executables: %zu", scan.executables.size());

    if (profiles.empty())
    {
        LogError("no 32-bit x86 PE32 executable found under %s",
                 scan.root.string().c_str());
        return kExitHddError;
    }

    const re2dj::target::TargetProfile* selected = nullptr;
    if (options.target_id.empty())
    {
        selected = &profiles.front();
    }
    else
    {
        selected = re2dj::target::FindTargetProfileById(profiles, options.target_id);
        if (selected == nullptr)
        {
            LogError("no target profile with id '%s'", options.target_id.c_str());
            options.list_targets = true;
        }
    }

    LogInfo("targets:");
    for (const re2dj::target::TargetProfile& profile : profiles)
    {
        PrintProfile(profile, selected == &profile);
    }

    if (selected == nullptr)
    {
        return kExitUsage;
    }
    if (options.list_targets)
    {
        return kExitOk;
    }

    LogInfo("selected target : %s", selected->id.c_str());
    LogInfo("display name    : %s", selected->display_name.c_str());
    LogInfo("executable      : %s", selected->executable_relative_path.c_str());
    LogInfo("working dir     : %s",
                selected->working_directory_relative_path.empty()
                    ? "<hdd root>"
                    : selected->working_directory_relative_path.c_str());
    if (selected->guest_drive_letter != '\0')
    {
        LogInfo("guest path      : %c:%s",
                    selected->guest_drive_letter,
                    selected->guest_directory.c_str());
    }
    else
    {
        LogInfo("guest path      : <not known for this dump>");
    }
    LogInfo("format hint     : %s",
                std::string(re2dj::target::ExecutableFormatHintName(selected->format_hint))
                    .c_str());

    const re2dj::hdd::ExecutableEntry* selected_entry = nullptr;
    for (const re2dj::hdd::ExecutableEntry& entry : scan.executables)
    {
        if (entry.relative_path != selected->executable_relative_path)
        {
            continue;
        }
        selected_entry = &entry;
        const re2dj::exe::PeImageInfo& info = entry.pe_info;
        LogInfo("machine         : %s", std::string(re2dj::exe::MachineName(info.machine)).c_str());
        LogInfo("magic           : %s", std::string(re2dj::exe::MagicName(info.magic)).c_str());
        LogInfo("subsystem       : %s",
                    std::string(re2dj::exe::SubsystemName(info.subsystem)).c_str());
        LogInfo("image base      : 0x%08llx",
                    static_cast<unsigned long long>(info.image_base));
        LogInfo("entry point rva : 0x%08x", info.entry_point_rva);
        const std::string entry_section(re2dj::exe::EntryPointSectionName(info));
        LogInfo("entry section   : %s%s",
                    entry_section.empty() ? "<outside every section>" : entry_section.c_str(),
                    re2dj::exe::HasEntryPointOutsideTextSection(info)
                        ? "  (outside .text - likely a protection stub)"
                        : "");
        LogInfo("sections        : %u", static_cast<unsigned>(info.sections.size()));
        break;
    }

    if (!selected->note.empty())
    {
        LogInfo("note: %s", selected->note.c_str());
    }

    if (!options.run)
    {
        LogInfo("Nothing was executed. Pass --run to enter the available execution "
                "backend, or use re2dj_pe_analyzer for a full header dump.");
        return kExitOk;
    }

#if defined(__linux__)
    if (selected_entry == nullptr)
    {
        LogError("selected executable metadata is unavailable");
        return kExitHddError;
    }

    std::filesystem::path executable_path;
    if (!root.ResolveFile(selected->executable_relative_path, &executable_path))
    {
        LogError("selected executable is no longer available");
        return kExitHddError;
    }

    re2dj::platform::linux::OriginalRunResult run_result;
    const bool executed = RunLinuxOriginal(
        options, *selected, executable_path, selected_entry->pe_info, {}, &run_result, &error);
    if (!executed)
    {
        LogFatal("EXECUTION_FAILED", "Linux execution failed: %s", error.c_str());
        return kExitNotImplemented;
    }

    LogInfo("load base       : 0x%08x", run_result.load_base.value());
    LogInfo("entry point     : 0x%08x", run_result.entry_point.value());
    if (IsLinuxContinuationRun(options))
    {
        if (run_result.seh_dispatch_count > 0)
        {
            LogInfo("seh dispatched  : count=%u last_handler=0x%08x resumed_eip=0x%08x",
                        run_result.seh_dispatch_count,
                        run_result.last_seh_handler.value(),
                        run_result.last_seh_resumed_eip.value());
        }
        PrintApiCalls(run_result);
        PrintDeviceActivity(run_result);
    }
    switch (run_result.boundary)
    {
    case re2dj::platform::linux::OriginalRunBoundary::kImportGate:
    {
        if (run_result.by_ordinal)
        {
            LogInfo("first boundary  : import %s!#%u",
                        run_result.module.c_str(),
                        static_cast<unsigned>(run_result.ordinal));
        }
        else
        {
            LogInfo("first boundary  : import %s!%s",
                        run_result.module.c_str(),
                        run_result.name.c_str());
        }
        LogInfo("gate / eip / esp: 0x%08x / 0x%08x / 0x%08x",
                    run_result.gate_address.value(),
                    run_result.instruction_pointer.value(),
                    run_result.stack_pointer.value());
        if (run_result.import_stack_observed)
        {
            LogInfo("stack ret / arg0: 0x%08x / 0x%08x",
                        run_result.import_return_address,
                        run_result.import_first_argument);
        }
        if (run_result.import_first_argument_text_observed)
        {
            LogInfo("arg0 text       : %s", run_result.import_first_argument_text.c_str());
        }
        const std::string export_name = run_result.by_ordinal
            ? "#" + std::to_string(run_result.ordinal)
            : run_result.name;
        LogFatal("HLE_UNIMPLEMENTED",
                 "execution stopped at import %s!%s",
                 run_result.module.c_str(),
                 export_name.c_str());
        return kExitNotImplemented;
    }
    case re2dj::platform::linux::OriginalRunBoundary::kProcessExit:
        if (IsLinuxContinuationRun(options))
        {
            PrintContinuationBoundary(run_result);
            return kExitOk;
        }
        LogInfo("first boundary  : process exit (guest status 0x%08x)",
                    run_result.status_code);
        return kExitOk;
    case re2dj::platform::linux::OriginalRunBoundary::kFault:
        LogFatal("GUEST_FAULT",
                 "host signal/status %u, eip 0x%08x",
                 run_result.status_code,
                 run_result.instruction_pointer.value());
        return kExitNotImplemented;
    case re2dj::platform::linux::OriginalRunBoundary::kStopped:
        LogFatal("EXECUTION_STOPPED", "guest stopped before a supported terminal boundary");
        return kExitNotImplemented;
    case re2dj::platform::linux::OriginalRunBoundary::kFirstImportCompleted:
        LogInfo("first import completion: return 0x%08x, SIGTRAP EIP 0x%08x",
                    run_result.import_return_address, run_result.instruction_pointer.value());
        return kExitOk;
    case re2dj::platform::linux::OriginalRunBoundary::kFirstResolverObserved:
        LogInfo("first resolver completion: %s, return 0x%08x, SIGTRAP EIP 0x%08x",
                    run_result.import_first_argument_text.c_str(),
                    run_result.import_return_address, run_result.instruction_pointer.value());
        PrintResolverIdentity(run_result);
        return kExitOk;
    case re2dj::platform::linux::OriginalRunBoundary::kGetVersionCalled:
        LogInfo("GetVersion call completion: return 0x%08x, SIGTRAP EIP 0x%08x",
                    run_result.import_return_address, run_result.instruction_pointer.value());
        PrintResolverIdentity(run_result);
        PrintInstructionTrace(run_result);
        return kExitOk;
    case re2dj::platform::linux::OriginalRunBoundary::kGetVersionCallNotReached:
        LogInfo("GetVersion thunk not reached: signal %u, EIP 0x%08x",
                    run_result.status_code, run_result.instruction_pointer.value());
        PrintFaultObservation(run_result);
        PrintInstructionTrace(run_result);
        PrintResolverIdentity(run_result);
        if (run_result.unhandled_dynamic_request_observed)
        {
            LogFatal("HLE_UNIMPLEMENTED",
                     "dynamic request=%s",
                     run_result.unhandled_dynamic_request.c_str());
        }
        return kExitOk;
    case re2dj::platform::linux::OriginalRunBoundary::kCreateFileCalled:
        LogInfo("CreateFileA call completion: return 0x%08x, SIGTRAP EIP 0x%08x",
                    run_result.import_return_address,
                    run_result.instruction_pointer.value());
        PrintCreateFileObservation(run_result);
        PrintResolverIdentity(run_result);
        return kExitOk;
    case re2dj::platform::linux::OriginalRunBoundary::kCreateFileCallNotReached:
        LogInfo("CreateFileA thunk not reached: signal %u, EIP 0x%08x",
                    run_result.status_code,
                    run_result.instruction_pointer.value());
        PrintFaultObservation(run_result);
        return kExitOk;
    case re2dj::platform::linux::OriginalRunBoundary::kContinuationUnhandledImport:
    case re2dj::platform::linux::OriginalRunBoundary::kContinuationUnresolvedLookup:
    case re2dj::platform::linux::OriginalRunBoundary::kContinuationFault:
    case re2dj::platform::linux::OriginalRunBoundary::kContinuationCallLimit:
        PrintContinuationBoundary(run_result);
        return kExitOk;
    }
#elif defined(_WIN32)
    if (options.fullscreen && !selected->run_defaults.hle_d3d3)
    {
        LogFatal("EXECUTION_UNSUPPORTED",
                 "--fullscreen is not supported by profile '%s'",
                 selected->id.c_str());
        return kExitNotImplemented;
    }
    if ((options.audio_gain_explicit || options.audio_volume_trace) &&
        !selected->run_defaults.hle_directsound)
    {
        LogFatal("EXECUTION_UNSUPPORTED",
                 "audio options are not supported by profile '%s'",
                 selected->id.c_str());
        return kExitNotImplemented;
    }
    if (options.demo_volume_explicit && !selected->run_defaults.demo_volume.has_value())
    {
        LogFatal("EXECUTION_UNSUPPORTED",
                 "--demo-volume is not supported by profile '%s'",
                 selected->id.c_str());
        return kExitNotImplemented;
    }
    re2dj::platform::windows::OriginalProcessOptions run_options;
    run_options.hdd_directory = root.root();
    run_options.target_id = selected->id;
    run_options.hle_profile_id = selected->hle_profile_id;
    run_options.profile_defaults = selected->run_defaults;
    if (options.audio_gain_explicit)
    {
        run_options.profile_defaults.audio_gain_db = options.audio_gain_db;
    }
    if (options.demo_volume_explicit)
    {
        run_options.profile_defaults.demo_volume = options.demo_volume;
    }
    if (options.fullscreen_explicit)
    {
        run_options.profile_defaults.fullscreen = options.fullscreen;
    }
    if (options.present_sync_explicit)
    {
        run_options.profile_defaults.present_sync = options.present_sync;
    }
    if (options.guest_wait_trace)
    {
        run_options.profile_defaults.guest_wait_trace = true;
    }
    if (options.image_dump)
    {
        run_options.profile_defaults.image_dump = true;
        run_options.profile_defaults.image_dump_delay_ms = options.image_dump_delay_ms;
    }
    run_options.audio_volume_trace = options.audio_volume_trace;
    run_options.io_config = NormalizeIoConfigForProfile(
        options.io_config, run_options.profile_defaults, selected->id);
    const int run_result =
        re2dj::platform::windows::RunOriginalProcess(run_options, &error);
    if (run_result < 0)
    {
        LogFatal("EXECUTION_FAILED", "Windows execution failed: %s", error.c_str());
        return kExitNotImplemented;
    }
    return run_result;
#else
    LogFatal("EXECUTION_UNSUPPORTED",
             "--run is not connected to an execution backend on this host");
    return kExitNotImplemented;
#endif
}
