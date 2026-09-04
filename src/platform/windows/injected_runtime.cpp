#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <intrin.h>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "re2dj/hle/hardlock/api_descriptor.h"
#include "re2dj/hle/hardlock/protocol.h"
#include "re2dj/hle/hardlock/device.h"
#include "re2dj/hle/hardlock/transform_responses.h"
#include "re2dj/device/lptdi_challenge_response.h"
#include "re2dj/input/legacy_io_port_bus.h"
#include "re2dj/storage/fat32_chd.h"
#include "re2dj/storage/guest_path.h"
#include "direct3d3_com_facade.h"
#include "directdraw7_com_facade.h"
#include "display_mode_boundary.h"
#include "ez2dj_keyboard_input.h"
#include "message_box_boundary.h"
#include "directinput7_com_facade.h"
#include "directsound_com_facade.h"

extern "C" __declspec(dllexport) volatile DWORD g_re2dj_probe_original_target = 0;
extern "C" __declspec(dllexport) char g_re2dj_hle_command_line[MAX_PATH] = {};
extern "C" __declspec(dllexport) char g_re2dj_hle_windows_directory[MAX_PATH] = {};
extern "C" __declspec(dllexport) char g_re2dj_vfs_hdd_root[MAX_PATH] = {};
extern "C" __declspec(dllexport) char g_re2dj_vfs_overlay_root[MAX_PATH] = {};
extern "C" __declspec(dllexport) char g_re2dj_vfs_chd_path[MAX_PATH] = {};
extern "C" __declspec(dllexport) char g_re2dj_vfs_trace_path[MAX_PATH] = {};
// Dynamic file APIs are routed to the VFS only for profiles with confirmed
// dynamic resolver evidence.
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_vfs_dynamic_resolver = 0;
extern "C" __declspec(dllexport) char g_re2dj_device_mock_path_prefix[MAX_PATH] = {};
// Device-emulation policy: 0 keeps the natural open failure, 1 lets the
// emulated \\.\ devices open successfully.
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_device_mock = 0;
// IOCTL policy: 0 disables synthetic IOCTLs, 1 returns zero bytes, 2 reports
// the full output size while preserving the buffer, 3 copies configured
// response-profile bytes, and 4 derives a response for a selected target state.
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_device_ioctl_mode = 0;
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_device_response_410_size = 0;
extern "C" __declspec(dllexport) unsigned char g_re2dj_device_response_410[8] = {};
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_device_response_414_size = 0;
extern "C" __declspec(dllexport) unsigned char g_re2dj_device_response_414[104] = {};
extern "C" __declspec(dllexport) unsigned char g_re2dj_device_target_state[8] = {};
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_hardlock_response_450_enabled = 0;
extern "C" __declspec(dllexport) unsigned char g_re2dj_hardlock_response_450[6] = {};
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_hardlock_44c_tail_enabled = 0;
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_hardlock_44c_tail_word = 0;
// The Hardlock device boundary. Enabled when the launcher has material to
// apply. It answers the four IOCTLs from values computed outside this
// repository; nothing here derives a response.
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_hardlock_device_enabled = 0;
// Externally computed Function 0x0e response map, transferred by the launcher.
// Each entry is an eight-byte challenge followed by its eight-byte response.
// re2DJ never derives these values; it only applies what it is given.
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_hardlock_transform_response_count = 0;
extern "C" __declspec(dllexport) unsigned char g_re2dj_hardlock_transform_responses[4096] = {};
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_wts_console_session_mock = 0;
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_hle_io_ports = 0;
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_io_image_base = 0;
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_io_in_byte_rva = 0;
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_io_out_byte_rva = 0;
extern "C" __declspec(dllexport) char g_re2dj_io_config_path[MAX_PATH] = {};
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_hle_message_box = 0;
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_message_box_result = 1;

namespace
{

constexpr char kProbeMessage[] = "re2dj:handoff:GetCommandLineA";
constexpr char kHleMessage[] = "re2dj:hle:GetCommandLineA";
constexpr char kWindowsDirectoryMessage[] = "re2dj:hle:GetWindowsDirectoryA";
constexpr char kCreateFileMessage[] = "re2dj:vfs:CreateFileA";
constexpr char kFileApiMessage[] = "re2dj:vfs:file-api";
constexpr char kDeviceIoControlMessage[] = "re2dj:device:DeviceIoControl";
constexpr char kExitProcessMessage[] = "re2dj:probe:ExitProcess";

re2dj::input::LegacyIoPortBus g_legacy_io_port_bus;
re2dj::platform::windows::Ez2DjKeyboardInput g_keyboard_input;
volatile LONG g_keyboard_input_state = 0;
volatile LONG g_vfs_image_trace_count = 0;
volatile LONG g_vfs_script_trace_count = 0;
volatile LONG g_vfs_device_trace_count = 0;
volatile LONG g_vfs_open_trace_count = 0;
volatile LONG g_dynamic_resolver_trace_count = 0;
volatile LONG g_dynamic_resolver_caller_trace_count = 0;
volatile LONG g_wts_query_trace_count = 0;

using WtsQuerySessionInformationAProc = BOOL(WINAPI*)(
    HANDLE server,
    DWORD session_id,
    DWORD info_class,
    LPSTR* buffer,
    DWORD* bytes_returned);
WtsQuerySessionInformationAProc g_original_wts_query_session_information_a = nullptr;

// Asset classes the bounded open diagnostic reports on. Images answer which
// bitmaps a scene resolved, scripts answer whether the scene description that
// names those bitmaps was reached at all.
enum class VfsAssetKind
{
    kNone,
    kImage,
    kScript,
};

bool HasExtensionIgnoreCase(const char* path, const char* extension)
{
    if (path == nullptr)
    {
        return false;
    }
    const std::size_t length = std::strlen(path);
    const std::size_t extension_length = std::strlen(extension);
    return length >= extension_length &&
           _stricmp(path + length - extension_length, extension) == 0;
}

VfsAssetKind ClassifyVfsAsset(const char* path)
{
    if (HasExtensionIgnoreCase(path, ".bmp"))
    {
        return VfsAssetKind::kImage;
    }
    if (HasExtensionIgnoreCase(path, ".str"))
    {
        return VfsAssetKind::kScript;
    }
    return VfsAssetKind::kNone;
}

bool IsWin32DevicePath(const char* path)
{
    return path != nullptr && std::strlen(path) >= 4 && path[0] == '\\' && path[1] == '\\' && path[2] == '.' &&
           path[3] == '\\';
}

// Separate budgets so the attract loop's bitmap sweep cannot exhaust the log
// before the rarer script requests appear in it.
bool ClaimVfsTraceBudget(VfsAssetKind kind)
{
    constexpr LONG kMaximumImageDiagnostics = 1024;
    constexpr LONG kMaximumScriptDiagnostics = 256;
    switch (kind)
    {
    case VfsAssetKind::kImage:
        return InterlockedIncrement(&g_vfs_image_trace_count) <= kMaximumImageDiagnostics;
    case VfsAssetKind::kScript:
        return InterlockedIncrement(&g_vfs_script_trace_count) <= kMaximumScriptDiagnostics;
    case VfsAssetKind::kNone:
        break;
    }
    return false;
}

bool ClaimVfsDeviceTraceBudget()
{
    constexpr LONG kMaximumDeviceDiagnostics = 128;
    return InterlockedIncrement(&g_vfs_device_trace_count) <= kMaximumDeviceDiagnostics;
}

bool ClaimVfsOpenTraceBudget()
{
    constexpr LONG kMaximumOpenDiagnostics = 128;
    return InterlockedIncrement(&g_vfs_open_trace_count) <= kMaximumOpenDiagnostics;
}

bool ClaimDynamicResolverTraceBudget()
{
    constexpr LONG kMaximumResolverDiagnostics = 128;
    return InterlockedIncrement(&g_dynamic_resolver_trace_count) <=
           kMaximumResolverDiagnostics;
}

bool ClaimDynamicResolverCallerTraceBudget()
{
    constexpr LONG kMaximumCallerDiagnostics = 32;
    return InterlockedIncrement(&g_dynamic_resolver_caller_trace_count) <=
           kMaximumCallerDiagnostics;
}

bool ClaimWtsQueryTraceBudget()
{
    constexpr LONG kMaximumWtsQueryDiagnostics = 32;
    return InterlockedIncrement(&g_wts_query_trace_count) <=
           kMaximumWtsQueryDiagnostics;
}

void AppendVfsTraceMessage(const char* message)
{
    if (g_re2dj_vfs_trace_path[0] == '\0' || message == nullptr)
    {
        return;
    }
    HANDLE trace = CreateFileA(g_re2dj_vfs_trace_path,
                               FILE_APPEND_DATA,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               nullptr,
                               OPEN_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL,
                               nullptr);
    if (trace == INVALID_HANDLE_VALUE)
    {
        return;
    }
    DWORD written = 0;
    WriteFile(trace,
              message,
              static_cast<DWORD>(std::strlen(message)),
              &written,
              nullptr);
    CloseHandle(trace);
}

void EnsureDiagnosticBoundariesInstalled()
{
    // The launcher fills the enable flags after this module is loaded, so the
    // boundary cannot be installed from DllMain. It is installed from the HLE
    // entry points the guest reaches earliest instead, and the boundary itself
    // ignores every call after the first successful install.
    if (g_re2dj_hle_message_box == 0)
    {
        return;
    }
    re2dj::platform::windows::InstallMessageBoxBoundary(
        &AppendVfsTraceMessage, static_cast<int>(g_re2dj_message_box_result));
}

void ReportDynamicResolverCallerWindow(std::uintptr_t caller)
{
    constexpr std::size_t kBytesBeforeCaller = 8;
    constexpr std::size_t kBytesAfterCaller = 16;
    if (g_re2dj_vfs_trace_path[0] == '\0' || caller < kBytesBeforeCaller ||
        !ClaimDynamicResolverCallerTraceBudget())
    {
        return;
    }
    const std::uintptr_t base = caller - kBytesBeforeCaller;
    unsigned char bytes[kBytesBeforeCaller + kBytesAfterCaller] = {};
    SIZE_T copied = 0;
    const BOOL readable =
        ReadProcessMemory(GetCurrentProcess(),
                          reinterpret_cast<const void*>(base),
                          bytes,
                          sizeof(bytes),
                          &copied) != FALSE &&
        copied == sizeof(bytes);
    char hex[sizeof(bytes) * 2 + 1] = {};
    for (SIZE_T index = 0; index < copied; ++index)
    {
        std::snprintf(hex + index * 2, 3, "%02x", bytes[index]);
    }
    char message[256] = {};
    std::snprintf(message,
                  sizeof(message),
                  "re2dj:vfs:dynamic-resolver-caller:base=0x%08x:caller=0x%08x:readable=%u:bytes=%s\r\n",
                  static_cast<unsigned>(base),
                  static_cast<unsigned>(caller),
                  readable ? 1U : 0U,
                  hex);
    AppendVfsTraceMessage(message);
}

// Running totals of the Hardlock requests this process answered, kept so the
// exit record can say whether the protection was involved. Log order alone
// cannot: a request can be the last line written and still be seconds old.
struct HardlockActivity
{
    unsigned total = 0;
    unsigned initialize = 0;
    unsigned handshake = 0;
    unsigned descriptor = 0;
    unsigned transform = 0;
    unsigned other = 0;
    unsigned rejected = 0;
    const char* last_kind = "none";
    const char* last_outcome = "none";
    unsigned last_bytes = 0;
    ULONGLONG last_tick = 0;
};

HardlockActivity g_hardlock_activity;

// Set once the observation wrapper runs. Not every exit reaches it: a run that
// ends through the executable's own ExitProcess import never passes the
// dynamic resolver, and such a run was observed exiting with code 1.
volatile LONG g_exit_wrapper_fired = 0;

void ReportExitProcessHardlock()
{
    if (g_re2dj_vfs_trace_path[0] == '\0')
    {
        return;
    }
    const HardlockActivity& activity = g_hardlock_activity;
    char elapsed[32] = "none";
    if (activity.total != 0)
    {
        const ULONGLONG now = GetTickCount64();
        const ULONGLONG delta = now >= activity.last_tick ? now - activity.last_tick : 0;
        std::snprintf(elapsed, sizeof(elapsed), "%llu", delta);
    }
    char message[384] = {};
    std::snprintf(message,
                  sizeof(message),
                  "re2dj:vfs:exit-process-hardlock:total=%u:initialize=%u:handshake=%u:"
                  "descriptor=%u:transform=%u:other=%u:rejected=%u:"
                  "last_kind=%.31s:last_outcome=%.31s:last_bytes=%u:elapsed_ms=%s\r\n",
                  activity.total,
                  activity.initialize,
                  activity.handshake,
                  activity.descriptor,
                  activity.transform,
                  activity.other,
                  activity.rejected,
                  activity.last_kind,
                  activity.last_outcome,
                  activity.last_bytes,
                  elapsed);
    AppendVfsTraceMessage(message);
}

// Written from process detach for the exits the wrapper never sees, so every
// run ends with one attribution record regardless of which path it took. The
// exit code is not available here: during detach the process is still marked
// active, so only the route is recorded.
void ReportExitDetach()
{
    if (g_re2dj_vfs_trace_path[0] == '\0')
    {
        return;
    }
    AppendVfsTraceMessage(
        "re2dj:vfs:exit-detach:route=process_detach:wrapper=0:code=unknown\r\n");
}

// Records who ended the process. The guest exits deliberately once its own
// checks fail, so the caller's address is what separates a protection
// rejection from an ordinary shutdown. No budget guards this: it fires once
// per process, and losing it would lose the whole point of the wrapper.
void ReportExitProcess(const char* route, unsigned code, std::uintptr_t caller)
{
    if (g_re2dj_vfs_trace_path[0] == '\0')
    {
        return;
    }
    const std::uintptr_t image_base =
        reinterpret_cast<std::uintptr_t>(GetModuleHandleA(nullptr));
    const bool caller_in_image = image_base != 0 && caller >= image_base;
    constexpr std::size_t kBytesBeforeCaller = 24;
    constexpr std::size_t kBytesAfterCaller = 8;
    unsigned char bytes[kBytesBeforeCaller + kBytesAfterCaller] = {};
    SIZE_T copied = 0;
    const std::uintptr_t base =
        caller >= kBytesBeforeCaller ? caller - kBytesBeforeCaller : 0;
    const BOOL readable =
        base != 0 &&
        ReadProcessMemory(GetCurrentProcess(),
                          reinterpret_cast<const void*>(base),
                          bytes,
                          sizeof(bytes),
                          &copied) != FALSE &&
        copied == sizeof(bytes);
    char hex[sizeof(bytes) * 2 + 1] = {};
    for (SIZE_T index = 0; readable && index < copied; ++index)
    {
        std::snprintf(hex + index * 2, 3, "%02x", bytes[index]);
    }
    char message[320] = {};
    std::snprintf(message,
                  sizeof(message),
                  "re2dj:vfs:exit-process:route=%.31s:code=%u:caller=0x%08x:image_base=0x%08x:"
                  "caller_rva=0x%08x:in_image=%u:window_base=0x%08x:readable=%u:bytes=%s\r\n",
                  route == nullptr ? "unknown" : route,
                  code,
                  static_cast<unsigned>(caller),
                  static_cast<unsigned>(image_base),
                  caller_in_image ? static_cast<unsigned>(caller - image_base) : 0U,
                  caller_in_image ? 1U : 0U,
                  static_cast<unsigned>(base),
                  readable ? 1U : 0U,
                  hex);
    AppendVfsTraceMessage(message);
}

void ReportDynamicResolverName(const char* name,
                               const char* route,
                               std::uintptr_t address,
                               std::uintptr_t caller)
{
    if (name == nullptr || route == nullptr || !ClaimDynamicResolverTraceBudget())
    {
        return;
    }
    char message[320] = {};
    std::snprintf(message,
                  sizeof(message),
                  "re2dj:vfs:dynamic-resolver:name=%.95s:route=%.31s:address=0x%08x:caller=0x%08x\r\n",
                  name,
                  route,
                  static_cast<unsigned>(address),
                  static_cast<unsigned>(caller));
    AppendVfsTraceMessage(message);
    ReportDynamicResolverCallerWindow(caller);
}

void ReportWtsQuery(DWORD session_id,
                    DWORD info_class,
                    BOOL success,
                    LPSTR* buffer,
                    DWORD* bytes_returned)
{
    if (!ClaimWtsQueryTraceBudget())
    {
        return;
    }
    const DWORD size = bytes_returned == nullptr ? 0 : *bytes_returned;
    std::uint32_t scalar = 0;
    DWORD scalar_size = 0;
    if (success != FALSE && buffer != nullptr && *buffer != nullptr && size != 0)
    {
        scalar_size = size < sizeof(scalar) ? size : sizeof(scalar);
        std::memcpy(&scalar, *buffer, scalar_size);
    }
    char message[256] = {};
    std::snprintf(message,
                  sizeof(message),
                  "re2dj:vfs:wts-query:session=%lu:class=%lu:success=%u:bytes=%lu:scalar_size=%lu:scalar=0x%08x\r\n",
                  static_cast<unsigned long>(session_id),
                  static_cast<unsigned long>(info_class),
                  success != FALSE ? 1U : 0U,
                  static_cast<unsigned long>(size),
                  static_cast<unsigned long>(scalar_size),
                  static_cast<unsigned>(scalar));
    AppendVfsTraceMessage(message);
}

void ReportVfsAssetOpen(const char* api,
                        const char* requested,
                        const char* mapped,
                        HANDLE result,
                        DWORD error)
{
    if (g_re2dj_vfs_trace_path[0] == '\0' ||
        !ClaimVfsTraceBudget(ClassifyVfsAsset(requested)))
    {
        return;
    }
    char message[900] = {};
    std::snprintf(message,
                  sizeof(message),
                  "re2dj:vfs:asset-open:api=%s:request=%s:mapped=%s:success=%u:error=%lu\r\n",
                  api,
                  requested,
                  mapped == nullptr ? "" : mapped,
                  result != INVALID_HANDLE_VALUE ? 1U : 0U,
                  static_cast<unsigned long>(error));
    AppendVfsTraceMessage(message);
}

void ReportVfsDeviceOpen(const char* api,
                         const char* requested,
                         HANDLE result,
                         DWORD error)
{
    if (!IsWin32DevicePath(requested) || !ClaimVfsDeviceTraceBudget())
    {
        return;
    }
    char message[900] = {};
    std::snprintf(message,
                  sizeof(message),
                  "re2dj:vfs:device-open:api=%s:request=%s:success=%u:error=%lu\r\n",
                  api,
                  requested,
                  result != INVALID_HANDLE_VALUE ? 1U : 0U,
                  static_cast<unsigned long>(error));
    AppendVfsTraceMessage(message);
}

void ReportVfsCreateFileRequest(LPCSTR requested,
                                DWORD access,
                                DWORD disposition,
                                DWORD flags)
{
    if (g_re2dj_vfs_trace_path[0] == '\0' || requested == nullptr ||
        !ClaimVfsOpenTraceBudget())
    {
        return;
    }
    char message[900] = {};
    std::snprintf(message,
                  sizeof(message),
                  "re2dj:vfs:create-file:stage=request:request=%.511s:access=0x%08x:disposition=0x%08x:flags=0x%08x\r\n",
                  requested,
                  static_cast<unsigned>(access),
                  static_cast<unsigned>(disposition),
                  static_cast<unsigned>(flags));
    AppendVfsTraceMessage(message);
}

void ReportVfsCreateFileResult(const char* stage,
                               const char* requested,
                               const char* mapped,
                               HANDLE result,
                               DWORD error)
{
    if (g_re2dj_vfs_trace_path[0] == '\0' || stage == nullptr ||
        requested == nullptr || !ClaimVfsOpenTraceBudget())
    {
        return;
    }
    char message[900] = {};
    std::snprintf(message,
                  sizeof(message),
                  "re2dj:vfs:create-file:stage=%.63s:request=%.383s:mapped=%.383s:success=%u:error=%lu\r\n",
                  stage,
                  requested,
                  mapped == nullptr ? "" : mapped,
                  result != INVALID_HANDLE_VALUE ? 1U : 0U,
                  static_cast<unsigned long>(error));
    AppendVfsTraceMessage(message);
}

void ReportDeviceIoControlCode(DWORD control_code,
                               DWORD input_size,
                               DWORD output_size)
{
    if (!ClaimVfsDeviceTraceBudget())
    {
        return;
    }
    char message[160] = {};
    std::snprintf(message,
                  sizeof(message),
                  "re2dj:vfs:device-ioctl-entry:code=0x%08lx:input_size=%lu:output_size=%lu\r\n",
                  static_cast<unsigned long>(control_code),
                  static_cast<unsigned long>(input_size),
                  static_cast<unsigned long>(output_size));
    AppendVfsTraceMessage(message);
}


re2dj::hle::hardlock::HardlockDeviceOptions BuildHardlockDeviceOptions()
{
    re2dj::hle::hardlock::HardlockDeviceOptions options;
    if (g_re2dj_hardlock_response_450_enabled != 0)
    {
        re2dj::hle::hardlock::HardlockHandshakeResponse response = {};
        std::memcpy(response.data(),
                    g_re2dj_hardlock_response_450,
                    response.size());
        options.handshake_response = response;
    }
    if (g_re2dj_hardlock_44c_tail_enabled != 0)
    {
        options.descriptor_tail_word =
            static_cast<std::uint16_t>(g_re2dj_hardlock_44c_tail_word);
    }
    const DWORD response_count = g_re2dj_hardlock_transform_response_count;
    constexpr DWORD kEntryStride =
        static_cast<DWORD>(re2dj::hle::hardlock::kHardlockTransformBlockSize * 2);
    const DWORD response_capacity =
        static_cast<DWORD>(sizeof(g_re2dj_hardlock_transform_responses)) / kEntryStride;
    if (response_count != 0 && response_count <= response_capacity)
    {
        options.transform_responses.reserve(response_count);
        for (DWORD index = 0; index < response_count; ++index)
        {
            const unsigned char* const entry =
                g_re2dj_hardlock_transform_responses + index * kEntryStride;
            re2dj::hle::hardlock::HardlockTransformResponseEntry parsed;
            std::memcpy(parsed.input.data(), entry, parsed.input.size());
            std::memcpy(parsed.output.data(),
                        entry + parsed.input.size(),
                        parsed.output.size());
            options.transform_responses.push_back(parsed);
        }
    }
    return options;
}

// Answers one Hardlock IOCTL at the device boundary. Returns false when the
// request is outside the device contract so the caller keeps its existing
// behavior.
bool CompleteHardlockRequest(DWORD control_code,
                            const void* input,
                            DWORD input_size,
                            void* output,
                            DWORD output_size,
                            LPDWORD bytes_returned,
                            BOOL* completed)
{
    if (g_re2dj_hardlock_device_enabled == 0 || completed == nullptr)
    {
        return false;
    }
    const auto* const input_bytes = static_cast<const std::uint8_t*>(input);
    auto* const output_bytes = static_cast<std::uint8_t*>(output);
    const std::span<const std::uint8_t> input_span =
        input_bytes == nullptr ? std::span<const std::uint8_t>()
                               : std::span<const std::uint8_t>(input_bytes, input_size);
    const std::span<std::uint8_t> output_span =
        output_bytes == nullptr ? std::span<std::uint8_t>()
                                : std::span<std::uint8_t>(output_bytes, output_size);

    re2dj::hle::hardlock::HardlockDevice device(BuildHardlockDeviceOptions());
    const re2dj::hle::hardlock::HardlockDeviceResult result =
        device.Complete(control_code, input_span, output_span);
    if (result.outcome == re2dj::hle::hardlock::HardlockOutcome::kNotHandled)
    {
        return false;
    }

    char message[256] = {};
    std::snprintf(message,
                  sizeof(message),
                  "re2dj:vfs:hardlock-device:request=%s:outcome=%s:bytes=%u:"
                  "handshake_answered=%u:status_cleared=%u:tail=%u:"
                  "mapped=%u:unmapped=%u:tick_ms=%llu\r\n",
                  re2dj::hle::hardlock::HardlockRequestKindName(result.kind),
                  re2dj::hle::hardlock::HardlockOutcomeName(result.outcome),
                  static_cast<unsigned>(result.bytes_written),
                  result.handshake_answered ? 1u : 0u,
                  result.descriptor_status_cleared ? 1u : 0u,
                  result.descriptor_tail_written ? 1u : 0u,
                  static_cast<unsigned>(result.transform_blocks_mapped),
                  static_cast<unsigned>(result.transform_blocks_unmapped),
                  static_cast<unsigned long long>(GetTickCount64()));
    AppendVfsTraceMessage(message);

    // Same place, same facts, kept for the exit record. Counting here rather
    // than parsing the trace back keeps the two in step even when the trace
    // file is absent.
    ++g_hardlock_activity.total;
    switch (result.kind)
    {
        case re2dj::hle::hardlock::HardlockRequestKind::kInitialize:
            ++g_hardlock_activity.initialize;
            break;
        case re2dj::hle::hardlock::HardlockRequestKind::kHandshake:
            ++g_hardlock_activity.handshake;
            break;
        case re2dj::hle::hardlock::HardlockRequestKind::kDescriptor:
            ++g_hardlock_activity.descriptor;
            break;
        case re2dj::hle::hardlock::HardlockRequestKind::kTransform:
            ++g_hardlock_activity.transform;
            break;
        default:
            ++g_hardlock_activity.other;
            break;
    }
    if (result.outcome == re2dj::hle::hardlock::HardlockOutcome::kRejectedShape)
    {
        ++g_hardlock_activity.rejected;
    }
    g_hardlock_activity.last_kind =
        re2dj::hle::hardlock::HardlockRequestKindName(result.kind);
    g_hardlock_activity.last_outcome =
        re2dj::hle::hardlock::HardlockOutcomeName(result.outcome);
    g_hardlock_activity.last_bytes = static_cast<unsigned>(result.bytes_written);
    g_hardlock_activity.last_tick = GetTickCount64();

    if (result.outcome == re2dj::hle::hardlock::HardlockOutcome::kRejectedShape)
    {
        if (bytes_returned != nullptr)
        {
            *bytes_returned = 0;
        }
        SetLastError(ERROR_INVALID_DATA);
        *completed = FALSE;
        return true;
    }
    if (bytes_returned != nullptr)
    {
        *bytes_returned = static_cast<DWORD>(result.bytes_written);
    }
    SetLastError(ERROR_SUCCESS);
    *completed = TRUE;
    return true;
}

LONG CALLBACK HandleLegacyIoPortException(EXCEPTION_POINTERS* exception)
{
    if (g_re2dj_hle_io_ports == 0 || g_re2dj_io_image_base == 0 ||
        exception == nullptr || exception->ExceptionRecord == nullptr ||
        exception->ContextRecord == nullptr ||
        exception->ExceptionRecord->ExceptionCode != EXCEPTION_PRIV_INSTRUCTION)
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    const DWORD address = static_cast<DWORD>(
        reinterpret_cast<std::uintptr_t>(exception->ExceptionRecord->ExceptionAddress));
    const bool is_read = g_re2dj_io_in_byte_rva != 0 &&
                         address == g_re2dj_io_image_base + g_re2dj_io_in_byte_rva;
    const bool is_write = g_re2dj_io_out_byte_rva != 0 &&
                          address == g_re2dj_io_image_base + g_re2dj_io_out_byte_rva;
    if (!is_read && !is_write)
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    const unsigned char opcode = *reinterpret_cast<const unsigned char*>(address);
    if (opcode != (is_read ? 0xec : 0xee))
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    const std::uint16_t port = static_cast<std::uint16_t>(exception->ContextRecord->Edx);
    std::uint8_t value = static_cast<std::uint8_t>(exception->ContextRecord->Eax);
    if (is_read && g_re2dj_io_config_path[0] != '\0')
    {
        if (g_keyboard_input_state == 0)
        {
            std::string error;
            if (g_keyboard_input.Initialize(g_re2dj_io_config_path, &error))
            {
                InterlockedExchange(&g_keyboard_input_state, 1);
            }
            else
            {
                const std::string message = "re2dj:io-config:" + error + "\n";
                OutputDebugStringA(message.c_str());
                InterlockedExchange(&g_keyboard_input_state, 2);
            }
        }
        if (g_keyboard_input_state == 1)
        {
            g_keyboard_input.Poll(&g_legacy_io_port_bus,
                                 static_cast<std::uint64_t>(GetTickCount()));
        }
    }
    const bool handled = is_read ? g_legacy_io_port_bus.ReadByte(port, &value)
                                 : g_legacy_io_port_bus.WriteByte(port, value);
    if (!handled)
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (is_read)
    {
        exception->ContextRecord->Eax =
            (exception->ContextRecord->Eax & 0xffffff00u) | value;
    }
    exception->ContextRecord->Eip += 1;
    return EXCEPTION_CONTINUE_EXECUTION;
}

bool HasPrefixIgnoreCase(const char* text, const char* prefix)
{
    for (; *prefix != '\0'; ++text, ++prefix)
    {
        const bool text_separator = *text == '\\' || *text == '/';
        const bool prefix_separator = *prefix == '\\' || *prefix == '/';
        if (*text == '\0' ||
            ((!text_separator || !prefix_separator) && _strnicmp(text, prefix, 1) != 0))
        {
            return false;
        }
    }
    return *text == '\0' || *text == '\\' || *text == '/';
}

bool FindPathSuffixUnderRoot(const char* name, const char* root, const char** suffix)
{
    if (name == nullptr || root == nullptr || suffix == nullptr || root[0] == '\0' ||
        !HasPrefixIgnoreCase(name, root))
    {
        return false;
    }
    const char* candidate = name + std::strlen(root);
    while (*candidate == '\\' || *candidate == '/')
    {
        ++candidate;
    }
    *suffix = candidate;
    return true;
}

bool JoinRoot(const char* root, const char* suffix, char path[MAX_PATH])
{
    if (root == nullptr || root[0] == '\0' || suffix == nullptr)
    {
        return false;
    }
    if (strcpy_s(path, MAX_PATH, root) != 0)
    {
        return false;
    }
    const std::size_t length = std::strlen(path);
    if (*suffix != '\0' && length != 0 && path[length - 1] != '\\' && path[length - 1] != '/')
    {
        if (strcat_s(path, MAX_PATH, "\\") != 0)
        {
            return false;
        }
    }
    while (*suffix == '\\' || *suffix == '/')
    {
        ++suffix;
    }
    return strcat_s(path, MAX_PATH, suffix) == 0;
}

bool EnsureParentDirectories(const char* path)
{
    char parent[MAX_PATH] = {};
    if (strcpy_s(parent, path) != 0)
    {
        return false;
    }
    char* slash = std::strrchr(parent, '\\');
    if (slash == nullptr)
    {
        return true;
    }
    *slash = '\0';
    for (char* cursor = parent + 3; *cursor != '\0'; ++cursor)
    {
        if (*cursor != '\\' && *cursor != '/')
        {
            continue;
        }
        const char saved = *cursor;
        *cursor = '\0';
        const BOOL created = CreateDirectoryA(parent, nullptr);
        const DWORD error = created ? ERROR_SUCCESS : GetLastError();
        *cursor = saved;
        if (!created && error != ERROR_ALREADY_EXISTS)
        {
            return false;
        }
    }
    const BOOL created = CreateDirectoryA(parent, nullptr);
    return created != FALSE || GetLastError() == ERROR_ALREADY_EXISTS;
}

bool IsRegularFile(const char* path)
{
    const DWORD attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

// Synthetic handles for emulated \\.\ devices live in a reserved range so the
// file wrappers can recognize them without consulting the host handle table.
constexpr std::uintptr_t kDeviceMockHandleBase = 0xFEED0000;

// CHD-backed read handles are process-local tokens, not Windows kernel
// handles. They are intentionally disjoint from the device mock range.
constexpr std::uintptr_t kChdFileHandleBase = 0xFCCD0000;
constexpr std::size_t kMaximumChdFileHandles = 256;

struct ChdFileHandle
{
    bool used = false;
    std::uint64_t position = 0;
    std::uint32_t size = 0;
    std::string relative_path;
};

std::unique_ptr<re2dj::storage::Fat32Volume> g_chd_volume;
std::string g_chd_mount_error;
ChdFileHandle g_chd_file_handles[kMaximumChdFileHandles];

bool IsChdFileHandle(HANDLE handle)
{
    const std::uintptr_t value = reinterpret_cast<std::uintptr_t>(handle);
    return value > kChdFileHandleBase &&
           value <= kChdFileHandleBase + kMaximumChdFileHandles;
}

ChdFileHandle* LookupChdFileHandle(HANDLE handle)
{
    if (!IsChdFileHandle(handle))
    {
        return nullptr;
    }
    const std::size_t index = static_cast<std::size_t>(
        reinterpret_cast<std::uintptr_t>(handle) - kChdFileHandleBase - 1);
    return index < kMaximumChdFileHandles && g_chd_file_handles[index].used
               ? &g_chd_file_handles[index]
               : nullptr;
}

bool IsChdConfigured()
{
    return g_re2dj_vfs_chd_path[0] != '\0';
}

bool EnsureChdMounted()
{
    if (g_chd_volume != nullptr)
    {
        return true;
    }
    if (!IsChdConfigured())
    {
        g_chd_mount_error = "CHD path was not configured";
        return false;
    }
    if (!re2dj::storage::Fat32Volume::Open(g_re2dj_vfs_chd_path,
                                           &g_chd_volume,
                                           &g_chd_mount_error))
    {
        return false;
    }
    return true;
}

// The guest's logical current directory, held as path components under the HDD
// root. The guest changes it with SetCurrentDirectoryA and then opens resources
// by bare name, so a relative request means something different after every
// change. The host process directory stays where the launcher put it: only this
// mapping moves, which keeps unrelated host APIs unaffected.
std::vector<std::string> g_guest_directory_components;

// Strips whichever root prefix `name` carries and reports whether one was
// found. A name under the mapped HDD root, or under the drive letter the
// original used, names the root directly rather than the current directory.
bool StripGuestRoot(const char* name, const char** suffix)
{
    if (FindPathSuffixUnderRoot(name, g_re2dj_vfs_hdd_root, suffix))
    {
        return true;
    }
    if (HasPrefixIgnoreCase(name, "D:\\ez2dj"))
    {
        const char* candidate = name + 8;
        while (*candidate == '\\' || *candidate == '/')
        {
            ++candidate;
        }
        *suffix = candidate;
        return true;
    }
    return false;
}

// Resolves a guest request to a path relative to the HDD root, '/'-separated.
// Root-anchored forms resolve against the root; every other form resolves
// against the tracked current directory. Returns false for a path Win32 syntax
// rejects, for a UNC path, and for one that climbs above the root.
bool ResolveGuestRelativePath(const char* name, std::string* relative)
{
    if (name == nullptr || relative == nullptr || g_re2dj_vfs_hdd_root[0] == '\0')
    {
        return false;
    }
    const char* suffix = nullptr;
    const bool rooted = StripGuestRoot(name, &suffix);
    if (!rooted)
    {
        suffix = name;
    }
    if (*suffix == '\0')
    {
        relative->clear();
        return rooted;
    }

    re2dj::storage::GuestPath request;
    if (!re2dj::storage::ParseGuestPath(suffix, &request))
    {
        return false;
    }
    // The base the request combines with. Only a plainly relative request keeps
    // the current directory; anything that names a root starts empty.
    re2dj::storage::GuestPath base;
    base.kind = re2dj::storage::GuestPathKind::kDriveAbsolute;
    base.drive_letter = 'D';
    if (!rooted && request.kind == re2dj::storage::GuestPathKind::kRelative)
    {
        base.components = g_guest_directory_components;
    }
    // The root prefix is already gone, so what remains resolves against the
    // base regardless of the shape the guest wrote it in.
    request.kind = re2dj::storage::GuestPathKind::kRelative;
    request.drive_letter = '\0';

    re2dj::storage::GuestPath combined;
    if (!re2dj::storage::CombineGuestPath(base, request, &combined))
    {
        return false;
    }
    *relative = re2dj::storage::GuestPathToRelativeString(combined);
    return true;
}

// The same resolution rendered as a native suffix, for joining onto a root.
bool ResolveGuestNativeSuffix(const char* name, std::string* suffix)
{
    if (!ResolveGuestRelativePath(name, suffix))
    {
        return false;
    }
    for (char& value : *suffix)
    {
        if (value == '/')
        {
            value = '\\';
        }
    }
    return true;
}

bool ChdRelativePath(const char* name, std::string* relative)
{
    std::string resolved;
    if (relative == nullptr || !ResolveGuestRelativePath(name, &resolved) ||
        resolved.empty())
    {
        return false;
    }
    relative->assign("EZ2DJ/");
    relative->append(resolved);
    return true;
}

HANDLE AllocateChdFileHandle(const std::string& relative_path, std::uint32_t size)
{
    for (std::size_t index = 0; index < kMaximumChdFileHandles; ++index)
    {
        ChdFileHandle& handle = g_chd_file_handles[index];
        if (handle.used)
        {
            continue;
        }
        handle.used = true;
        handle.position = 0;
        handle.size = size;
        handle.relative_path = relative_path;
        return reinterpret_cast<HANDLE>(kChdFileHandleBase + index + 1);
    }
    return INVALID_HANDLE_VALUE;
}

bool IsDeviceMockHandle(HANDLE handle)
{
    const std::uintptr_t value = reinterpret_cast<std::uintptr_t>(handle);
    return value > kDeviceMockHandleBase && value <= kDeviceMockHandleBase + 0xff;
}

// Matches the profile-selected device path prefix case-insensitively. A plain
// prefix test is used because the LPTDI port digit varies at guest runtime.
bool HasDeviceMockPrefix(const char* name)
{
    if (g_re2dj_device_mock == 0 || name == nullptr)
    {
        return false;
    }
    const char* prefix = g_re2dj_device_mock_path_prefix;
    if (prefix[0] == '\0')
    {
        prefix = "\\\\.\\lptdi";
    }
    const std::size_t prefix_length = std::strlen(prefix);
    return prefix_length != 0 && _strnicmp(name, prefix, prefix_length) == 0;
}

bool MapVfsPath(const char* name, bool write, char path[MAX_PATH], char source[MAX_PATH])
{
    if (name == nullptr)
    {
        return false;
    }
    // A support-directory path is decided first and by prefix alone; everything
    // else is an HDD path and goes through the guest current directory.
    const char* support_suffix = nullptr;
    std::string hdd_storage;
    const char* hdd_suffix = nullptr;
    if (!FindPathSuffixUnderRoot(name, g_re2dj_vfs_hdd_root, &hdd_suffix) &&
        (FindPathSuffixUnderRoot(name, g_re2dj_hle_windows_directory, &support_suffix) ||
         HasPrefixIgnoreCase(name, "C:\\windows")))
    {
        if (support_suffix == nullptr)
        {
            support_suffix = name + 10;
        }
    }
    else if (!ResolveGuestNativeSuffix(name, &hdd_storage))
    {
        return false;
    }
    else
    {
        hdd_suffix = hdd_storage.c_str();
        support_suffix = nullptr;
    }

    const char* target_root = write ? g_re2dj_vfs_overlay_root
                                    : (hdd_suffix != nullptr ? g_re2dj_vfs_hdd_root
                                                              : g_re2dj_hle_windows_directory);
    if (target_root[0] == '\0')
    {
        return false;
    }
    if (write && support_suffix != nullptr)
    {
        char support_root[MAX_PATH] = {};
        if (!JoinRoot(target_root, "windows", support_root) ||
            !JoinRoot(support_root, support_suffix, path))
        {
            return false;
        }
    }
    else if (!JoinRoot(target_root, hdd_suffix != nullptr ? hdd_suffix : support_suffix, path))
    {
        return false;
    }

    if (!write)
    {
        char overlay[MAX_PATH] = {};
        char overlay_windows[MAX_PATH] = {};
        if (g_re2dj_vfs_overlay_root[0] != '\0' &&
            ((support_suffix == nullptr && JoinRoot(g_re2dj_vfs_overlay_root, hdd_suffix, overlay)) ||
             (support_suffix != nullptr &&
              JoinRoot(g_re2dj_vfs_overlay_root, "windows", overlay_windows) &&
              JoinRoot(overlay_windows, support_suffix, overlay))) &&
            IsRegularFile(overlay))
        {
            strcpy_s(path, MAX_PATH, overlay);
        }
        return true;
    }
    if (source != nullptr)
    {
        const char* source_root = hdd_suffix != nullptr ? g_re2dj_vfs_hdd_root
                                                         : g_re2dj_hle_windows_directory;
        if (!JoinRoot(source_root, hdd_suffix != nullptr ? hdd_suffix : support_suffix, source))
        {
            return false;
        }
    }
    return true;
}

HANDLE OpenChdReadFile(const char* name, DWORD disposition)
{
    if (!IsChdConfigured() || disposition == CREATE_NEW || disposition == CREATE_ALWAYS ||
        disposition == TRUNCATE_EXISTING)
    {
        return INVALID_HANDLE_VALUE;
    }
    std::string relative;
    if (!ChdRelativePath(name, &relative) || !EnsureChdMounted())
    {
        return INVALID_HANDLE_VALUE;
    }
    re2dj::storage::Fat32Entry entry;
    if (!g_chd_volume->Find(relative, &entry, &g_chd_mount_error) || entry.directory)
    {
        return INVALID_HANDLE_VALUE;
    }
    const HANDLE handle = AllocateChdFileHandle(relative, entry.size);
    if (handle == INVALID_HANDLE_VALUE)
    {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return INVALID_HANDLE_VALUE;
    }
    const std::string mapped = "chd://" + relative;
    ReportVfsAssetOpen("CreateFileA", name, mapped.c_str(), handle, ERROR_SUCCESS);
    SetLastError(ERROR_SUCCESS);
    return handle;
}

bool MaterializeChdFile(const char* name, const char* output)
{
    if (!IsChdConfigured() || output == nullptr)
    {
        return false;
    }
    std::string relative;
    if (!ChdRelativePath(name, &relative) || !EnsureChdMounted())
    {
        return false;
    }
    return g_chd_volume->MaterializeFile(relative, output, &g_chd_mount_error);
}

// True when a resolved guest path names a directory. The overlay and the
// native tree are consulted first because a materialized copy is what the host
// will actually open, and the CHD last because it is the source of truth for
// anything not yet copied out.
bool GuestDirectoryExists(const std::string& relative)
{
    if (relative.empty())
    {
        return true;
    }
    std::string native = relative;
    for (char& value : native)
    {
        if (value == '/')
        {
            value = '\\';
        }
    }
    const char* const roots[] = {g_re2dj_vfs_hdd_root, g_re2dj_vfs_overlay_root};
    for (const char* root : roots)
    {
        char path[MAX_PATH] = {};
        if (root[0] == '\0' || !JoinRoot(root, native.c_str(), path))
        {
            continue;
        }
        const DWORD attributes = GetFileAttributesA(path);
        if (attributes != INVALID_FILE_ATTRIBUTES &&
            (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
        {
            return true;
        }
    }
    if (!IsChdConfigured() || !EnsureChdMounted())
    {
        return false;
    }
    re2dj::storage::Fat32Entry entry;
    std::string error;
    return g_chd_volume->Find("EZ2DJ/" + relative, &entry, &error) && entry.directory;
}

std::vector<std::string> SplitGuestRelative(const std::string& relative)
{
    std::vector<std::string> components;
    std::string current;
    for (const char value : relative)
    {
        if (value == '/')
        {
            if (!current.empty())
            {
                components.push_back(current);
                current.clear();
            }
            continue;
        }
        current.push_back(value);
    }
    if (!current.empty())
    {
        components.push_back(current);
    }
    return components;
}

// The current directory as the native path the guest can hand back to us. The
// guest already round-trips paths in this form: every open that succeeds today
// is an absolute path it built from what we returned.
bool GuestDirectoryNativePath(char path[MAX_PATH])
{
    std::string native;
    for (const std::string& component : g_guest_directory_components)
    {
        if (!native.empty())
        {
            native.push_back('\\');
        }
        native.append(component);
    }
    return JoinRoot(g_re2dj_vfs_hdd_root, native.c_str(), path);
}

void ReportVfsCurrentDirectory(const char* stage,
                               const char* requested,
                               const char* resolved,
                               bool success)
{
    if (g_re2dj_vfs_trace_path[0] == '\0' || !ClaimVfsOpenTraceBudget())
    {
        return;
    }
    char message[900] = {};
    std::snprintf(message,
                  sizeof(message),
                  "re2dj:vfs:current-directory:stage=%.31s:request=%.383s:resolved=%.383s:success=%u\r\n",
                  stage,
                  requested == nullptr ? "" : requested,
                  resolved == nullptr ? "" : resolved,
                  success ? 1U : 0U);
    AppendVfsTraceMessage(message);
}

}  // namespace

// Observes the guest's own exit and then performs it. Nothing about the exit
// changes: the wrapper exists because this executable resolves ExitProcess
// through GetProcAddress, so the launcher's static IAT breakpoint never sees
// the call.
extern "C" __declspec(dllexport) void WINAPI Re2djHleExitProcess(UINT code)
{
    InterlockedExchange(&g_exit_wrapper_fired, 1);
    ReportExitProcess("exit_process",
                      static_cast<unsigned>(code),
                      reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
    ReportExitProcessHardlock();
    ExitProcess(code);
}

// The observed code-1 exit reaches neither the ExitProcess wrapper nor process
// detach, and the guest resolves TerminateProcess dynamically, which is the
// one remaining way to end this process. Terminating self skips every cleanup
// path, so this is the only place that exit can be attributed.
extern "C" __declspec(dllexport) BOOL WINAPI Re2djHleTerminateProcess(HANDLE process,
                                                                     UINT code)
{
    const bool terminates_self =
        process == GetCurrentProcess() ||
        (process != nullptr && GetProcessId(process) == GetCurrentProcessId());
    if (terminates_self)
    {
        InterlockedExchange(&g_exit_wrapper_fired, 1);
        ReportExitProcess("terminate_process",
                          static_cast<unsigned>(code),
                          reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
        ReportExitProcessHardlock();
    }
    return TerminateProcess(process, code);
}

extern "C" __declspec(dllexport) BOOL WINAPI Re2djVfsSetCurrentDirectoryA(LPCSTR name)
{
    std::string resolved;
    if (name == nullptr || !ResolveGuestRelativePath(name, &resolved) ||
        !GuestDirectoryExists(resolved))
    {
        ReportVfsCurrentDirectory("set", name, resolved.c_str(), false);
        SetLastError(ERROR_PATH_NOT_FOUND);
        return FALSE;
    }
    g_guest_directory_components = SplitGuestRelative(resolved);
    ReportVfsCurrentDirectory("set", name, resolved.c_str(), true);
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

extern "C" __declspec(dllexport) DWORD WINAPI Re2djVfsGetCurrentDirectoryA(DWORD size,
                                                                          LPSTR buffer)
{
    char path[MAX_PATH] = {};
    if (!GuestDirectoryNativePath(path))
    {
        SetLastError(ERROR_INVALID_NAME);
        return 0;
    }
    const DWORD length = static_cast<DWORD>(std::strlen(path));
    // Win32 reports the buffer size it needs, terminator included, when the
    // caller's buffer is too small, and the written length when it fits.
    if (buffer == nullptr || size <= length)
    {
        ReportVfsCurrentDirectory("get-size", path, path, true);
        return length + 1;
    }
    std::memcpy(buffer, path, static_cast<std::size_t>(length) + 1);
    ReportVfsCurrentDirectory("get", path, path, true);
    SetLastError(ERROR_SUCCESS);
    return length;
}

extern "C" __declspec(dllexport) HANDLE WINAPI Re2djVfsLoadImageA(
    HINSTANCE instance,
    LPCSTR name,
    UINT type,
    int desired_width,
    int desired_height,
    UINT flags)
{
    const bool is_file_bitmap = !IS_INTRESOURCE(name) && name != nullptr &&
                                type == IMAGE_BITMAP && (flags & LR_LOADFROMFILE) != 0;
    if (!is_file_bitmap)
    {
        return LoadImageA(instance, name, type, desired_width, desired_height, flags);
    }
    char path[MAX_PATH] = {};
    const char* load_name = name;
    if (MapVfsPath(name, false, path, nullptr))
    {
        if (!IsRegularFile(path) && !MaterializeChdFile(name, path))
        {
            SetLastError(ERROR_FILE_NOT_FOUND);
            return nullptr;
        }
        load_name = path;
    }
    const HANDLE result =
        LoadImageA(instance, load_name, type, desired_width, desired_height, flags);
    const DWORD error = result == nullptr ? GetLastError() : ERROR_SUCCESS;
    ReportVfsAssetOpen("LoadImageA",
                       name,
                       load_name,
                       result == nullptr ? INVALID_HANDLE_VALUE : result,
                       error);
    SetLastError(error);
    return result;
}

extern "C" __declspec(dllexport) HANDLE WINAPI Re2djVfsCreateFileA(
    LPCSTR name,
    DWORD access,
    DWORD share,
    LPSECURITY_ATTRIBUTES security,
    DWORD disposition,
    DWORD flags,
    HANDLE template_handle)
{
    EnsureDiagnosticBoundariesInstalled();
    OutputDebugStringA(kCreateFileMessage);
    ReportVfsCreateFileRequest(name, access, disposition, flags);
    if (name == nullptr)
    {
        SetLastError(ERROR_INVALID_NAME);
        return INVALID_HANDLE_VALUE;
    }
    if (HasDeviceMockPrefix(name))
    {
        const HANDLE result = reinterpret_cast<HANDLE>(kDeviceMockHandleBase + 1);
        ReportVfsCreateFileResult("device",
                                  name,
                                  nullptr,
                                  result,
                                  ERROR_SUCCESS);
        ReportVfsDeviceOpen("CreateFileA",
                            name,
                            result,
                            ERROR_SUCCESS);
        SetLastError(ERROR_SUCCESS);
        return result;
    }
    char path[MAX_PATH] = {};
    char source[MAX_PATH] = {};
    const bool write = (access & (GENERIC_WRITE | FILE_APPEND_DATA | DELETE)) != 0;
    if (!MapVfsPath(name, write, path, source))
    {
        ReportVfsCreateFileResult("unmapped",
                                  name,
                                  nullptr,
                                  INVALID_HANDLE_VALUE,
                                  ERROR_INVALID_NAME);
        ReportVfsDeviceOpen("CreateFileA", name, INVALID_HANDLE_VALUE, ERROR_INVALID_NAME);
        ReportVfsAssetOpen("CreateFileA", name, "", INVALID_HANDLE_VALUE, ERROR_INVALID_NAME);
        SetLastError(ERROR_INVALID_NAME);
        return INVALID_HANDLE_VALUE;
    }
    if (!write && !IsRegularFile(path))
    {
        const HANDLE chd_handle = OpenChdReadFile(name, disposition);
        if (chd_handle != INVALID_HANDLE_VALUE)
        {
            ReportVfsCreateFileResult("chd",
                                      name,
                                      "chd://",
                                      chd_handle,
                                      ERROR_SUCCESS);
            return chd_handle;
        }
    }
    if (write)
    {
        bool overlay_exists = IsRegularFile(path);
        const bool source_exists = IsRegularFile(source);
        if (!overlay_exists && IsChdConfigured() && disposition != CREATE_NEW &&
            (disposition == OPEN_EXISTING || disposition == OPEN_ALWAYS ||
             disposition == TRUNCATE_EXISTING) &&
            MaterializeChdFile(name, path))
        {
            overlay_exists = true;
        }
        if (disposition == CREATE_NEW && (overlay_exists || source_exists))
        {
            ReportVfsCreateFileResult("overlay-exists",
                                      name,
                                      path,
                                      INVALID_HANDLE_VALUE,
                                      ERROR_FILE_EXISTS);
            SetLastError(ERROR_FILE_EXISTS);
            return INVALID_HANDLE_VALUE;
        }
        if ((disposition == OPEN_EXISTING || disposition == TRUNCATE_EXISTING) &&
            !overlay_exists && !source_exists)
        {
            ReportVfsCreateFileResult("overlay-missing",
                                      name,
                                      path,
                                      INVALID_HANDLE_VALUE,
                                      ERROR_FILE_NOT_FOUND);
            SetLastError(ERROR_FILE_NOT_FOUND);
            return INVALID_HANDLE_VALUE;
        }
        if (!overlay_exists && source_exists && disposition != CREATE_ALWAYS &&
            !EnsureParentDirectories(path))
        {
            ReportVfsCreateFileResult("overlay-parent",
                                      name,
                                      path,
                                      INVALID_HANDLE_VALUE,
                                      ERROR_PATH_NOT_FOUND);
            SetLastError(ERROR_PATH_NOT_FOUND);
            return INVALID_HANDLE_VALUE;
        }
        if (!overlay_exists && source_exists && disposition != CREATE_ALWAYS &&
            CopyFileA(source, path, TRUE) == FALSE)
        {
            const DWORD error = GetLastError();
            ReportVfsCreateFileResult("overlay-copy",
                                      name,
                                      path,
                                      INVALID_HANDLE_VALUE,
                                      error);
            SetLastError(error);
            return INVALID_HANDLE_VALUE;
        }
        if (!EnsureParentDirectories(path))
        {
            ReportVfsCreateFileResult("overlay-parent",
                                      name,
                                      path,
                                      INVALID_HANDLE_VALUE,
                                      ERROR_PATH_NOT_FOUND);
            SetLastError(ERROR_PATH_NOT_FOUND);
            return INVALID_HANDLE_VALUE;
        }
    }
    // Windows 9x did not enforce the FILE_FLAG_NO_BUFFERING alignment rules, so
    // the original opens scene scripts with that flag and then reads a whole
    // non-sector-multiple file into an unaligned buffer. The NT kernel enforces
    // them and fails that read, so the flag is dropped here. Only the caching
    // policy changes; the bytes the guest receives are the same.
    const HANDLE result = CreateFileA(path,
                                      access,
                                      share,
                                      security,
                                      disposition,
                                      flags & ~static_cast<DWORD>(FILE_FLAG_NO_BUFFERING),
                                      template_handle);
    const DWORD error = result == INVALID_HANDLE_VALUE ? GetLastError() : ERROR_SUCCESS;
    ReportVfsCreateFileResult("native", name, path, result, error);
    ReportVfsAssetOpen("CreateFileA", name, path, result, error);
    SetLastError(error);
    return result;
}

extern "C" __declspec(dllexport) BOOL WINAPI Re2djVfsReadFile(
    HANDLE handle, LPVOID buffer, DWORD size, LPDWORD transferred, LPOVERLAPPED overlapped)
{
    OutputDebugStringA(kFileApiMessage);
    if (IsDeviceMockHandle(handle))
    {
        if (transferred != nullptr)
        {
            *transferred = 0;
        }
        SetLastError(ERROR_SUCCESS);
        return TRUE;
    }
    if (ChdFileHandle* chd_handle = LookupChdFileHandle(handle); chd_handle != nullptr)
    {
        if (overlapped != nullptr || (buffer == nullptr && size != 0))
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }
        const std::size_t request = size;
        const std::size_t available =
            chd_handle->position >= chd_handle->size
                ? 0
                : static_cast<std::size_t>(chd_handle->size - chd_handle->position);
        const std::size_t count = std::min(request, available);
        std::string error;
        if (count != 0 && !EnsureChdMounted())
        {
            SetLastError(ERROR_INVALID_DATA);
            return FALSE;
        }
        if (count != 0 && !g_chd_volume->ReadFileRange(chd_handle->relative_path,
                                                        chd_handle->position,
                                                        buffer,
                                                        count,
                                                        &error))
        {
            SetLastError(ERROR_READ_FAULT);
            return FALSE;
        }
        chd_handle->position += count;
        if (transferred != nullptr)
        {
            *transferred = static_cast<DWORD>(count);
        }
        SetLastError(ERROR_SUCCESS);
        return TRUE;
    }
    return ReadFile(handle, buffer, size, transferred, overlapped);
}

extern "C" __declspec(dllexport) BOOL WINAPI Re2djVfsWriteFile(
    HANDLE handle, LPCVOID buffer, DWORD size, LPDWORD transferred, LPOVERLAPPED overlapped)
{
    OutputDebugStringA(kFileApiMessage);
    if (IsDeviceMockHandle(handle))
    {
        if (transferred != nullptr)
        {
            *transferred = 0;
        }
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }
    if (LookupChdFileHandle(handle) != nullptr)
    {
        if (transferred != nullptr)
        {
            *transferred = 0;
        }
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }
    return WriteFile(handle, buffer, size, transferred, overlapped);
}

extern "C" __declspec(dllexport) DWORD WINAPI Re2djVfsSetFilePointer(
    HANDLE handle, LONG distance, PLONG distance_high, DWORD method)
{
    OutputDebugStringA(kFileApiMessage);
    if (IsDeviceMockHandle(handle))
    {
        SetLastError(ERROR_INVALID_FUNCTION);
        return INVALID_SET_FILE_POINTER;
    }
    if (ChdFileHandle* chd_handle = LookupChdFileHandle(handle); chd_handle != nullptr)
    {
        const std::uint64_t distance_bits =
            static_cast<std::uint32_t>(distance) |
            (static_cast<std::uint64_t>(static_cast<std::uint32_t>(
                 distance_high == nullptr ? 0 : *distance_high))
             << 32);
        const std::int64_t signed_distance = distance_high == nullptr
                                                  ? static_cast<std::int64_t>(distance)
                                                  : static_cast<std::int64_t>(distance_bits);
        const std::int64_t base = method == FILE_BEGIN
                                      ? 0
                                      : (method == FILE_CURRENT
                                             ? static_cast<std::int64_t>(chd_handle->position)
                                             : (method == FILE_END
                                                    ? static_cast<std::int64_t>(chd_handle->size)
                                                    : -1));
        if (base < 0)
        {
            SetLastError(ERROR_NEGATIVE_SEEK);
            return INVALID_SET_FILE_POINTER;
        }
        std::uint64_t next = 0;
        if (signed_distance < 0)
        {
            const std::uint64_t magnitude = static_cast<std::uint64_t>(-(signed_distance + 1)) + 1;
            if (magnitude > static_cast<std::uint64_t>(base))
            {
                SetLastError(ERROR_NEGATIVE_SEEK);
                return INVALID_SET_FILE_POINTER;
            }
            next = static_cast<std::uint64_t>(base) - magnitude;
        }
        else
        {
            if (static_cast<std::uint64_t>(base) >
                static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)()) -
                    static_cast<std::uint64_t>(signed_distance))
            {
                SetLastError(ERROR_SEEK);
                return INVALID_SET_FILE_POINTER;
            }
            next = static_cast<std::uint64_t>(base) + static_cast<std::uint64_t>(signed_distance);
        }
        if (next > chd_handle->size)
        {
            SetLastError(ERROR_SEEK);
            return INVALID_SET_FILE_POINTER;
        }
        chd_handle->position = next;
        if (distance_high != nullptr)
        {
            *distance_high = static_cast<LONG>(next >> 32);
        }
        SetLastError(ERROR_SUCCESS);
        return static_cast<DWORD>(next);
    }
    return SetFilePointer(handle, distance, distance_high, method);
}

extern "C" __declspec(dllexport) DWORD WINAPI Re2djVfsGetFileSize(
    HANDLE handle, LPDWORD high)
{
    OutputDebugStringA(kFileApiMessage);
    if (IsDeviceMockHandle(handle))
    {
        SetLastError(ERROR_INVALID_FUNCTION);
        return INVALID_FILE_SIZE;
    }
    if (ChdFileHandle* chd_handle = LookupChdFileHandle(handle); chd_handle != nullptr)
    {
        if (high != nullptr)
        {
            *high = 0;
        }
        SetLastError(ERROR_SUCCESS);
        return chd_handle->size;
    }
    return GetFileSize(handle, high);
}

extern "C" __declspec(dllexport) BOOL WINAPI Re2djVfsCloseHandle(HANDLE handle)
{
    OutputDebugStringA(kFileApiMessage);
    if (IsDeviceMockHandle(handle))
    {
        SetLastError(ERROR_SUCCESS);
        return TRUE;
    }
    if (ChdFileHandle* chd_handle = LookupChdFileHandle(handle); chd_handle != nullptr)
    {
        chd_handle->used = false;
        chd_handle->position = 0;
        chd_handle->size = 0;
        chd_handle->relative_path.clear();
        SetLastError(ERROR_SUCCESS);
        return TRUE;
    }
    return CloseHandle(handle);
}

extern "C" __declspec(dllexport) DWORD WINAPI Re2djVfsGetFileType(HANDLE handle)
{
    OutputDebugStringA(kFileApiMessage);
    if (IsDeviceMockHandle(handle))
    {
        SetLastError(ERROR_SUCCESS);
        return FILE_TYPE_CHAR;
    }
    if (LookupChdFileHandle(handle) != nullptr)
    {
        SetLastError(ERROR_SUCCESS);
        return FILE_TYPE_DISK;
    }
    return GetFileType(handle);
}

extern "C" __declspec(dllexport) BOOL WINAPI Re2djDeviceIoControlMock(
    HANDLE handle,
    DWORD control_code,
    LPVOID input,
    DWORD input_size,
    LPVOID output,
    DWORD output_size,
    LPDWORD bytes_returned,
    LPOVERLAPPED overlapped)
{
    OutputDebugStringA(kDeviceIoControlMessage);
    if (IsDeviceMockHandle(handle))
    {
        ReportDeviceIoControlCode(control_code, input_size, output_size);
        BOOL device_result = FALSE;
        if (CompleteHardlockRequest(control_code,
                                   input,
                                   input_size,
                                   output,
                                   output_size,
                                   bytes_returned,
                                   &device_result))
        {
            return device_result;
        }
        if (g_re2dj_hardlock_response_450_enabled != 0 &&
            control_code == 0x9c402450)
        {
            if (bytes_returned != nullptr)
            {
                *bytes_returned = 0;
            }
            if (input == nullptr || input_size != 6)
            {
                SetLastError(ERROR_INVALID_DATA);
                return FALSE;
            }
            if (output == nullptr || output_size != 6)
            {
                SetLastError(ERROR_INSUFFICIENT_BUFFER);
                return FALSE;
            }
            std::memcpy(output,
                        g_re2dj_hardlock_response_450,
                        sizeof(g_re2dj_hardlock_response_450));
            if (bytes_returned != nullptr)
            {
                *bytes_returned = sizeof(g_re2dj_hardlock_response_450);
            }
            SetLastError(ERROR_SUCCESS);
            return TRUE;
        }
        if (g_re2dj_hardlock_44c_tail_enabled != 0 &&
            control_code == 0x9c40244c)
        {
            if (bytes_returned != nullptr)
            {
                *bytes_returned = 0;
            }
            if (input == nullptr || input_size != 256)
            {
                SetLastError(ERROR_INVALID_DATA);
                return FALSE;
            }
            if (output == nullptr || output_size != 256)
            {
                SetLastError(ERROR_INSUFFICIENT_BUFFER);
                return FALSE;
            }
            re2dj::hle::hardlock::HardlockApiDescriptorHeader header;
            if (!re2dj::hle::hardlock::ParseHardlockApiDescriptorHeader(
                    std::span<const std::uint8_t>(
                        static_cast<const std::uint8_t*>(input), input_size),
                    &header))
            {
                SetLastError(ERROR_INVALID_DATA);
                return FALSE;
            }
            if (header.function == 0)
            {
                const std::uint16_t tail_word =
                    static_cast<std::uint16_t>(g_re2dj_hardlock_44c_tail_word);
                std::memcpy(static_cast<unsigned char*>(output) + 0xfe,
                            &tail_word,
                            sizeof(tail_word));
                if (bytes_returned != nullptr)
                {
                    *bytes_returned = output_size;
                }
                SetLastError(ERROR_SUCCESS);
                return TRUE;
            }
        }
        if (g_re2dj_device_ioctl_mode == 4)
        {
            if (bytes_returned != nullptr)
            {
                *bytes_returned = 0;
            }
            if (control_code == 0x9c406410)
            {
                if (output == nullptr || output_size < 8)
                {
                    SetLastError(ERROR_INSUFFICIENT_BUFFER);
                    return FALSE;
                }
                std::memset(output, 0, 8);
                if (bytes_returned != nullptr)
                {
                    *bytes_returned = 8;
                }
                SetLastError(ERROR_SUCCESS);
                return TRUE;
            }
            if (control_code != 0x9c406414)
            {
                SetLastError(ERROR_INVALID_FUNCTION);
                return FALSE;
            }
            if (input == nullptr || input_size < 4)
            {
                SetLastError(ERROR_INVALID_DATA);
                return FALSE;
            }
            if (output == nullptr || output_size < 104)
            {
                SetLastError(ERROR_INSUFFICIENT_BUFFER);
                return FALSE;
            }
            const auto* const input_bytes = static_cast<const unsigned char*>(input);
            const std::uint32_t seed =
                static_cast<std::uint32_t>(input_bytes[0]) |
                (static_cast<std::uint32_t>(input_bytes[1]) << 8) |
                (static_cast<std::uint32_t>(input_bytes[2]) << 16) |
                (static_cast<std::uint32_t>(input_bytes[3]) << 24);
            re2dj::device::LptdiTargetState target_state = {};
            std::memcpy(target_state.data(),
                        g_re2dj_device_target_state,
                        target_state.size());
            const re2dj::device::LptdiTargetState response =
                re2dj::device::EncodeLptdiTargetState(seed, target_state);
            std::memset(output, 0, 104);
            std::memcpy(static_cast<unsigned char*>(output) + 4,
                        response.data(),
                        response.size());
            if (bytes_returned != nullptr)
            {
                *bytes_returned = 104;
            }
            SetLastError(ERROR_SUCCESS);
            return TRUE;
        }
        if (g_re2dj_device_ioctl_mode == 3)
        {
            const unsigned char* response = nullptr;
            DWORD response_size = 0;
            DWORD response_capacity = 0;
            if (control_code == 0x9c406410)
            {
                response = g_re2dj_device_response_410;
                response_size = g_re2dj_device_response_410_size;
                response_capacity = sizeof(g_re2dj_device_response_410);
            }
            else if (control_code == 0x9c406414)
            {
                response = g_re2dj_device_response_414;
                response_size = g_re2dj_device_response_414_size;
                response_capacity = sizeof(g_re2dj_device_response_414);
            }
            if (bytes_returned != nullptr)
            {
                *bytes_returned = 0;
            }
            if (response == nullptr || response_size == 0)
            {
                SetLastError(ERROR_INVALID_FUNCTION);
                return FALSE;
            }
            if (response_size > response_capacity)
            {
                SetLastError(ERROR_INVALID_DATA);
                return FALSE;
            }
            if (output == nullptr || output_size < response_size)
            {
                SetLastError(ERROR_INSUFFICIENT_BUFFER);
                return FALSE;
            }
            std::memcpy(output, response, response_size);
            if (bytes_returned != nullptr)
            {
                *bytes_returned = response_size;
            }
            SetLastError(ERROR_SUCCESS);
            return TRUE;
        }
        if (bytes_returned != nullptr)
        {
            *bytes_returned = g_re2dj_device_ioctl_mode == 2 && output != nullptr
                                  ? output_size
                                  : 0;
        }
        SetLastError(ERROR_SUCCESS);
        return TRUE;
    }
    return DeviceIoControl(handle,
                           control_code,
                           input,
                           input_size,
                           output,
                           output_size,
                           bytes_returned,
                           overlapped);
}

// The protected 3rd executable resolves its device APIs dynamically. Keep the
// dynamic hook narrow so unrelated GetProcAddress requests retain Win32
// behavior while device operations use the same wrappers as static imports.
BOOL WINAPI Re2djObserveWtsQuerySessionInformationA(
    HANDLE server,
    DWORD session_id,
    DWORD info_class,
    LPSTR* buffer,
    DWORD* bytes_returned)
{
    if (g_original_wts_query_session_information_a == nullptr)
    {
        SetLastError(ERROR_PROC_NOT_FOUND);
        return FALSE;
    }
    const BOOL result = g_original_wts_query_session_information_a(
        server, session_id, info_class, buffer, bytes_returned);
    const DWORD error = GetLastError();
    constexpr DWORD kWtsCurrentSession = 0xffffffffu;
    constexpr DWORD kWtsConnectState = 4;
    if (result != FALSE && g_re2dj_wts_console_session_mock != 0 &&
        session_id == kWtsCurrentSession && info_class == kWtsConnectState &&
        buffer != nullptr && *buffer != nullptr && bytes_returned != nullptr &&
        *bytes_returned == sizeof(std::uint32_t))
    {
        const std::uint32_t active_state = 0;
        std::memcpy(*buffer, &active_state, sizeof(active_state));
    }
    ReportWtsQuery(session_id, info_class, result, buffer, bytes_returned);
    SetLastError(error);
    return result;
}

extern "C" __declspec(dllexport) FARPROC WINAPI Re2djHleGetProcAddress(
    HMODULE module, LPCSTR name)
{
    EnsureDiagnosticBoundariesInstalled();
    const std::uintptr_t caller =
        reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    if (name != nullptr && reinterpret_cast<std::uintptr_t>(name) > 0xffffu)
    {
        // The host display mode is never changed, so this boundary is not tied
        // to any diagnostic flag. A guest that resolves its imports through
        // GetProcAddress would otherwise reach the real API.
        if (_stricmp(name, "ChangeDisplaySettingsExA") == 0)
        {
            const FARPROC result =
                reinterpret_cast<FARPROC>(&Re2djHleChangeDisplaySettingsExA);
            ReportDynamicResolverName(
                name, "hle", reinterpret_cast<std::uintptr_t>(result), caller);
            return result;
        }
        if (_stricmp(name, "ChangeDisplaySettingsA") == 0)
        {
            const FARPROC result =
                reinterpret_cast<FARPROC>(&Re2djHleChangeDisplaySettingsA);
            ReportDynamicResolverName(
                name, "hle", reinterpret_cast<std::uintptr_t>(result), caller);
            return result;
        }
        if (_stricmp(name, "DirectInputCreateA") == 0)
        {
            const FARPROC result =
                reinterpret_cast<FARPROC>(&Re2djHleDirectInputCreateA);
            ReportDynamicResolverName(
                name, "hle", reinterpret_cast<std::uintptr_t>(result), caller);
            return result;
        }
        if (_stricmp(name, "DirectSoundCreate") == 0)
        {
            const FARPROC result =
                reinterpret_cast<FARPROC>(&Re2djHleDirectSoundCreate);
            ReportDynamicResolverName(
                name, "hle", reinterpret_cast<std::uintptr_t>(result), caller);
            return result;
        }
        if (g_re2dj_vfs_dynamic_resolver != 0)
        {
            if (_stricmp(name, "CreateFileA") == 0)
            {
                const FARPROC result =
                    reinterpret_cast<FARPROC>(&Re2djVfsCreateFileA);
                ReportDynamicResolverName(
                    name, "hle", reinterpret_cast<std::uintptr_t>(result), caller);
                return result;
            }
            if (_stricmp(name, "ReadFile") == 0)
            {
                const FARPROC result =
                    reinterpret_cast<FARPROC>(&Re2djVfsReadFile);
                ReportDynamicResolverName(
                    name, "hle", reinterpret_cast<std::uintptr_t>(result), caller);
                return result;
            }
            if (_stricmp(name, "WriteFile") == 0)
            {
                const FARPROC result =
                    reinterpret_cast<FARPROC>(&Re2djVfsWriteFile);
                ReportDynamicResolverName(
                    name, "hle", reinterpret_cast<std::uintptr_t>(result), caller);
                return result;
            }
            if (_stricmp(name, "SetFilePointer") == 0)
            {
                const FARPROC result =
                    reinterpret_cast<FARPROC>(&Re2djVfsSetFilePointer);
                ReportDynamicResolverName(
                    name, "hle", reinterpret_cast<std::uintptr_t>(result), caller);
                return result;
            }
            if (_stricmp(name, "GetFileSize") == 0)
            {
                const FARPROC result =
                    reinterpret_cast<FARPROC>(&Re2djVfsGetFileSize);
                ReportDynamicResolverName(
                    name, "hle", reinterpret_cast<std::uintptr_t>(result), caller);
                return result;
            }
            if (_stricmp(name, "CloseHandle") == 0)
            {
                const FARPROC result =
                    reinterpret_cast<FARPROC>(&Re2djVfsCloseHandle);
                ReportDynamicResolverName(
                    name, "hle", reinterpret_cast<std::uintptr_t>(result), caller);
                return result;
            }
            if (_stricmp(name, "GetFileType") == 0)
            {
                const FARPROC result =
                    reinterpret_cast<FARPROC>(&Re2djVfsGetFileType);
                ReportDynamicResolverName(
                    name, "hle", reinterpret_cast<std::uintptr_t>(result), caller);
                return result;
            }
            // The guest ends itself through dynamically resolved exit APIs, so
            // this is the only place those calls can be seen. Both are covered
            // because an observed exit took the terminate path instead.
            if (_stricmp(name, "ExitProcess") == 0)
            {
                const FARPROC result =
                    reinterpret_cast<FARPROC>(&Re2djHleExitProcess);
                ReportDynamicResolverName(
                    name, "observe", reinterpret_cast<std::uintptr_t>(result), caller);
                return result;
            }
            if (_stricmp(name, "TerminateProcess") == 0)
            {
                const FARPROC result =
                    reinterpret_cast<FARPROC>(&Re2djHleTerminateProcess);
                ReportDynamicResolverName(
                    name, "observe", reinterpret_cast<std::uintptr_t>(result), caller);
                return result;
            }
            // The guest opens resources by bare name after changing directory,
            // so these two decide what every later relative open means.
            if (_stricmp(name, "SetCurrentDirectoryA") == 0)
            {
                const FARPROC result =
                    reinterpret_cast<FARPROC>(&Re2djVfsSetCurrentDirectoryA);
                ReportDynamicResolverName(
                    name, "hle", reinterpret_cast<std::uintptr_t>(result), caller);
                return result;
            }
            if (_stricmp(name, "GetCurrentDirectoryA") == 0)
            {
                const FARPROC result =
                    reinterpret_cast<FARPROC>(&Re2djVfsGetCurrentDirectoryA);
                ReportDynamicResolverName(
                    name, "hle", reinterpret_cast<std::uintptr_t>(result), caller);
                return result;
            }
            if (_stricmp(name, "DirectDrawCreate") == 0)
            {
                const FARPROC result =
                    reinterpret_cast<FARPROC>(&Re2djHleDirectDrawCreate);
                ReportDynamicResolverName(
                    name, "hle", reinterpret_cast<std::uintptr_t>(result), caller);
                return result;
            }
            if (_stricmp(name, "DirectDrawCreateEx") == 0)
            {
                const FARPROC result =
                    reinterpret_cast<FARPROC>(&Re2djHleDirectDrawCreateEx);
                ReportDynamicResolverName(
                    name, "hle", reinterpret_cast<std::uintptr_t>(result), caller);
                return result;
            }
        }
        if (g_re2dj_device_mock != 0)
        {
            if (_stricmp(name, "DeviceIoControl") == 0)
            {
                const FARPROC result =
                    reinterpret_cast<FARPROC>(&Re2djDeviceIoControlMock);
                ReportDynamicResolverName(
                    name, "hle", reinterpret_cast<std::uintptr_t>(result), caller);
                return result;
            }
            if (_stricmp(name, "WTSQuerySessionInformationA") == 0)
            {
                const FARPROC original = GetProcAddress(module, name);
                if (original != nullptr)
                {
                    g_original_wts_query_session_information_a =
                        reinterpret_cast<WtsQuerySessionInformationAProc>(original);
                    const FARPROC result =
                        reinterpret_cast<FARPROC>(&Re2djObserveWtsQuerySessionInformationA);
                    ReportDynamicResolverName(name,
                                              "observe",
                                              reinterpret_cast<std::uintptr_t>(result),
                                              caller);
                    return result;
                }
            }
        }
    }
    if (reinterpret_cast<std::uintptr_t>(name) == 1)
    {
        char module_name[MAX_PATH] = {};
        if (module != nullptr && GetModuleFileNameA(module, module_name, sizeof(module_name)) != 0 &&
            strstr(module_name, "DSOUND") != nullptr)
        {
            const FARPROC result =
                reinterpret_cast<FARPROC>(&Re2djHleDirectSoundCreate);
            ReportDynamicResolverName(
                "#1", "hle", reinterpret_cast<std::uintptr_t>(result), caller);
            return result;
        }
    }
    const FARPROC result = GetProcAddress(module, name);
    if (name != nullptr && reinterpret_cast<std::uintptr_t>(name) > 0xffffu)
    {
        ReportDynamicResolverName(
            name, "win32", reinterpret_cast<std::uintptr_t>(result), caller);
    }
    return result;
}

extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI Re2djProbeExitProcess(UINT code)
{
    char message[96] = {};
    std::snprintf(message,
                  sizeof(message),
                  "re2dj:probe:ExitProcess:code=0x%08x:return=0x%08x",
                  static_cast<unsigned>(code),
                  static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(_ReturnAddress())));
    OutputDebugStringA(message);
    OutputDebugStringA(kExitProcessMessage);
    ExitProcess(code);
}

extern "C" __declspec(dllexport) void __declspec(naked) Re2djProbeGetCommandLineA()
{
    __asm
    {
        pushfd
        pushad
        push offset kProbeMessage
        call OutputDebugStringA
        add esp, 4
        popad
        popfd
        jmp dword ptr [g_re2dj_probe_original_target]
    }
}

extern "C" __declspec(dllexport) void __declspec(naked) Re2djHleGetCommandLineA()
{
    __asm
    {
        pushfd
        pushad
        push offset kHleMessage
        call OutputDebugStringA
        add esp, 4
        popad
        popfd
        mov eax, offset g_re2dj_hle_command_line
        ret
    }
}

extern "C" __declspec(dllexport) void __declspec(naked) Re2djHleGetWindowsDirectoryA()
{
    __asm
    {
        push offset kWindowsDirectoryMessage
        call OutputDebugStringA
        add esp, 4
        mov eax, dword ptr [esp+4]
        test eax, eax
        je no_copy
        push dword ptr [esp+8]
        push offset g_re2dj_hle_windows_directory
        push eax
        call lstrcpynA
    no_copy:
        push offset g_re2dj_hle_windows_directory
        call lstrlenA
        ret 8
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(module);
        if (AddVectoredExceptionHandler(1, HandleLegacyIoPortException) == nullptr)
        {
            return FALSE;
        }
    }
    if (reason == DLL_PROCESS_DETACH &&
        InterlockedCompareExchange(&g_exit_wrapper_fired, 0, 0) == 0)
    {
        // Detach is the one point every exit crosses. Writing a trace line
        // here runs under the loader lock, which is why it is limited to the
        // two records that make an exit attributable and nothing else.
        ReportExitDetach();
        ReportExitProcessHardlock();
    }
    return TRUE;
}
