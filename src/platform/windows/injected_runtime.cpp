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

#include "re2dj/device/hardlock_api_descriptor.h"
#include "re2dj/device/hardlock_protocol.h"
#include "re2dj/device/lptdi_challenge_response.h"
#include "re2dj/input/legacy_io_port_bus.h"
#include "re2dj/storage/fat32_chd.h"
#include "ez2dj_keyboard_input.h"

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
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_hardlock_secret_enabled = 0;
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_hardlock_module_address = 0;
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_hardlock_seed1 = 0;
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_hardlock_seed2 = 0;
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_hardlock_seed3 = 0;
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_wts_console_session_mock = 0;
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_hle_io_ports = 0;
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_io_image_base = 0;
extern "C" __declspec(dllexport) char g_re2dj_io_config_path[MAX_PATH] = {};

namespace
{

constexpr char kProbeMessage[] = "re2dj:handoff:GetCommandLineA";
constexpr char kHleMessage[] = "re2dj:hle:GetCommandLineA";
constexpr char kWindowsDirectoryMessage[] = "re2dj:hle:GetWindowsDirectoryA";
constexpr char kCreateFileMessage[] = "re2dj:vfs:CreateFileA";
constexpr char kFileApiMessage[] = "re2dj:vfs:file-api";
constexpr char kDeviceIoControlMessage[] = "re2dj:device:DeviceIoControl";
constexpr char kDisplayModeMessage[] = "re2dj:hle:ChangeDisplaySettingsExA";
constexpr char kExitProcessMessage[] = "re2dj:probe:ExitProcess";

re2dj::input::LegacyIoPortBus g_legacy_io_port_bus;
re2dj::device::HardlockProtocolTracker g_hardlock_protocol_tracker;
re2dj::platform::windows::Ez2DjKeyboardInput g_keyboard_input;
volatile LONG g_keyboard_input_state = 0;
volatile LONG g_vfs_image_trace_count = 0;
volatile LONG g_vfs_script_trace_count = 0;
volatile LONG g_vfs_device_trace_count = 0;
volatile LONG g_vfs_open_trace_count = 0;
volatile LONG g_dynamic_resolver_trace_count = 0;
volatile LONG g_dynamic_resolver_caller_trace_count = 0;
volatile LONG g_wts_query_trace_count = 0;
volatile LONG g_hardlock_450_trace_count = 0;

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

void ReportHardlock450Packet(DWORD control_code,
                             const void* input,
                             DWORD input_size,
                             const void* output,
                             DWORD output_size)
{
    constexpr DWORD kHardlockQuery = 0x9c402450;
    constexpr DWORD kPacketSize = 6;
    constexpr LONG kMaximumPacketDiagnostics = 16;
    if (control_code != kHardlockQuery || input == nullptr || output == nullptr ||
        input_size != kPacketSize || output_size != kPacketSize)
    {
        return;
    }
    const LONG index = InterlockedIncrement(&g_hardlock_450_trace_count);
    if (index > kMaximumPacketDiagnostics)
    {
        return;
    }
    const auto* const bytes = static_cast<const unsigned char*>(input);
    char message[192] = {};
    std::snprintf(message,
                  sizeof(message),
                  "re2dj:vfs:hardlock-450-packet:index=%ld:in_place=%u:"
                  "input=%02x%02x%02x%02x%02x%02x\r\n",
                  static_cast<long>(index),
                  input == output ? 1U : 0U,
                  static_cast<unsigned>(bytes[0]),
                  static_cast<unsigned>(bytes[1]),
                  static_cast<unsigned>(bytes[2]),
                  static_cast<unsigned>(bytes[3]),
                  static_cast<unsigned>(bytes[4]),
                  static_cast<unsigned>(bytes[5]));
    AppendVfsTraceMessage(message);
}

void FormatHexBytes(const std::array<std::uint8_t, 8>& bytes,
                    char output[17])
{
    constexpr char kDigits[] = "0123456789abcdef";
    for (std::size_t index = 0; index < bytes.size(); ++index)
    {
        output[index * 2] = kDigits[bytes[index] >> 4];
        output[index * 2 + 1] = kDigits[bytes[index] & 0x0f];
    }
    output[16] = '\0';
}

void ReportHardlockDescriptor(DWORD control_code,
                              const void* input,
                              DWORD input_size)
{
    constexpr DWORD kHardlockApiCall = 0x9c40244c;
    constexpr DWORD kHardlockEnvelopeCall = 0x9c402458;
    if ((control_code != kHardlockApiCall &&
         control_code != kHardlockEnvelopeCall) ||
        input == nullptr ||
        input_size < re2dj::device::kHardlockApiDescriptorSize ||
        !ClaimVfsDeviceTraceBudget())
    {
        return;
    }

    const auto* const input_bytes = static_cast<const std::uint8_t*>(input);
    re2dj::device::HardlockApiDescriptorHeader header;
    if (!re2dj::device::ParseHardlockApiDescriptorHeader(
            std::span<const std::uint8_t>(input_bytes, input_size), &header))
    {
        return;
    }
    std::uint16_t tail_word = 0;
    if (!re2dj::device::ParseHardlockApiDescriptorTailWord(
            std::span<const std::uint8_t>(input_bytes, input_size), &tail_word))
    {
        return;
    }

    if (g_re2dj_hardlock_secret_enabled != 0)
    {
        char message[240] = {};
        std::snprintf(
            message,
            sizeof(message),
            "re2dj:vfs:hardlock-descriptor:code=0x%08lx:function=0x%04x:status=%u:secret_fields=redacted\r\n",
            static_cast<unsigned long>(control_code),
            static_cast<unsigned>(header.function),
            static_cast<unsigned>(header.status));
        AppendVfsTraceMessage(message);
        return;
    }

    char reference[17] = {};
    char verify[17] = {};
    FormatHexBytes(header.id_reference, reference);
    FormatHexBytes(header.id_verify, verify);
    char message[440] = {};
    std::snprintf(
        message,
        sizeof(message),
        "re2dj:vfs:hardlock-descriptor:code=0x%08lx:version=%02x%02x:module_id=0x%04x:module_address=0x%04x:block_count=%u:function=0x%04x:status=%u:remote=%u:port=0x%04x:id_ref=%s:id_verify=%s:tail_word=0x%04x\r\n",
        static_cast<unsigned long>(control_code),
        static_cast<unsigned>(header.api_version[0]),
        static_cast<unsigned>(header.api_version[1]),
        static_cast<unsigned>(header.module_id),
        static_cast<unsigned>(header.module_address),
        static_cast<unsigned>(header.block_count),
        static_cast<unsigned>(header.function),
        static_cast<unsigned>(header.status),
        static_cast<unsigned>(header.remote),
        static_cast<unsigned>(header.port),
        reference,
        verify,
        static_cast<unsigned>(tail_word));
    AppendVfsTraceMessage(message);
}

void ReportHardlockProtocol(DWORD control_code,
                            const void* input,
                            DWORD input_size,
                            DWORD output_size)
{
    if (g_re2dj_hardlock_secret_enabled == 0)
    {
        return;
    }
    const auto* const input_bytes = static_cast<const std::uint8_t*>(input);
    const std::span<const std::uint8_t> input_span =
        input_bytes == nullptr ? std::span<const std::uint8_t>()
                               : std::span<const std::uint8_t>(input_bytes, input_size);
    const re2dj::device::HardlockRequestObservation observation =
        g_hardlock_protocol_tracker.Observe(
            control_code,
            input_span,
            output_size,
            static_cast<std::uint16_t>(g_re2dj_hardlock_module_address));
    if (observation.kind == re2dj::device::HardlockRequestKind::kUnknown)
    {
        return;
    }

    char message[280] = {};
    std::snprintf(message,
                  sizeof(message),
                  "re2dj:vfs:hardlock-protocol:request=%s:shape=%u:sequence=%u:"
                  "descriptor=%u:function=%u:block_count=%u:module_match=%u\r\n",
                  re2dj::device::HardlockRequestKindName(observation.kind),
                  observation.shape_valid ? 1u : 0u,
                  observation.sequence_valid ? 1u : 0u,
                  observation.descriptor_valid ? 1u : 0u,
                  static_cast<unsigned>(observation.function),
                  static_cast<unsigned>(observation.block_count),
                  observation.descriptor_valid && observation.module_address_matches ? 1u : 0u);
    AppendVfsTraceMessage(message);
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

    constexpr DWORD kInByteRva = 0x00038987;
    constexpr DWORD kOutByteRva = 0x000389ab;
    const DWORD address = static_cast<DWORD>(
        reinterpret_cast<std::uintptr_t>(exception->ExceptionRecord->ExceptionAddress));
    const bool is_read = address == g_re2dj_io_image_base + kInByteRva;
    const bool is_write = address == g_re2dj_io_image_base + kOutByteRva;
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
        if (*text == '\0' || _strnicmp(text, prefix, 1) != 0)
        {
            return false;
        }
    }
    return *text == '\0' || *text == '\\' || *text == '/';
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

bool GuestHddSuffix(const char* name, const char** suffix)
{
    if (name == nullptr || suffix == nullptr)
    {
        return false;
    }
    if (HasPrefixIgnoreCase(name, "D:\\ez2dj"))
    {
        *suffix = name + 8;
        return true;
    }
    if (name[0] != '\\' && name[0] != '/')
    {
        *suffix = name;
        return true;
    }
    return false;
}

bool ChdRelativePath(const char* name, std::string* relative)
{
    const char* suffix = nullptr;
    if (!GuestHddSuffix(name, &suffix) || relative == nullptr)
    {
        return false;
    }
    while (*suffix == '\\' || *suffix == '/')
    {
        ++suffix;
    }
    if (*suffix == '\0')
    {
        return false;
    }
    relative->assign("EZ2DJ/");
    relative->append(suffix);
    for (char& value : *relative)
    {
        if (value == '\\')
        {
            value = '/';
        }
    }
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
    const char* hdd_suffix = nullptr;
    const char* support_suffix = nullptr;
    if (HasPrefixIgnoreCase(name, "D:\\ez2dj"))
    {
        hdd_suffix = name + 8;
    }
    else if (HasPrefixIgnoreCase(name, "C:\\windows"))
    {
        support_suffix = name + 10;
    }
    else if (name[0] != '\\' && name[0] != '/')
    {
        hdd_suffix = name;
    }
    else
    {
        return false;
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

}  // namespace

extern "C" __declspec(dllexport) LONG WINAPI Re2djHleChangeDisplaySettingsExA(
    LPCSTR device_name,
    DEVMODEA* dev_mode,
    HWND window,
    DWORD flags,
    LPVOID reserved)
{
    constexpr DWORD kRequiredFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
    if (device_name == nullptr && dev_mode != nullptr && window == nullptr &&
        flags == CDS_UPDATEREGISTRY && reserved == nullptr &&
        (dev_mode->dmFields & kRequiredFields) == kRequiredFields &&
        dev_mode->dmPelsWidth == 640 && dev_mode->dmPelsHeight == 480 &&
        dev_mode->dmBitsPerPel == 16)
    {
        OutputDebugStringA(kDisplayModeMessage);
        SetLastError(ERROR_SUCCESS);
        return DISP_CHANGE_SUCCESSFUL;
    }
    return ChangeDisplaySettingsExA(device_name, dev_mode, window, flags, reserved);
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
        ReportHardlock450Packet(control_code,
                                input,
                                input_size,
                                output,
                                output_size);
        ReportHardlockDescriptor(control_code, input, input_size);
        ReportHardlockProtocol(control_code, input, input_size, output_size);
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
            re2dj::device::HardlockApiDescriptorHeader header;
            if (!re2dj::device::ParseHardlockApiDescriptorHeader(
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
    const std::uintptr_t caller =
        reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    if (name != nullptr && reinterpret_cast<std::uintptr_t>(name) > 0xffffu)
    {
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
    return TRUE;
}
