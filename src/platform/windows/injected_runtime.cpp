#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cctype>
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
#include "re2dj/input/ez2dancer_io_port_bus.h"
#include "re2dj/storage/fat32_chd.h"
#include "re2dj/storage/guest_path.h"
#include "direct3d3_com_facade.h"
#include "directdraw7_com_facade.h"
#include "display_mode_boundary.h"
#include "ez2dj_keyboard_input.h"
#include "ini_profile_hle.h"
#include "message_box_boundary.h"
#include "directinput7_com_facade.h"
#include "directsound_com_facade.h"

extern "C" __declspec(dllexport) volatile DWORD g_re2dj_probe_original_target = 0;
extern "C" __declspec(dllexport) char g_re2dj_hle_command_line[MAX_PATH] = {};
extern "C" __declspec(dllexport) char g_re2dj_hle_windows_directory[MAX_PATH] = {};
extern "C" __declspec(dllexport) char g_re2dj_vfs_hdd_root[MAX_PATH] = {};
// The one name by which the guest calls its own root, taken from the profile's
// guest drive letter and directory. The drive letter is a property of the dump,
// not of the product: 1st SE's CHD boots from C:\ez2dj while the dumps this
// runtime was first written against used D:\ez2dj, which stays the default for
// profiles that record no guest path.
extern "C" __declspec(dllexport) char g_re2dj_vfs_guest_root[MAX_PATH] = "D:\\ez2dj";
// The directory inside the CHD that holds the product, used to turn a guest-
// relative path into an image path. It is a property of the image rather than
// of EZ2DJ, so the launcher sets it from the profile's executable path; the
// default only keeps a profile that sets nothing behaving as before.
extern "C" __declspec(dllexport) char g_re2dj_vfs_chd_root[MAX_PATH] = "EZ2DJ";
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
extern "C" __declspec(dllexport) unsigned char g_re2dj_hardlock_transform_responses
    [re2dj::hle::hardlock::kHardlockTransformBlockRowCapacity *
     re2dj::hle::hardlock::kHardlockTransformBlockSize * 2] = {};
// Externally computed request rows, each answering a whole transform payload,
// transferred as the fixed-width records payload_responses.h defines.
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_hardlock_payload_response_count = 0;
extern "C" __declspec(dllexport) unsigned char g_re2dj_hardlock_payload_responses
    [re2dj::hle::hardlock::kHardlockPayloadRecordCapacity *
     re2dj::hle::hardlock::kHardlockPayloadRecordSize] = {};
// Diagnostic-only flag: record hashes of incoming transform blocks without
// exposing the block or response bytes.
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_hardlock_transform_input_trace = 0;
// Diagnostic-only: when enabled, the device rejects any descriptor or transform
// whose header function equals this value. Never set from a profile.
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_hardlock_reject_function_enabled = 0;
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_hardlock_reject_function = 0;
// Explicit one-shot diagnostic output for transform input blocks. The
// launcher supplies a user-selected temporary path; this is never enabled by
// default and does not contain response bytes.
extern "C" __declspec(dllexport) char g_re2dj_hardlock_transform_input_dump[MAX_PATH] = {};
extern "C" __declspec(dllexport) char g_re2dj_hardlock_descriptor_output[MAX_PATH] = {};
extern "C" __declspec(dllexport) char g_re2dj_hardlock_descriptor_profile[MAX_PATH] = {};
extern "C" __declspec(dllexport) volatile LONG g_re2dj_hardlock_descriptor_written = 0;
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_wts_console_session_mock = 0;
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_hle_io_ports = 0;
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_io_image_base = 0;
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_io_in_byte_rva = 0;
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_io_out_byte_rva = 0;
// Non-zero when this profile's board is word-wide. The launcher sets it from
// the profile, so the handler never has to infer the width from the guest.
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_io_word_width = 0;
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
re2dj::input::Ez2DancerIoPortBus g_dancer_io_port_bus;
re2dj::platform::windows::Ez2DjKeyboardInput g_keyboard_input;
volatile LONG g_keyboard_input_state = 0;
volatile LONG g_vfs_image_trace_count = 0;
volatile LONG g_vfs_script_trace_count = 0;
volatile LONG g_vfs_device_trace_count = 0;
volatile LONG g_vfs_io_port_trace_count = 0;
volatile LONG g_vfs_open_trace_count = 0;
volatile LONG g_vfs_file_trace_count = 0;
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

// Raw port accesses are far more frequent than device requests once a guest
// starts polling its board, so they carry their own budget rather than
// competing with the device trace for the same allowance.
bool ClaimVfsIoPortTraceBudget()
{
    constexpr LONG kMaximumIoPortDiagnostics = 256;
    return InterlockedIncrement(&g_vfs_io_port_trace_count) <= kMaximumIoPortDiagnostics;
}

bool ClaimVfsOpenTraceBudget()
{
    // Raised from 128 once a diagnosis needed the whole open sequence: a
    // title screen probes about 60 sprite files before drawing, and the old
    // budget truncated the log mid-pass and made an absent read look like a
    // missing trace. This matches the file-event budget below.
    constexpr LONG kMaximumOpenDiagnostics = 1024;
    return InterlockedIncrement(&g_vfs_open_trace_count) <= kMaximumOpenDiagnostics;
}

LONG ClaimVfsFileTraceBudget()
{
    constexpr LONG kMaximumFileDiagnostics = 1024;
    const LONG event = InterlockedIncrement(&g_vfs_file_trace_count);
    return event <= kMaximumFileDiagnostics ? event : 0;
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

void AppendDiagnosticFile(const char* path, const char* message)
{
    if (path == nullptr || path[0] == '\0' || message == nullptr)
    {
        return;
    }
    HANDLE trace = CreateFileA(path,
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

void AppendVfsTraceMessage(const char* message)
{
    AppendDiagnosticFile(g_re2dj_vfs_trace_path, message);
}

// A file handle the guest opens and then abandons leaves no trace of its own,
// so the two queries a loader makes between opening and reading are recorded
// under the same budget as the read events.
void ReportVfsFileQuery(const char* api, const char* kind, HANDLE handle, DWORD result)
{
    if (g_re2dj_vfs_trace_path[0] == '\0')
    {
        return;
    }
    const LONG event = ClaimVfsFileTraceBudget();
    if (event == 0)
    {
        return;
    }
    char message[256] = {};
    std::snprintf(message,
                  sizeof(message),
                  "re2dj:vfs:file-query:event=%ld:api=%.23s:kind=%.15s:handle=0x%08x:result=%u\r\n",
                  event,
                  api == nullptr ? "unknown" : api,
                  kind == nullptr ? "unknown" : kind,
                  static_cast<unsigned>(reinterpret_cast<ULONG_PTR>(handle)),
                  static_cast<unsigned>(result));
    AppendVfsTraceMessage(message);
}

void ReportVfsReadFileEnter(
    const char* kind, HANDLE handle, DWORD size, LPOVERLAPPED overlapped)
{
    if (g_re2dj_vfs_trace_path[0] == '\0')
    {
        return;
    }
    const LONG event = ClaimVfsFileTraceBudget();
    if (event == 0)
    {
        return;
    }
    char message[256] = {};
    std::snprintf(message,
                  sizeof(message),
                  "re2dj:vfs:read-file-enter:event=%ld:kind=%.15s:handle=0x%08x:size=%u:overlapped=%u\r\n",
                  event,
                  kind == nullptr ? "unknown" : kind,
                  static_cast<unsigned>(reinterpret_cast<ULONG_PTR>(handle)),
                  static_cast<unsigned>(size),
                  overlapped != nullptr ? 1U : 0U);
    AppendVfsTraceMessage(message);
}

void ReportVfsReadFileResult(
    const char* kind, HANDLE handle, BOOL result, DWORD transferred, DWORD error)
{
    if (g_re2dj_vfs_trace_path[0] == '\0')
    {
        return;
    }
    const LONG event = ClaimVfsFileTraceBudget();
    if (event == 0)
    {
        return;
    }
    char message[256] = {};
    std::snprintf(message,
                  sizeof(message),
                  "re2dj:vfs:read-file-result:event=%ld:kind=%.15s:handle=0x%08x:ok=%u:transferred=%u:error=%u\r\n",
                  event,
                  kind == nullptr ? "unknown" : kind,
                  static_cast<unsigned>(reinterpret_cast<ULONG_PTR>(handle)),
                  result != FALSE ? 1U : 0U,
                  static_cast<unsigned>(transferred),
                  static_cast<unsigned>(error));
    AppendVfsTraceMessage(message);
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

// Reads SizeOfImage from the main module's PE header so the stack scan can tell
// image-resident return addresses from everything else.
std::uintptr_t MainImageSize(std::uintptr_t image_base)
{
    if (image_base == 0)
    {
        return 0;
    }
    std::int32_t lfanew = 0;
    SIZE_T copied = 0;
    if (ReadProcessMemory(GetCurrentProcess(),
                          reinterpret_cast<const void*>(image_base + 0x3c),
                          &lfanew,
                          sizeof(lfanew),
                          &copied) == FALSE ||
        copied != sizeof(lfanew) || lfanew <= 0)
    {
        return 0;
    }
    std::uint32_t size_of_image = 0;
    // SizeOfImage sits at optional-header offset 0x38, i.e. PE signature (4) +
    // file header (20) + 0x38 past e_lfanew.
    if (ReadProcessMemory(
            GetCurrentProcess(),
            reinterpret_cast<const void*>(image_base + static_cast<std::uintptr_t>(lfanew) + 0x50),
            &size_of_image,
            sizeof(size_of_image),
            &copied) == FALSE ||
        copied != sizeof(size_of_image))
    {
        return 0;
    }
    return size_of_image;
}

// Records the image-resident return addresses on the stack at exit, so the
// .protect call chain that decided to exit is visible. Only code addresses are
// logged, never data. The scan is bounded and fires once per process.
void ReportExitStackChain(void* return_slot)
{
    if (g_re2dj_vfs_trace_path[0] == '\0' || return_slot == nullptr)
    {
        return;
    }
    const std::uintptr_t image_base =
        reinterpret_cast<std::uintptr_t>(GetModuleHandleA(nullptr));
    const std::uintptr_t image_size = MainImageSize(image_base);
    if (image_base == 0 || image_size == 0)
    {
        return;
    }
    constexpr std::size_t kStackWords = 96;
    std::uint32_t stack[kStackWords] = {};
    SIZE_T copied = 0;
    if (ReadProcessMemory(GetCurrentProcess(),
                          return_slot,
                          stack,
                          sizeof(stack),
                          &copied) == FALSE ||
        copied < sizeof(std::uint32_t))
    {
        return;
    }
    const std::size_t words = copied / sizeof(std::uint32_t);
    char refs[640] = {};
    std::size_t cursor = 0;
    unsigned reported = 0;
    // Each reported return address is preceded by the call that led toward the
    // exit, so a window before it shows what the caller did. 40 before, 8 after.
    constexpr std::size_t kBytesBeforeReturn = 40;
    constexpr std::size_t kBytesAfterReturn = 8;
    for (std::size_t index = 0; index < words && reported < 12; ++index)
    {
        const std::uintptr_t value = stack[index];
        if (value < image_base || value >= image_base + image_size)
        {
            continue;
        }
        const int written = std::snprintf(
            refs + cursor,
            sizeof(refs) - cursor,
            "%s%u:0x%08x",
            reported == 0 ? "" : ",",
            static_cast<unsigned>(index),
            static_cast<unsigned>(value - image_base));
        if (written >= 0 && static_cast<std::size_t>(written) < sizeof(refs) - cursor)
        {
            cursor += static_cast<std::size_t>(written);
        }
        // Dump the call site preceding this return address.
        if (value >= kBytesBeforeReturn)
        {
            const std::uintptr_t window_base = value - kBytesBeforeReturn;
            unsigned char window[kBytesBeforeReturn + kBytesAfterReturn] = {};
            SIZE_T window_copied = 0;
            if (ReadProcessMemory(GetCurrentProcess(),
                                  reinterpret_cast<const void*>(window_base),
                                  window,
                                  sizeof(window),
                                  &window_copied) != FALSE &&
                window_copied != 0)
            {
                char hex[sizeof(window) * 2 + 1] = {};
                for (SIZE_T byte_index = 0; byte_index < window_copied; ++byte_index)
                {
                    std::snprintf(hex + byte_index * 2, 3, "%02x", window[byte_index]);
                }
                char window_message[256] = {};
                std::snprintf(
                    window_message,
                    sizeof(window_message),
                    "re2dj:vfs:exit-stack-code:index=%u:return_rva=0x%08x:window_base=0x%08x:bytes=%s\r\n",
                    static_cast<unsigned>(index),
                    static_cast<unsigned>(value - image_base),
                    static_cast<unsigned>(window_base - image_base),
                    hex);
                AppendVfsTraceMessage(window_message);
            }
        }
        ++reported;
    }
    char message[768] = {};
    std::snprintf(message,
                  sizeof(message),
                  "re2dj:vfs:exit-stack-chain:image_base=0x%08x:image_size=0x%08x:refs=%s\r\n",
                  static_cast<unsigned>(image_base),
                  static_cast<unsigned>(image_size),
                  refs);
    AppendVfsTraceMessage(message);
}

void ReportCrashException(EXCEPTION_POINTERS* exception)
{
    if (exception == nullptr || exception->ExceptionRecord == nullptr ||
        exception->ContextRecord == nullptr)
    {
        return;
    }
    const DWORD code = exception->ExceptionRecord->ExceptionCode;
    // One record per exception code, not one per run. A protection layer that
    // raises a handled fault at entry - as the 1st SE .protect build does with
    // an access violation - used to consume the only slot and mask the fault
    // that actually ends the process. Distinct codes are capped so a repeating
    // fault still cannot fill the log.
    {
        constexpr std::size_t kMaximumReportedCodes = 8;
        static volatile LONG s_reported_codes[kMaximumReportedCodes] = {};
        bool claimed = false;
        for (std::size_t index = 0; index < kMaximumReportedCodes; ++index)
        {
            const LONG seen = InterlockedCompareExchange(
                &s_reported_codes[index], static_cast<LONG>(code), 0);
            if (seen == 0)
            {
                claimed = true;
                break;
            }
            if (static_cast<DWORD>(seen) == code)
            {
                break;
            }
        }
        if (!claimed)
        {
            return;
        }
    }

    const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(
        exception->ExceptionRecord->ExceptionAddress);
    const std::uintptr_t image_base =
        reinterpret_cast<std::uintptr_t>(GetModuleHandleA(nullptr));
    const bool in_image = image_base != 0 && address >= image_base;
    const std::uintptr_t rva = in_image ? (address - image_base) : 0;

    unsigned char code_bytes[16] = {};
    SIZE_T code_copied = 0;
    const BOOL code_readable =
        ReadProcessMemory(GetCurrentProcess(),
                          reinterpret_cast<const void*>(address),
                          code_bytes,
                          sizeof(code_bytes),
                          &code_copied) != FALSE;
    char code_hex[sizeof(code_bytes) * 2 + 1] = {};
    for (SIZE_T i = 0; code_readable && i < code_copied; ++i)
    {
        std::snprintf(code_hex + i * 2, 3, "%02x", code_bytes[i]);
    }

    const std::uintptr_t esp = exception->ContextRecord->Esp;
    DWORD stack_words[8] = {};
    SIZE_T stack_copied = 0;
    const BOOL stack_readable =
        ReadProcessMemory(GetCurrentProcess(),
                          reinterpret_cast<const void*>(esp),
                          stack_words,
                          sizeof(stack_words),
                          &stack_copied) != FALSE;
    char stack_hex[sizeof(stack_words) * 9 + 1] = {};
    int stack_pos = 0;
    for (SIZE_T i = 0; stack_readable && i < (stack_copied / sizeof(DWORD)); ++i)
    {
        stack_pos += std::snprintf(stack_hex + stack_pos,
                                   sizeof(stack_hex) - stack_pos,
                                   "%s%08x",
                                   i == 0 ? "" : ",",
                                   static_cast<unsigned>(stack_words[i]));
    }

    char message[600] = {};
    std::snprintf(message,
                  sizeof(message),
                  "re2dj:vfs:crash-exception:code=0x%08x:address=0x%08x:image_base=0x%08x:"
                  "rva=0x%08x:in_image=%u:eax=0x%08x:ebx=0x%08x:ecx=0x%08x:edx=0x%08x:"
                  "esi=0x%08x:edi=0x%08x:ebp=0x%08x:esp=0x%08x:eip=0x%08x:eflags=0x%08x:"
                  "bytes=%s:stack=%s\r\n",
                  static_cast<unsigned>(code),
                  static_cast<unsigned>(address),
                  static_cast<unsigned>(image_base),
                  static_cast<unsigned>(rva),
                  in_image ? 1U : 0U,
                  static_cast<unsigned>(exception->ContextRecord->Eax),
                  static_cast<unsigned>(exception->ContextRecord->Ebx),
                  static_cast<unsigned>(exception->ContextRecord->Ecx),
                  static_cast<unsigned>(exception->ContextRecord->Edx),
                  static_cast<unsigned>(exception->ContextRecord->Esi),
                  static_cast<unsigned>(exception->ContextRecord->Edi),
                  static_cast<unsigned>(exception->ContextRecord->Ebp),
                  static_cast<unsigned>(exception->ContextRecord->Esp),
                  static_cast<unsigned>(exception->ContextRecord->Eip),
                  static_cast<unsigned>(exception->ContextRecord->EFlags),
                  code_hex,
                  stack_hex);
    AppendVfsTraceMessage(message);
    OutputDebugStringA(message);

    // Read 128-byte code window around faulting address (64 bytes before, 64 bytes after)
    const std::uintptr_t code_win_start = address >= 64 ? address - 64 : 0;
    unsigned char code_win_bytes[128] = {};
    SIZE_T code_win_copied = 0;
    ReadProcessMemory(GetCurrentProcess(),
                      reinterpret_cast<const void*>(code_win_start),
                      code_win_bytes,
                      sizeof(code_win_bytes),
                      &code_win_copied);
    char code_win_hex[sizeof(code_win_bytes) * 2 + 1] = {};
    for (SIZE_T i = 0; i < code_win_copied; ++i)
    {
        std::snprintf(code_win_hex + i * 2, 3, "%02x", code_win_bytes[i]);
    }

    // Read 32 bytes around EBP (caller return address is at [ebp+4])
    const std::uintptr_t ebp_val = exception->ContextRecord->Ebp;
    DWORD ebp_words[8] = {};
    SIZE_T ebp_copied = 0;
    ReadProcessMemory(GetCurrentProcess(),
                      reinterpret_cast<const void*>(ebp_val),
                      ebp_words,
                      sizeof(ebp_words),
                      &ebp_copied);
    char ebp_hex[sizeof(ebp_words) * 9 + 1] = {};
    int ebp_pos = 0;
    for (SIZE_T i = 0; i < (ebp_copied / sizeof(DWORD)); ++i)
    {
        ebp_pos += std::snprintf(ebp_hex + ebp_pos,
                                 sizeof(ebp_hex) - ebp_pos,
                                 "%s%08x",
                                 i == 0 ? "" : ",",
                                 static_cast<unsigned>(ebp_words[i]));
    }

    // Read 32 bytes around ECX + 0x10494 (the divisor location)
    const std::uintptr_t divisor_base = exception->ContextRecord->Ecx + 0x10484;
    DWORD ecx_words[8] = {};
    SIZE_T ecx_copied = 0;
    ReadProcessMemory(GetCurrentProcess(),
                      reinterpret_cast<const void*>(divisor_base),
                      ecx_words,
                      sizeof(ecx_words),
                      &ecx_copied);
    char ecx_hex[sizeof(ecx_words) * 9 + 1] = {};
    int ecx_pos = 0;
    for (SIZE_T i = 0; i < (ecx_copied / sizeof(DWORD)); ++i)
    {
        ecx_pos += std::snprintf(ecx_hex + ecx_pos,
                                 sizeof(ecx_hex) - ecx_pos,
                                 "%s%08x",
                                 i == 0 ? "" : ",",
                                 static_cast<unsigned>(ecx_words[i]));
    }

    char detail_msg[768] = {};
    std::snprintf(detail_msg,
                  sizeof(detail_msg),
                  "re2dj:vfs:crash-context:code_win_start=0x%08x:code_win=%s:ebp_words=%s:ecx_words=%s\r\n",
                  static_cast<unsigned>(code_win_start),
                  code_win_hex,
                  ebp_hex,
                  ecx_hex);
    AppendVfsTraceMessage(detail_msg);
    OutputDebugStringA(detail_msg);

    // A divide-by-zero through a struct field says which field, but not where
    // that field is written. The packed image is decrypted by the time it
    // faults and this runs inside the guest, so the code that touches the same
    // field can be found by scanning for its displacement. Only the F7 /7 form
    // with a 32-bit displacement is decoded, which is the shape that faulted.
    if (code == static_cast<DWORD>(EXCEPTION_INT_DIVIDE_BY_ZERO) &&
        code_readable != FALSE && code_copied >= 6 && code_bytes[0] == 0xf7 &&
        (code_bytes[1] & 0xc0) == 0x80)
    {
        const std::uint32_t displacement =
            static_cast<std::uint32_t>(code_bytes[2]) |
            (static_cast<std::uint32_t>(code_bytes[3]) << 8) |
            (static_cast<std::uint32_t>(code_bytes[4]) << 16) |
            (static_cast<std::uint32_t>(code_bytes[5]) << 24);
        const std::uintptr_t scan_start = image_base + 0x1000;
        constexpr std::size_t kScanBytes = 0x53000;
        std::vector<std::uint8_t> text(kScanBytes, 0);
        SIZE_T text_copied = 0;
        ReadProcessMemory(GetCurrentProcess(),
                          reinterpret_cast<const void*>(scan_start),
                          text.data(),
                          text.size(),
                          &text_copied);
        // The faulting ModRM names a base register whose value is the object.
        // A static object's address is itself a constant in the code, so
        // scanning for it finds the sites that build the object - which is
        // where the field the divisor came from is written.
        const DWORD* const integer_registers[8] = {
            &exception->ContextRecord->Eax, &exception->ContextRecord->Ecx,
            &exception->ContextRecord->Edx, &exception->ContextRecord->Ebx,
            &exception->ContextRecord->Esp, &exception->ContextRecord->Ebp,
            &exception->ContextRecord->Esi, &exception->ContextRecord->Edi};
        const std::uint32_t base_value =
            static_cast<std::uint32_t>(*integer_registers[code_bytes[1] & 0x07]);
        std::string matches;
        unsigned found = 0;
        for (SIZE_T index = 0; index + 4 <= text_copied && found < 24; ++index)
        {
            const std::uint32_t value = static_cast<std::uint32_t>(text[index]) |
                                        (static_cast<std::uint32_t>(text[index + 1]) << 8) |
                                        (static_cast<std::uint32_t>(text[index + 2]) << 16) |
                                        (static_cast<std::uint32_t>(text[index + 3]) << 24);
            if (value != displacement && value != base_value)
            {
                continue;
            }
            char entry[32] = {};
            // The displacement follows the opcode and ModRM, so the instruction
            // starts a couple of bytes earlier; the RVA is what matters here.
            std::snprintf(entry,
                          sizeof(entry),
                          "%c%08x,",
                          value == displacement ? 'd' : 'b',
                          static_cast<unsigned>(scan_start + index - image_base));
            matches.append(entry);
            ++found;
        }
        char scan_msg[768] = {};
        std::snprintf(scan_msg,
                      sizeof(scan_msg),
                      "re2dj:vfs:crash-field-scan:displacement=0x%08x:base=0x%08x:"
                      "scanned=%u:matches=%u:rvas=%.560s\r\n",
                      static_cast<unsigned>(displacement),
                      static_cast<unsigned>(base_value),
                      static_cast<unsigned>(text_copied),
                      found,
                      matches.c_str());
        AppendVfsTraceMessage(scan_msg);
    }
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

// One code window around the site that opens the first bitmap. The guest opens
// every sprite bitmap and then abandons it without reading, so the branch that
// skips the load sits immediately after this call. A .protect build decrypts
// .text at run time, which is why the window has to be read from inside the
// process rather than from the original file.
volatile LONG g_asset_caller_window_written = 0;

void ReportAssetOpenCallerWindow(const char* requested, std::uintptr_t caller)
{
    if (g_re2dj_vfs_trace_path[0] == '\0' || requested == nullptr || caller == 0)
    {
        return;
    }
    const std::size_t length = std::strlen(requested);
    if (length < 4 || _stricmp(requested + length - 4, ".bmp") != 0)
    {
        return;
    }
    if (InterlockedCompareExchange(&g_asset_caller_window_written, 1, 0) != 0)
    {
        return;
    }
    constexpr std::size_t kBefore = 32;
    constexpr std::size_t kAfter = 288;
    const std::uintptr_t start = caller >= kBefore ? caller - kBefore : 0;
    unsigned char bytes[kBefore + kAfter] = {};
    SIZE_T copied = 0;
    ReadProcessMemory(GetCurrentProcess(),
                      reinterpret_cast<const void*>(start),
                      bytes,
                      sizeof(bytes),
                      &copied);
    char hex[sizeof(bytes) * 2 + 1] = {};
    for (SIZE_T index = 0; index < copied; ++index)
    {
        std::snprintf(hex + index * 2, 3, "%02x", bytes[index]);
    }
    // The call site turned out to be a FileExists helper, so the decision that
    // skips the load belongs to its caller. Recording the stack above our own
    // return address exposes that frame's return address without guessing at a
    // frame layout.
    // Most of the stack above this point is our own trace buffers, so the raw
    // words are useless. Only values that land in the guest image's code range
    // are kept: those are the return addresses of the frames that led here.
    const auto* const return_slot =
        static_cast<const std::uintptr_t*>(_AddressOfReturnAddress());
    std::uintptr_t stack_words[1024] = {};
    SIZE_T stack_copied = 0;
    ReadProcessMemory(GetCurrentProcess(),
                      return_slot,
                      stack_words,
                      sizeof(stack_words),
                      &stack_copied);
    const std::uintptr_t image_low =
        reinterpret_cast<std::uintptr_t>(GetModuleHandleA(nullptr));
    char stack_hex[32 * 9 + 1] = {};
    const std::size_t stack_count = stack_copied / sizeof(stack_words[0]);
    std::size_t kept = 0;
    std::uintptr_t previous = 0;
    for (std::size_t index = 0; index < stack_count && kept < 32; ++index)
    {
        const std::uintptr_t value = stack_words[index];
        if (value <= image_low || value >= image_low + 0x00100000 || value == previous)
        {
            continue;
        }
        previous = value;
        std::snprintf(stack_hex + kept * 9, 10, "%08x,", static_cast<unsigned>(value));
        ++kept;
    }
    char message[1536] = {};
    std::snprintf(message,
                  sizeof(message),
                  "re2dj:vfs:asset-open-caller-window:request=%.63s:caller=0x%08x:"
                  "window_start=0x%08x:before=%u:bytes=%s:stack=%s\r\n",
                  requested,
                  static_cast<unsigned>(caller),
                  static_cast<unsigned>(start),
                  static_cast<unsigned>(kBefore),
                  hex,
                  stack_hex);
    AppendVfsTraceMessage(message);

    // The immediate caller is only the existence check. Each frame above it is
    // dumped as well, because the decision that skips the load lives in one of
    // them and their code is likewise only readable while running.
    std::size_t frame_index = 0;
    previous = 0;
    for (std::size_t index = 0; index < stack_count && frame_index < 6; ++index)
    {
        const std::uintptr_t value = stack_words[index];
        if (value <= image_low || value >= image_low + 0x00100000 ||
            value == previous || value == caller)
        {
            continue;
        }
        previous = value;
        const std::uintptr_t frame_start = value >= kBefore ? value - kBefore : 0;
        unsigned char frame_bytes[kBefore + kAfter] = {};
        SIZE_T frame_copied = 0;
        ReadProcessMemory(GetCurrentProcess(),
                          reinterpret_cast<const void*>(frame_start),
                          frame_bytes,
                          sizeof(frame_bytes),
                          &frame_copied);
        if (frame_copied == 0)
        {
            continue;
        }
        char frame_hex[sizeof(frame_bytes) * 2 + 1] = {};
        for (SIZE_T byte_index = 0; byte_index < frame_copied; ++byte_index)
        {
            std::snprintf(frame_hex + byte_index * 2, 3, "%02x", frame_bytes[byte_index]);
        }
        char frame_message[1024] = {};
        std::snprintf(frame_message,
                      sizeof(frame_message),
                      "re2dj:vfs:asset-open-frame-window:index=%u:address=0x%08x:"
                      "window_start=0x%08x:before=%u:bytes=%s\r\n",
                      static_cast<unsigned>(frame_index),
                      static_cast<unsigned>(value),
                      static_cast<unsigned>(frame_start),
                      static_cast<unsigned>(kBefore),
                      frame_hex);
        AppendVfsTraceMessage(frame_message);
        ++frame_index;
    }
}

void ReportVfsAssetOpen(const char* api,
                        const char* requested,
                        const char* mapped,
                        HANDLE result,
                        DWORD error,
                        std::uintptr_t caller)
{
    if (g_re2dj_vfs_trace_path[0] == '\0' ||
        !ClaimVfsTraceBudget(ClassifyVfsAsset(requested)))
    {
        return;
    }
    // The call site is reported as an RVA as well, because the absolute address
    // depends on where the image landed and cannot be compared across runs.
    const std::uintptr_t image_base =
        reinterpret_cast<std::uintptr_t>(GetModuleHandleA(nullptr));
    const std::uintptr_t caller_rva =
        caller == 0 || caller < image_base ? 0 : caller - image_base;
    char message[900] = {};
    std::snprintf(message,
                  sizeof(message),
                  "re2dj:vfs:asset-open:api=%s:request=%s:mapped=%s:success=%u:error=%lu"
                  ":caller=0x%08x:caller_rva=0x%08x\r\n",
                  api,
                  requested,
                  mapped == nullptr ? "" : mapped,
                  result != INVALID_HANDLE_VALUE ? 1U : 0U,
                  static_cast<unsigned long>(error),
                  static_cast<unsigned>(caller),
                  static_cast<unsigned>(caller_rva));
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

std::uint64_t HashHardlockField(std::span<const std::uint8_t> bytes)
{
    std::uint64_t hash = 1469598103934665603ULL;
    for (const std::uint8_t value : bytes)
    {
        hash ^= value;
        hash *= 1099511628211ULL;
    }
    return hash;
}

void RecordHardlockTransformInputHashes(DWORD control_code,
                                        const void* input,
                                        DWORD input_size)
{
    if (g_re2dj_hardlock_transform_input_trace == 0 ||
        control_code != re2dj::hle::hardlock::kHardlockIoctlTransform ||
        input == nullptr ||
        input_size < re2dj::hle::hardlock::kHardlockApiDescriptorSize)
    {
        return;
    }
    const auto* const bytes = static_cast<const std::uint8_t*>(input);
    re2dj::hle::hardlock::HardlockApiDescriptorHeader header;
    const bool header_valid =
        re2dj::hle::hardlock::ParseHardlockApiDescriptorHeader(
            std::span<const std::uint8_t>(bytes, input_size), &header);
    if (!header_valid)
    {
        AppendVfsTraceMessage(
            "re2dj:vfs:hardlock-transform-inputs:header_valid=0\r\n");
        return;
    }
    const std::size_t payload_size =
        input_size - re2dj::hle::hardlock::kHardlockApiDescriptorSize;
    const std::size_t available_blocks =
        payload_size / re2dj::hle::hardlock::kHardlockTransformBlockSize;
    const std::size_t block_count =
        (std::min)(static_cast<std::size_t>(header.block_count), available_blocks);
    char hashes[768] = {};
    std::size_t cursor = 0;
    for (std::size_t index = 0; index < block_count; ++index)
    {
        const auto block = std::span<const std::uint8_t>(
            bytes + re2dj::hle::hardlock::kHardlockApiDescriptorSize +
                index * re2dj::hle::hardlock::kHardlockTransformBlockSize,
            re2dj::hle::hardlock::kHardlockTransformBlockSize);
        const int written = std::snprintf(
            hashes + cursor,
            sizeof(hashes) - cursor,
            "%s%016llx",
            index == 0 ? "" : ",",
            static_cast<unsigned long long>(HashHardlockField(block)));
        if (written < 0 || static_cast<std::size_t>(written) >= sizeof(hashes) - cursor)
        {
            break;
        }
        cursor += static_cast<std::size_t>(written);
    }
    char message[960] = {};
    std::snprintf(message,
                  sizeof(message),
                  "re2dj:vfs:hardlock-transform-inputs:header_valid=1:function=0x%04x:input_size=%u:block_count=%u:hashes=%s\r\n",
                  static_cast<unsigned>(header.function),
                  static_cast<unsigned>(input_size),
                  static_cast<unsigned>(block_count),
                  hashes);
    AppendVfsTraceMessage(message);

    if (g_re2dj_hardlock_transform_input_dump[0] != '\0')
    {
        char header_message[128] = {};
        std::snprintf(header_message,
                      sizeof(header_message),
                      "# function=0x%04x block_count=%u\r\n",
                      static_cast<unsigned>(header.function),
                      static_cast<unsigned>(block_count));
        AppendDiagnosticFile(g_re2dj_hardlock_transform_input_dump,
                             header_message);
        for (std::size_t index = 0; index < block_count; ++index)
        {
            const auto block = std::span<const std::uint8_t>(
                bytes + re2dj::hle::hardlock::kHardlockApiDescriptorSize +
                    index * re2dj::hle::hardlock::kHardlockTransformBlockSize,
                re2dj::hle::hardlock::kHardlockTransformBlockSize);
            char block_hex[17] = {};
            for (std::size_t byte_index = 0; byte_index < block.size(); ++byte_index)
            {
                std::snprintf(block_hex + byte_index * 2,
                              sizeof(block_hex) - byte_index * 2,
                              "%02x",
                              static_cast<unsigned>(block[byte_index]));
            }
            AppendDiagnosticFile(g_re2dj_hardlock_transform_input_dump, block_hex);
            AppendDiagnosticFile(g_re2dj_hardlock_transform_input_dump, "\r\n");
        }
    }
}

void WriteHardlockDescriptorReference(
    const re2dj::hle::hardlock::HardlockApiDescriptorHeader& header)
{
    if (g_re2dj_hardlock_descriptor_output[0] == '\0' ||
        InterlockedCompareExchange(&g_re2dj_hardlock_descriptor_written, 1, 0) != 0)
    {
        return;
    }
    char id_reference_hex[17] = {};
    char id_verify_hex[17] = {};
    for (std::size_t index = 0; index < header.id_reference.size(); ++index)
    {
        std::snprintf(id_reference_hex + index * 2,
                      3,
                      "%02x",
                      header.id_reference[index]);
        std::snprintf(id_verify_hex + index * 2,
                      3,
                      "%02x",
                      header.id_verify[index]);
    }
    char contents[512] = {};
    const int content_size = std::snprintf(
        contents,
        sizeof(contents),
        "; Generated by --hardlock-descriptor-dump.\r\n"
        "; Keep this file local; it is not an HLE response map.\r\n\r\n"
        "[%s]\r\nmodule_address=0x%04x\r\n"
        "id_ref=%s\r\nid_verify=%s\r\n",
        g_re2dj_hardlock_descriptor_profile[0] == '\0'
            ? "unknown"
            : g_re2dj_hardlock_descriptor_profile,
        static_cast<unsigned>(header.module_address),
        id_reference_hex,
        id_verify_hex);
    if (content_size <= 0 || static_cast<std::size_t>(content_size) >= sizeof(contents))
    {
        InterlockedExchange(&g_re2dj_hardlock_descriptor_written, 0);
        return;
    }
    HANDLE file = CreateFileA(g_re2dj_hardlock_descriptor_output,
                              GENERIC_READ | GENERIC_WRITE,
                              FILE_SHARE_READ,
                              nullptr,
                              OPEN_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        InterlockedExchange(&g_re2dj_hardlock_descriptor_written, 0);
        return;
    }
    LARGE_INTEGER existing_size = {};
    if (GetFileSizeEx(file, &existing_size) == FALSE || existing_size.QuadPart > 64 * 1024)
    {
        CloseHandle(file);
        InterlockedExchange(&g_re2dj_hardlock_descriptor_written, 0);
        return;
    }
    std::string existing(static_cast<std::size_t>(existing_size.QuadPart), '\0');
    DWORD bytes_read = 0;
    if (!existing.empty() &&
        (ReadFile(file,
                  existing.data(),
                  static_cast<DWORD>(existing.size()),
                  &bytes_read,
                  nullptr) == FALSE ||
         bytes_read != static_cast<DWORD>(existing.size())))
    {
        CloseHandle(file);
        InterlockedExchange(&g_re2dj_hardlock_descriptor_written, 0);
        return;
    }
    const std::string profile_name = g_re2dj_hardlock_descriptor_profile[0] == '\0'
                                         ? "unknown"
                                         : g_re2dj_hardlock_descriptor_profile;
    const std::string section_header = "[" + profile_name + "]";
    std::string merged = existing;
    std::size_t section_start = merged.find(section_header);
    while (section_start != std::string::npos && section_start != 0 &&
           merged[section_start - 1] != '\n')
    {
        section_start = merged.find(section_header, section_start + 1);
    }
    if (section_start != std::string::npos)
    {
        const std::size_t next_section_marker = merged.find("\n[", section_start + 1);
        const std::size_t section_end = next_section_marker == std::string::npos
                                            ? merged.size()
                                            : next_section_marker + 1;
        merged.replace(section_start,
                       section_end - section_start,
                       contents,
                       static_cast<std::size_t>(content_size));
    }
    else
    {
        if (!merged.empty() && merged.back() != '\n')
        {
            merged.push_back('\n');
        }
        merged.append(contents, static_cast<std::size_t>(content_size));
    }
    LARGE_INTEGER beginning = {};
    const BOOL seek_ok = SetFilePointerEx(file, beginning, nullptr, FILE_BEGIN);
    DWORD bytes_written = 0;
    const BOOL write_ok = seek_ok &&
                          WriteFile(file,
                                    merged.data(),
                                    static_cast<DWORD>(merged.size()),
                                    &bytes_written,
                                    nullptr);
    const BOOL truncate_ok = write_ok && SetEndOfFile(file);
    CloseHandle(file);
    if (truncate_ok == FALSE || bytes_written != static_cast<DWORD>(merged.size()))
    {
        InterlockedExchange(&g_re2dj_hardlock_descriptor_written, 0);
    }
}

void ReportDeviceIoControlCode(DWORD control_code,
                               const void* input,
                               DWORD input_size,
                               DWORD output_size)
{
    const auto* const input_bytes = static_cast<const std::uint8_t*>(input);
    const std::span<const std::uint8_t> input_span =
        input_bytes == nullptr ? std::span<const std::uint8_t>()
                               : std::span<const std::uint8_t>(input_bytes, input_size);
    const bool is_descriptor =
        control_code == re2dj::hle::hardlock::kHardlockIoctlDescriptor;
    re2dj::hle::hardlock::HardlockApiDescriptorHeader header;
    bool header_valid = false;
    std::uint64_t id_reference_hash = 0;
    std::uint64_t id_verify_hash = 0;
    bool id_reference_nonzero = false;
    bool id_verify_nonzero = false;
    if (is_descriptor)
    {
        header_valid =
            re2dj::hle::hardlock::ParseHardlockApiDescriptorHeader(input_span, &header) &&
            input_span.size() == re2dj::hle::hardlock::kHardlockApiDescriptorSize;
        id_reference_hash = header_valid ? HashHardlockField(header.id_reference) : 0;
        id_verify_hash = header_valid ? HashHardlockField(header.id_verify) : 0;
        id_reference_nonzero =
            header_valid && std::any_of(header.id_reference.begin(),
                                        header.id_reference.end(),
                                        [](std::uint8_t value) { return value != 0; });
        id_verify_nonzero =
            header_valid && std::any_of(header.id_verify.begin(),
                                        header.id_verify.end(),
                                        [](std::uint8_t value) { return value != 0; });
        if (header_valid)
        {
            WriteHardlockDescriptorReference(header);
        }
    }
    if (!ClaimVfsDeviceTraceBudget())
    {
        return;
    }
    char message[768] = {};
    if (is_descriptor)
    {
        std::snprintf(
            message,
            sizeof(message),
            "re2dj:vfs:device-ioctl-entry:code=0x%08lx:input_size=%lu:output_size=%lu:"
            "header_valid=%u:module_id=0x%04x:module_address=0x%04x:data_address=0x%08lx:"
            "block_count=%u:function=0x%04x:status=0x%04x:remote=0x%04x:port=0x%04x:"
            "speed=0x%04x:network_users=0x%04x:id_ref_nonzero=%u:id_ref_hash=0x%016llx:"
            "id_verify_nonzero=%u:id_verify_hash=0x%016llx\r\n",
            static_cast<unsigned long>(control_code),
            static_cast<unsigned long>(input_size),
            static_cast<unsigned long>(output_size),
            header_valid ? 1u : 0u,
            header_valid ? static_cast<unsigned>(header.module_id) : 0u,
            header_valid ? static_cast<unsigned>(header.module_address) : 0u,
            header_valid ? static_cast<unsigned long>(header.data_address) : 0UL,
            header_valid ? static_cast<unsigned>(header.block_count) : 0u,
            header_valid ? static_cast<unsigned>(header.function) : 0u,
            header_valid ? static_cast<unsigned>(header.status) : 0u,
            header_valid ? static_cast<unsigned>(header.remote) : 0u,
            header_valid ? static_cast<unsigned>(header.port) : 0u,
            header_valid ? static_cast<unsigned>(header.speed) : 0u,
            header_valid ? static_cast<unsigned>(header.network_users) : 0u,
            id_reference_nonzero ? 1u : 0u,
            static_cast<unsigned long long>(id_reference_hash),
            id_verify_nonzero ? 1u : 0u,
            static_cast<unsigned long long>(id_verify_hash));
    }
    else
    {
        std::snprintf(message,
                      sizeof(message),
                      "re2dj:vfs:device-ioctl-entry:code=0x%08lx:input_size=%lu:output_size=%lu\r\n",
                      static_cast<unsigned long>(control_code),
                      static_cast<unsigned long>(input_size),
                      static_cast<unsigned long>(output_size));
    }
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
    const DWORD payload_count = g_re2dj_hardlock_payload_response_count;
    if (payload_count != 0 &&
        payload_count <= re2dj::hle::hardlock::kHardlockPayloadRecordCapacity)
    {
        options.payload_responses.reserve(payload_count);
        for (DWORD index = 0; index < payload_count; ++index)
        {
            const auto record = std::span<const std::uint8_t>(
                g_re2dj_hardlock_payload_responses +
                    index * re2dj::hle::hardlock::kHardlockPayloadRecordSize,
                re2dj::hle::hardlock::kHardlockPayloadRecordSize);
            re2dj::hle::hardlock::HardlockPayloadResponseEntry parsed;
            if (re2dj::hle::hardlock::UnpackHardlockPayloadResponse(record, &parsed))
            {
                options.payload_responses.push_back(std::move(parsed));
            }
        }
    }
    if (g_re2dj_hardlock_reject_function_enabled != 0)
    {
        options.reject_function =
            static_cast<std::uint16_t>(g_re2dj_hardlock_reject_function & 0xffff);
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
                  "mapped=%u:unmapped=%u:payload=%u:tick_ms=%llu\r\n",
                  re2dj::hle::hardlock::HardlockRequestKindName(result.kind),
                  re2dj::hle::hardlock::HardlockOutcomeName(result.outcome),
                  static_cast<unsigned>(result.bytes_written),
                  result.handshake_answered ? 1u : 0u,
                  result.descriptor_status_cleared ? 1u : 0u,
                  result.descriptor_tail_written ? 1u : 0u,
                  static_cast<unsigned>(result.transform_blocks_mapped),
                  static_cast<unsigned>(result.transform_blocks_unmapped),
                  result.transform_payload_mapped ? 1u : 0u,
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
    if (exception != nullptr && exception->ExceptionRecord != nullptr)
    {
        const DWORD code = exception->ExceptionRecord->ExceptionCode;
        if (code == EXCEPTION_INT_DIVIDE_BY_ZERO ||
            code == EXCEPTION_ACCESS_VIOLATION ||
            code == EXCEPTION_ILLEGAL_INSTRUCTION ||
            code == EXCEPTION_DATATYPE_MISALIGNMENT)
        {
            ReportCrashException(exception);
            return EXCEPTION_CONTINUE_SEARCH;
        }
    }

    if (g_re2dj_hle_io_ports == 0 || g_re2dj_io_image_base == 0 ||
        exception == nullptr || exception->ExceptionRecord == nullptr ||
        exception->ContextRecord == nullptr ||
        exception->ExceptionRecord->ExceptionCode != EXCEPTION_PRIV_INSTRUCTION)
    {
        if (exception != nullptr && exception->ExceptionRecord != nullptr &&
            exception->ExceptionRecord->ExceptionCode == EXCEPTION_PRIV_INSTRUCTION)
        {
            ReportCrashException(exception);
        }
        return EXCEPTION_CONTINUE_SEARCH;
    }

    const DWORD address = static_cast<DWORD>(
        reinterpret_cast<std::uintptr_t>(exception->ExceptionRecord->ExceptionAddress));
    // A word-wide port instruction in 32-bit code carries a 0x66 operand-size
    // prefix, so the faulting address is that prefix and the opcode follows it.
    // Getting this wrong does not merely answer the wrong port: advancing EIP
    // by one would resume inside the instruction.
    const unsigned char first_byte = *reinterpret_cast<const unsigned char*>(address);
    const bool word_prefixed = first_byte == 0x66;
    const unsigned char opcode =
        word_prefixed ? *reinterpret_cast<const unsigned char*>(address + 1) : first_byte;
    const DWORD instruction_length = word_prefixed ? 2u : 1u;
    const bool profile_is_word = g_re2dj_io_word_width != 0;
    // The profile states the board's width, so a guest instruction of the other
    // width is not this boundary's business. Unprefixed 0xed and 0xef are
    // 32-bit accesses, which no supported product has been observed using.
    const unsigned char read_opcode = profile_is_word ? 0xed : 0xec;
    const unsigned char write_opcode = profile_is_word ? 0xef : 0xee;
    if (word_prefixed != profile_is_word)
    {
        ReportCrashException(exception);
        return EXCEPTION_CONTINUE_SEARCH;
    }

    const bool configured_read = g_re2dj_io_in_byte_rva != 0 &&
                                 address == g_re2dj_io_image_base +
                                                g_re2dj_io_in_byte_rva;
    const bool configured_write = g_re2dj_io_out_byte_rva != 0 &&
                                  address == g_re2dj_io_image_base +
                                                g_re2dj_io_out_byte_rva;
    // A direction whose helper RVA is still unknown is judged by opcode alone.
    // Bring-up reaches one direction before the other, and the width is pinned
    // by the profile, so the opcode is unambiguous.
    const bool read_by_opcode = g_re2dj_hle_io_ports != 0 &&
                                g_re2dj_io_in_byte_rva == 0 && opcode == read_opcode;
    const bool write_by_opcode = g_re2dj_hle_io_ports != 0 &&
                                 g_re2dj_io_out_byte_rva == 0 && opcode == write_opcode;
    const bool is_read = configured_read || (!configured_write && read_by_opcode);
    const bool is_write = configured_write || (!configured_read && write_by_opcode);
    if (!is_read && !is_write)
    {
        ReportCrashException(exception);
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (opcode != (is_read ? read_opcode : write_opcode))
    {
        ReportCrashException(exception);
        return EXCEPTION_CONTINUE_SEARCH;
    }

    const std::uint16_t port = static_cast<std::uint16_t>(exception->ContextRecord->Edx);
    std::uint8_t value = static_cast<std::uint8_t>(exception->ContextRecord->Eax);
    std::uint16_t word_value = static_cast<std::uint16_t>(exception->ContextRecord->Eax);
    if (is_read && !profile_is_word && g_re2dj_io_config_path[0] != '\0')
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
    bool handled = false;
    if (profile_is_word)
    {
        handled = is_read ? g_dancer_io_port_bus.ReadWord(port, &word_value)
                          : g_dancer_io_port_bus.WriteWord(port, word_value);
    }
    else
    {
        handled = is_read ? g_legacy_io_port_bus.ReadByte(port, &value)
                          : g_legacy_io_port_bus.WriteByte(port, value);
    }
    // Every other boundary in this runtime records what the guest asked for,
    // and a board that answers silently cannot be diagnosed: which ports a
    // product really touches is exactly what is unresolved for a new one.
    if (ClaimVfsIoPortTraceBudget())
    {
        char message[160] = {};
        std::snprintf(message,
                      sizeof(message),
                      "re2dj:vfs:io-port:dir=%s:width=%u:port=0x%04x:value=0x%04x:handled=%u\r\n",
                      is_read ? "read" : "write",
                      profile_is_word ? 16u : 8u,
                      static_cast<unsigned>(port),
                      profile_is_word ? static_cast<unsigned>(word_value)
                                      : static_cast<unsigned>(value),
                      handled ? 1u : 0u);
        AppendVfsTraceMessage(message);
    }
    if (!handled)
    {
        ReportCrashException(exception);
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (is_read)
    {
        // Only the operand's own width is replaced; the rest of EAX belongs to
        // the guest.
        exception->ContextRecord->Eax =
            profile_is_word
                ? ((exception->ContextRecord->Eax & 0xffff0000u) | word_value)
                : ((exception->ContextRecord->Eax & 0xffffff00u) | value);
    }
    exception->ContextRecord->Eip += instruction_length;
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

constexpr std::uintptr_t kChdFindHandleBase = 0xFCCE0000;
constexpr std::size_t kMaximumChdFindHandles = 64;

struct ChdFindSearch
{
    bool used = false;
    std::vector<re2dj::storage::Fat32Entry> matches;
    std::size_t next_index = 0;
};
ChdFindSearch g_chd_find_searches[kMaximumChdFindHandles];

bool IsChdFindHandle(HANDLE handle)
{
    const std::uintptr_t value = reinterpret_cast<std::uintptr_t>(handle);
    return value > kChdFindHandleBase &&
           value <= kChdFindHandleBase + kMaximumChdFindHandles;
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
    const std::size_t guest_root_length = std::strlen(g_re2dj_vfs_guest_root);
    if (guest_root_length != 0 &&
        HasPrefixIgnoreCase(name, g_re2dj_vfs_guest_root))
    {
        const char* candidate = name + guest_root_length;
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
    relative->assign(g_re2dj_vfs_chd_root);
    if (!relative->empty() && relative->back() != '/')
    {
        relative->push_back('/');
    }
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

bool MapVfsSearchPath(const char* name, char path[MAX_PATH])
{
    if (name == nullptr || path == nullptr)
    {
        return false;
    }

    const std::string name_string(name);
    const std::size_t last_slash = name_string.find_last_of("\\/");
    const std::string directory = last_slash == std::string::npos
                                      ? "."
                                      : name_string.substr(0, last_slash);
    const std::string pattern = name_string.substr(
        last_slash == std::string::npos ? 0 : last_slash + 1);
    if (pattern.empty())
    {
        return false;
    }

    char mapped_directory[MAX_PATH] = {};
    if (!MapVfsPath(directory.c_str(), false, mapped_directory, nullptr))
    {
        return false;
    }
    return JoinRoot(mapped_directory, pattern.c_str(), path);
}

HANDLE OpenChdReadFile(const char* name, DWORD disposition, std::uintptr_t caller)
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
    ReportVfsAssetOpen("CreateFileA", name, mapped.c_str(), handle, ERROR_SUCCESS, caller);
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
    ReportExitStackChain(_AddressOfReturnAddress());
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
                       error,
                       reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
    SetLastError(error);
    return result;
}

// The guest's configuration lives in the CHD, where the real Win32 profile
// APIs cannot reach it: they open the name against the host filesystem and
// silently hand back the caller's default. Reads are answered from the VFS
// copy instead, and recorded so a value the guest depends on is visible.
// Resolves a profile file name to something the real profile API can open. The
// guest's own copy lives in the CHD, so it is materialised into the staging
// tree first; without this the API reads the host filesystem, finds nothing,
// and silently returns the caller's default.
const char* ResolveProfilePath(LPCSTR filename, char mapped[MAX_PATH])
{
    if (filename == nullptr || !MapVfsPath(filename, false, mapped, nullptr))
    {
        return filename;
    }
    if (IsRegularFile(mapped) || MaterializeChdFile(filename, mapped))
    {
        return mapped;
    }
    return filename;
}

void ReportProfileRead(const char* api,
                       const char* section,
                       const char* key,
                       const char* filename,
                       const char* resolved,
                       unsigned result)
{
    if (g_re2dj_vfs_trace_path[0] == '\0' || ClaimVfsFileTraceBudget() == 0)
    {
        return;
    }
    char message[768] = {};
    std::snprintf(message,
                  sizeof(message),
                  "re2dj:vfs:profile-read:api=%.31s:section=%.63s:key=%.63s:"
                  "file=%.159s:resolved=%.159s:result=%u\r\n",
                  api,
                  section == nullptr ? "<all>" : section,
                  key == nullptr ? "<all>" : key,
                  filename == nullptr ? "" : filename,
                  resolved == nullptr ? "" : resolved,
                  result);
    AppendVfsTraceMessage(message);
}

extern "C" __declspec(dllexport) UINT WINAPI Re2djVfsGetPrivateProfileIntA(
    LPCSTR section, LPCSTR key, INT default_value, LPCSTR filename)
{
    char mapped[MAX_PATH] = {};
    const char* const resolved = ResolveProfilePath(filename, mapped);
    const UINT value =
        Re2djHleGetPrivateProfileIntA(section, key, default_value, resolved);
    ReportProfileRead("GetPrivateProfileIntA", section, key, filename, resolved, value);
    return value;
}

extern "C" __declspec(dllexport) DWORD WINAPI Re2djVfsGetPrivateProfileStringA(
    LPCSTR section,
    LPCSTR key,
    LPCSTR default_value,
    LPSTR returned,
    DWORD size,
    LPCSTR filename)
{
    char mapped[MAX_PATH] = {};
    const char* const resolved = ResolveProfilePath(filename, mapped);
    const DWORD copied =
        GetPrivateProfileStringA(section, key, default_value, returned, size, resolved);
    ReportProfileRead(
        "GetPrivateProfileStringA", section, key, filename, resolved, copied);
    return copied;
}

extern "C" __declspec(dllexport) DWORD WINAPI Re2djVfsGetPrivateProfileSectionNamesA(
    LPSTR returned, DWORD size, LPCSTR filename)
{
    char mapped[MAX_PATH] = {};
    const char* const resolved = ResolveProfilePath(filename, mapped);
    const DWORD copied = GetPrivateProfileSectionNamesA(returned, size, resolved);
    ReportProfileRead(
        "GetPrivateProfileSectionNamesA", nullptr, nullptr, filename, resolved, copied);
    return copied;
}

extern "C" __declspec(dllexport) DWORD WINAPI Re2djVfsGetPrivateProfileSectionA(
    LPCSTR section, LPSTR returned, DWORD size, LPCSTR filename)
{
    char mapped[MAX_PATH] = {};
    const char* const resolved = ResolveProfilePath(filename, mapped);
    const DWORD copied = GetPrivateProfileSectionA(section, returned, size, resolved);
    ReportProfileRead(
        "GetPrivateProfileSectionA", section, nullptr, filename, resolved, copied);
    return copied;
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
    // Taken before any other call so it names the guest's own call site.
    const std::uintptr_t caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    EnsureDiagnosticBoundariesInstalled();
    OutputDebugStringA(kCreateFileMessage);
    ReportVfsCreateFileRequest(name, access, disposition, flags);
    ReportAssetOpenCallerWindow(name, caller);
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
        ReportVfsAssetOpen("CreateFileA", name, "", INVALID_HANDLE_VALUE, ERROR_INVALID_NAME, caller);
        SetLastError(ERROR_INVALID_NAME);
        return INVALID_HANDLE_VALUE;
    }
    if (!write && !IsRegularFile(path))
    {
        const HANDLE chd_handle = OpenChdReadFile(name, disposition, caller);
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
    ReportVfsAssetOpen("CreateFileA", name, path, result, error, caller);
    SetLastError(error);
    return result;
}

extern "C" __declspec(dllexport) BOOL WINAPI Re2djVfsReadFile(
    HANDLE handle, LPVOID buffer, DWORD size, LPDWORD transferred, LPOVERLAPPED overlapped)
{
    OutputDebugStringA(kFileApiMessage);
    ChdFileHandle* chd_handle = LookupChdFileHandle(handle);
    const char* kind = IsDeviceMockHandle(handle)
                           ? "device"
                           : chd_handle != nullptr ? "chd" : "native";
    ReportVfsReadFileEnter(kind, handle, size, overlapped);
    if (IsDeviceMockHandle(handle))
    {
        if (transferred != nullptr)
        {
            *transferred = 0;
        }
        SetLastError(ERROR_SUCCESS);
        ReportVfsReadFileResult(kind, handle, TRUE, 0, ERROR_SUCCESS);
        SetLastError(ERROR_SUCCESS);
        return TRUE;
    }
    if (chd_handle != nullptr)
    {
        if (overlapped != nullptr || (buffer == nullptr && size != 0))
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            ReportVfsReadFileResult(kind, handle, FALSE, 0, ERROR_INVALID_PARAMETER);
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
            ReportVfsReadFileResult(kind, handle, FALSE, 0, ERROR_INVALID_DATA);
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
            ReportVfsReadFileResult(kind, handle, FALSE, 0, ERROR_READ_FAULT);
            SetLastError(ERROR_READ_FAULT);
            return FALSE;
        }
        chd_handle->position += count;
        if (transferred != nullptr)
        {
            *transferred = static_cast<DWORD>(count);
        }
        SetLastError(ERROR_SUCCESS);
        ReportVfsReadFileResult(kind, handle, TRUE, static_cast<DWORD>(count), ERROR_SUCCESS);
        SetLastError(ERROR_SUCCESS);
        return TRUE;
    }
    const BOOL result = ReadFile(handle, buffer, size, transferred, overlapped);
    const DWORD error = result != FALSE ? ERROR_SUCCESS : GetLastError();
    const DWORD bytes = result != FALSE && transferred != nullptr ? *transferred : 0;
    ReportVfsReadFileResult(kind, handle, result, bytes, error);
    SetLastError(error);
    return result;
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
        ReportVfsFileQuery("GetFileSize", "chd", handle, chd_handle->size);
        return chd_handle->size;
    }
    const DWORD native_size = GetFileSize(handle, high);
    ReportVfsFileQuery("GetFileSize", "native", handle, native_size);
    return native_size;
}

extern "C" __declspec(dllexport) BOOL WINAPI Re2djVfsFindClose(HANDLE handle);

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
    if (IsChdFindHandle(handle))
    {
        return Re2djVfsFindClose(handle);
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
        ReportVfsFileQuery("GetFileType", "chd", handle, FILE_TYPE_DISK);
        return FILE_TYPE_DISK;
    }
    const DWORD native_type = GetFileType(handle);
    ReportVfsFileQuery("GetFileType", "native", handle, native_type);
    return native_type;
}

bool WildcardMatch(const char* pattern, const char* text)
{
    if (pattern == nullptr || text == nullptr)
    {
        return false;
    }
    if (std::strcmp(pattern, "*.*") == 0 || std::strcmp(pattern, "*") == 0)
    {
        return true;
    }
    while (*pattern != '\0')
    {
        if (*pattern == '*')
        {
            ++pattern;
            if (*pattern == '\0')
            {
                return true;
            }
            while (*text != '\0')
            {
                if (WildcardMatch(pattern, text))
                {
                    return true;
                }
                ++text;
            }
            return false;
        }
        if (*text == '\0')
        {
            return false;
        }
        if (*pattern != '?' &&
            std::tolower(static_cast<unsigned char>(*pattern)) !=
            std::tolower(static_cast<unsigned char>(*text)))
        {
            return false;
        }
        ++pattern;
        ++text;
    }
    return *text == '\0';
}

void PopulateFindData(const re2dj::storage::Fat32Entry& entry, LPWIN32_FIND_DATAA data)
{
    if (data == nullptr)
    {
        return;
    }
    std::memset(data, 0, sizeof(*data));
    data->dwFileAttributes = entry.directory ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
    data->nFileSizeLow = entry.size;
    data->nFileSizeHigh = 0;
    // Win32 reports real file times here. The FAT32 entry stores them as DOS
    // date and time words, and a zero word means the entry carries no such
    // stamp, so it is left as the zero the memset already wrote rather than
    // being converted: DosDateTimeToFileTime rejects zero.
    const auto fill_time = [](std::uint16_t date, std::uint16_t time, FILETIME* out) {
        if (date == 0)
        {
            return;
        }
        DosDateTimeToFileTime(date, time, out);
    };
    fill_time(entry.creation_date, entry.creation_time, &data->ftCreationTime);
    fill_time(entry.last_access_date, 0, &data->ftLastAccessTime);
    fill_time(entry.write_date, entry.write_time, &data->ftLastWriteTime);
    strncpy_s(data->cFileName, sizeof(data->cFileName), entry.name.c_str(), _TRUNCATE);
}

extern "C" __declspec(dllexport) BOOL WINAPI Re2djVfsFindClose(HANDLE handle)
{
    if (IsChdFindHandle(handle))
    {
        const std::size_t index = static_cast<std::size_t>(
            reinterpret_cast<std::uintptr_t>(handle) - kChdFindHandleBase - 1);
        if (index < kMaximumChdFindHandles && g_chd_find_searches[index].used)
        {
            g_chd_find_searches[index].used = false;
            g_chd_find_searches[index].matches.clear();
            g_chd_find_searches[index].next_index = 0;
            SetLastError(ERROR_SUCCESS);
            return TRUE;
        }
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    return FindClose(handle);
}

extern "C" __declspec(dllexport) HANDLE WINAPI Re2djVfsFindFirstFileA(
    LPCSTR name,
    LPWIN32_FIND_DATAA data)
{
    if (name == nullptr || data == nullptr)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return INVALID_HANDLE_VALUE;
    }

    // Split the pattern off before resolving, the way MapVfsSearchPath does for
    // the native path. The guest path parser rejects '*' and '?' as filename
    // characters, so handing it the whole search name fails and silently drops
    // the guest current directory, sweeping the CHD root instead.
    const std::string name_str(name);
    const std::size_t last_slash = name_str.find_last_of("\\/");
    const std::string directory =
        last_slash == std::string::npos ? "." : name_str.substr(0, last_slash);
    std::string pattern = name_str.substr(
        last_slash == std::string::npos ? 0 : last_slash + 1);

    std::string dir_part;
    if (!ResolveGuestRelativePath(directory.c_str(), &dir_part))
    {
        dir_part = last_slash == std::string::npos ? std::string() : directory;
        for (char& value : dir_part)
        {
            if (value == '\\')
            {
                value = '/';
            }
        }
    }

    std::string chd_dir = dir_part.empty() ? "EZ2DJ" : ("EZ2DJ/" + dir_part);

    std::vector<re2dj::storage::Fat32Entry> matches;
    if (EnsureChdMounted())
    {
        std::vector<re2dj::storage::Fat32Entry> entries;
        std::string error;
        if (g_chd_volume->ReadDirectory(chd_dir, &entries, &error))
        {
            for (const auto& entry : entries)
            {
                if (WildcardMatch(pattern.c_str(), entry.name.c_str()))
                {
                    matches.push_back(entry);
                }
            }
        }
    }

    if (!matches.empty())
    {
        for (std::size_t index = 0; index < kMaximumChdFindHandles; ++index)
        {
            ChdFindSearch& search = g_chd_find_searches[index];
            if (search.used)
            {
                continue;
            }
            search.used = true;
            search.matches = std::move(matches);
            search.next_index = 1;
            PopulateFindData(search.matches[0], data);
            const HANDLE handle = reinterpret_cast<HANDLE>(kChdFindHandleBase + index + 1);
            char message[384] = {};
            std::snprintf(message,
                          sizeof(message),
                          "re2dj:vfs:find-first:name=%s:chd_dir=%s:pattern=%s:matches=%zu:handle=0x%08x\r\n",
                          name,
                          chd_dir.c_str(),
                          pattern.c_str(),
                          search.matches.size(),
                          static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(handle)));
            AppendVfsTraceMessage(message);
            SetLastError(ERROR_SUCCESS);
            return handle;
        }
    }

    if (!IsChdConfigured())
    {
        char mapped_path[MAX_PATH] = {};
        if (!MapVfsSearchPath(name, mapped_path))
        {
            char message[768] = {};
            std::snprintf(message,
                          sizeof(message),
                          "re2dj:vfs:find-first:native:name=%.255s:mapped=:success=0:error=%lu\r\n",
                          name,
                          static_cast<unsigned long>(ERROR_INVALID_NAME));
            AppendVfsTraceMessage(message);
            SetLastError(ERROR_INVALID_NAME);
            return INVALID_HANDLE_VALUE;
        }

        const HANDLE host_handle = FindFirstFileA(mapped_path, data);
        const DWORD host_error = host_handle == INVALID_HANDLE_VALUE ? GetLastError() : ERROR_SUCCESS;
        char message[768] = {};
        std::snprintf(message,
                      sizeof(message),
                      "re2dj:vfs:find-first:native:name=%.255s:mapped=%.383s:success=%u:error=%lu\r\n",
                      name,
                      mapped_path,
                      host_handle != INVALID_HANDLE_VALUE ? 1U : 0U,
                      static_cast<unsigned long>(host_error));
        AppendVfsTraceMessage(message);
        SetLastError(host_error);
        return host_handle;
    }

    const HANDLE host_handle = FindFirstFileA(name, data);
    const DWORD host_error = host_handle == INVALID_HANDLE_VALUE ? GetLastError() : ERROR_SUCCESS;
    char message[384] = {};
    std::snprintf(message,
                  sizeof(message),
                  "re2dj:vfs:find-first-fallback:name=%s:success=%u:error=%lu\r\n",
                  name,
                  host_handle != INVALID_HANDLE_VALUE ? 1U : 0U,
                  host_error);
    AppendVfsTraceMessage(message);
    SetLastError(host_error);
    return host_handle;
}

extern "C" __declspec(dllexport) BOOL WINAPI Re2djVfsFindNextFileA(
    HANDLE handle,
    LPWIN32_FIND_DATAA data)
{
    if (data == nullptr)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (IsChdFindHandle(handle))
    {
        const std::size_t index = static_cast<std::size_t>(
            reinterpret_cast<std::uintptr_t>(handle) - kChdFindHandleBase - 1);
        if (index < kMaximumChdFindHandles && g_chd_find_searches[index].used)
        {
            ChdFindSearch& search = g_chd_find_searches[index];
            if (search.next_index < search.matches.size())
            {
                PopulateFindData(search.matches[search.next_index++], data);
                SetLastError(ERROR_SUCCESS);
                return TRUE;
            }
            SetLastError(ERROR_NO_MORE_FILES);
            return FALSE;
        }
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    return FindNextFileA(handle, data);
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
        ReportDeviceIoControlCode(control_code, input, input_size, output_size);
        RecordHardlockTransformInputHashes(control_code, input, input_size);
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
            // The song list lives in Songs\music.ini inside the CHD and is read
            // through these APIs, so leaving them on the real entry points made
            // the guest see an empty list.
            if (_stricmp(name, "GetPrivateProfileIntA") == 0)
            {
                const FARPROC result =
                    reinterpret_cast<FARPROC>(&Re2djVfsGetPrivateProfileIntA);
                ReportDynamicResolverName(
                    name, "hle", reinterpret_cast<std::uintptr_t>(result), caller);
                return result;
            }
            if (_stricmp(name, "GetPrivateProfileStringA") == 0)
            {
                const FARPROC result =
                    reinterpret_cast<FARPROC>(&Re2djVfsGetPrivateProfileStringA);
                ReportDynamicResolverName(
                    name, "hle", reinterpret_cast<std::uintptr_t>(result), caller);
                return result;
            }
            if (_stricmp(name, "GetPrivateProfileSectionNamesA") == 0)
            {
                const FARPROC result =
                    reinterpret_cast<FARPROC>(&Re2djVfsGetPrivateProfileSectionNamesA);
                ReportDynamicResolverName(
                    name, "hle", reinterpret_cast<std::uintptr_t>(result), caller);
                return result;
            }
            if (_stricmp(name, "GetPrivateProfileSectionA") == 0)
            {
                const FARPROC result =
                    reinterpret_cast<FARPROC>(&Re2djVfsGetPrivateProfileSectionA);
                ReportDynamicResolverName(
                    name, "hle", reinterpret_cast<std::uintptr_t>(result), caller);
                return result;
            }
            // The sprite loader reaches its bitmaps through LoadImageA, and a
            // packed build resolves that import itself at unpack time, which
            // overwrites whatever the launcher patched into the static slot.
            // Answering here is the only way the guest's own call reaches the
            // VFS, which is what lets a CHD-backed bitmap load at all.
            if (_stricmp(name, "LoadImageA") == 0)
            {
                const FARPROC result =
                    reinterpret_cast<FARPROC>(&Re2djVfsLoadImageA);
                ReportDynamicResolverName(
                    name, "hle", reinterpret_cast<std::uintptr_t>(result), caller);
                return result;
            }
            if (_stricmp(name, "FindFirstFileA") == 0)
            {
                const FARPROC result =
                    reinterpret_cast<FARPROC>(&Re2djVfsFindFirstFileA);
                ReportDynamicResolverName(
                    name, "hle", reinterpret_cast<std::uintptr_t>(result), caller);
                return result;
            }
            if (_stricmp(name, "FindNextFileA") == 0)
            {
                const FARPROC result =
                    reinterpret_cast<FARPROC>(&Re2djVfsFindNextFileA);
                ReportDynamicResolverName(
                    name, "hle", reinterpret_cast<std::uintptr_t>(result), caller);
                return result;
            }
            if (_stricmp(name, "FindClose") == 0)
            {
                const FARPROC result =
                    reinterpret_cast<FARPROC>(&Re2djVfsFindClose);
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
