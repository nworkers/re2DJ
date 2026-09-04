#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstring>
#include <cstdint>
#include <exception>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "re2dj/config/hardlock_secret_config.h"
#include "re2dj/hle/hardlock/handshake_response.h"
#include "re2dj/hle/hardlock/api_descriptor.h"
#include "re2dj/hle/hardlock/transform_responses.h"
#include "re2dj/device/lptdi_challenge_response.h"
#include "re2dj/device/lptdi_response_profile.h"
#include "re2dj/exe/pe_image.h"
#include "re2dj/hdd/hdd_root.h"
#include "re2dj/hdd/hdd_scan.h"
#include "re2dj/input/legacy_io_port_bus.h"
#include "re2dj/platform/windows/original_process_backend.h"
#include "re2dj/target/target_profile.h"

#include "../../platform/windows/injected_runtime_loader.h"
#include "../../platform/windows/runtime_export_locator.h"
#include "../windows_original_process_probe/iat_verifier.h"
#include "re2dj/exe/code_scan.h"
#include "re2dj/exe/immediate_scan.h"
#include "null_context_object_state.h"
#include "remote_module_exports.h"

namespace
{

class DiagnosticLog
{
public:
    bool Open(const std::string& target_id, std::string* error)
    {
        SYSTEMTIME now = {};
        GetLocalTime(&now);
        char timestamp[32] = {};
        std::snprintf(timestamp, sizeof(timestamp), "%04u%02u%02u-%02u%02u%02u-%03u",
                      now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond,
                      now.wMilliseconds);
        path_ = std::filesystem::current_path() / "logs" / "windows_x86_launcher_probe" /
                target_id / (std::string(timestamp) + ".jsonl");
        std::error_code filesystem_error;
        std::filesystem::create_directories(path_.parent_path(), filesystem_error);
        if (filesystem_error)
        {
            *error = "cannot create diagnostic log directory";
            return false;
        }
        stream_.open(path_, std::ios::out | std::ios::trunc);
        if (!stream_)
        {
            *error = "cannot open diagnostic log";
            return false;
        }
        return true;
    }

    void Write(const char* line)
    {
        if (stream_)
        {
            stream_ << line << '\n';
            stream_.flush();
        }
    }

    const std::filesystem::path& path() const
    {
        return path_;
    }

private:
    std::filesystem::path path_;
    std::ofstream stream_;
};

DiagnosticLog* g_diagnostic_log = nullptr;
bool g_trace = false;

void RecordDiagnostic(const char* format, ...)
{
    char line[2048] = {};
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(line, sizeof(line), format, arguments);
    va_end(arguments);
    if (g_diagnostic_log != nullptr)
    {
        g_diagnostic_log->Write(line);
    }
    if (g_trace)
    {
        std::fprintf(stderr, "%s\n", line);
    }
}

void PrintDiagnosticError(const std::string& error)
{
    RecordDiagnostic("{\"event\":\"outcome\",\"status\":\"error\",\"message\":\"%s\"}",
                     error.c_str());
    std::fprintf(stderr, "{\"error\":\"%s\"", error.c_str());
    if (g_diagnostic_log != nullptr)
    {
        std::fprintf(stderr, ",\"diagnostic_log\":\"%s\"",
                     g_diagnostic_log->path().generic_string().c_str());
    }
    std::fprintf(stderr, "}\n");
}

void PrintUsage()
{
    std::printf("Usage: re2dj_windows_x86_launcher_probe --hdd <directory> [--chd <image>] [--target <id>] [--target-executable <relative-path>] [--software-breakpoint] [--instruction-trace <max-steps>] [--inject-runtime [path]] [--probe-handoff|--hle-command-line|--hle-windows-directory|--hle-vfs [--hle-dynamic-vfs]|--hle-display-mode|--hle-d3d3 [--fullscreen]|--hle-directsound [--audio-gain-db <-24..18>] [--demo-volume <0..3>] [--audio-volume-trace]|--hle-io-ports [--io-config <path>]|--hle-message-box|--run-detached|--d3d-init-trace|--ksnd-load-trace|--device-mock-lptdi [--device-mock-lptdi-path-prefix <path>] [--device-mock-wts-console-session] [--device-mock-hardlock-450-response <12-hex-digits>] [--device-mock-hardlock-44c-tail <4-hex-digits>] [--hardlock-device] [--hardlock-transform-map <path>]|--device-mock-lptdi-ioctl-success|--device-mock-lptdi-ioctl-full-success|--device-mock-lptdi-response-profile <path>|--device-mock-lptdi-target-state <16-hex-digits>|--lptdi-post-ioctl-trace <max-steps> [--lptdi-post-ioctl-code <code>]|--probe-exit-process|--break-exit-process|--scan-fault-references|--field-reference-scan <hex-constant>|--field-write-watch <hex-address>|--code-window <hex-address>[:<hex-length>]|--slot-writer-trace|--null-context-object-source-trace|--null-context-field-writer-early-trace|--null-context-field-writer-trace|--null-context-field-access-trace|--null-context-field-reference-execution-trace|--null-context-object-state-trace|--null-context-object-reference-scan|--null-context-entry-trace|--null-context-allocation-trace|--api-trace] [--diagnostic-idle-timeout <milliseconds>] [--trace]\n");
}

bool WriteRemoteU32(HANDLE process, std::uintptr_t address, std::uint32_t value, std::string* error)
{
    SIZE_T written = 0;
    if (WriteProcessMemory(process,
                           reinterpret_cast<void*>(address),
                           &value,
                           sizeof(value),
                           &written) == FALSE ||
        written != sizeof(value))
    {
        *error = "cannot patch child memory";
        return false;
    }
    return true;
}

bool WriteRemoteBytes(HANDLE process,
                      std::uintptr_t address,
                      const std::uint8_t* bytes,
                      std::size_t size,
                      std::string* error)
{
    SIZE_T written = 0;
    if (bytes == nullptr || size == 0 ||
        WriteProcessMemory(process,
                           reinterpret_cast<void*>(address),
                           bytes,
                           size,
                           &written) == FALSE ||
        written != size)
    {
        *error = "cannot copy bytes into child memory";
        return false;
    }
    return true;
}

bool WriteRemoteAnsi(HANDLE process,
                     std::uintptr_t address,
                     const std::string& value,
                     std::string* error)
{
    if (value.size() >= MAX_PATH)
    {
        *error = "runtime path exceeds MAX_PATH configuration buffer";
        return false;
    }
    SIZE_T written = 0;
    if (WriteProcessMemory(process,
                           reinterpret_cast<void*>(address),
                           value.c_str(),
                           value.size() + 1,
                           &written) == FALSE ||
        written != value.size() + 1)
    {
        *error = "cannot configure injected runtime path";
        return false;
    }
    return true;
}

void TraceDebugEvent(const DEBUG_EVENT& event);

bool RecordAnsiOutputDebugString(HANDLE process,
                                 const DEBUG_EVENT& event,
                                 std::string* message)
{
    if (event.dwDebugEventCode != OUTPUT_DEBUG_STRING_EVENT ||
        event.u.DebugString.fUnicode != FALSE ||
        event.u.DebugString.nDebugStringLength == 0)
    {
        return false;
    }
    std::vector<char> buffer(event.u.DebugString.nDebugStringLength + 1, '\0');
    SIZE_T copied = 0;
    if (ReadProcessMemory(process,
                          event.u.DebugString.lpDebugStringData,
                          buffer.data(),
                          event.u.DebugString.nDebugStringLength,
                          &copied) == FALSE ||
        copied != event.u.DebugString.nDebugStringLength)
    {
        return false;
    }
    *message = buffer.data();
    RecordDiagnostic("{\"debug_event\":\"output_debug\",\"message\":\"%s\"}",
                     message->c_str());
    return true;
}

bool WaitForHandoff(HANDLE process, const char* expected_message, bool trace, std::string* error)
{
    (void)trace;
    for (std::uint32_t count = 0; count < 100; ++count)
    {
        DEBUG_EVENT event = {};
        if (WaitForDebugEvent(&event, 100) == FALSE)
        {
            if (GetLastError() == ERROR_SEM_TIMEOUT)
            {
                continue;
            }
            *error = "cannot wait for runtime handoff event";
            return false;
        }
        std::string debug_message;
        if (RecordAnsiOutputDebugString(process, event, &debug_message))
        {
            if (debug_message == expected_message)
            {
                return true;
            }
        }
        if (event.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT && event.u.LoadDll.hFile != nullptr)
        {
            CloseHandle(event.u.LoadDll.hFile);
        }
        if (event.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT)
        {
            TraceDebugEvent(event);
            char message[160] = {};
            std::snprintf(message,
                          sizeof(message),
                          "original process exited with code 0x%08x before expected runtime handoff",
                          static_cast<unsigned>(event.u.ExitProcess.dwExitCode));
            *error = message;
            return false;
        }
        if (ContinueDebugEvent(event.dwProcessId, event.dwThreadId, DBG_CONTINUE) == FALSE)
        {
            *error = "cannot continue child debug event during handoff";
            return false;
        }
    }
    *error = "runtime handoff was not observed before timeout";
    return false;
}

bool FindBundledRuntime(std::filesystem::path* runtime_path, std::string* error)
{
    std::vector<wchar_t> buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size())
    {
        *error = "cannot determine launcher probe path";
        return false;
    }
    const std::filesystem::path candidate = std::filesystem::path(buffer.data()).parent_path() /
                                            L"re2dj_windows_injected_runtime.dll";
    if (!std::filesystem::is_regular_file(candidate))
    {
        *error = "cannot find bundled injected runtime";
        return false;
    }
    *runtime_path = candidate;
    return true;
}

bool ReadFile(const std::filesystem::path& path,
              std::vector<std::uint8_t>* bytes,
              std::string* error)
{
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream)
    {
        *error = "cannot open original executable";
        return false;
    }
    const std::streamoff length = stream.tellg();
    if (length <= 0 || static_cast<std::uintmax_t>(length) >
                           (std::numeric_limits<std::size_t>::max)())
    {
        *error = "invalid original executable size";
        return false;
    }
    bytes->resize(static_cast<std::size_t>(length));
    stream.seekg(0);
    stream.read(reinterpret_cast<char*>(bytes->data()),
                static_cast<std::streamsize>(bytes->size()));
    if (!stream)
    {
        *error = "cannot read original executable";
        return false;
    }
    return true;
}

// Records the load base of one system module by its image file name. The
// comparison is on the final path leaf, case-insensitively.
void NoteSystemModuleBase(const DEBUG_EVENT& event,
                          const wchar_t* file_name,
                          std::uintptr_t* base)
{
    if (base == nullptr || *base != 0 || event.u.LoadDll.hFile == nullptr)
    {
        return;
    }
    std::vector<wchar_t> path(32768, L'\0');
    const DWORD length = GetFinalPathNameByHandleW(event.u.LoadDll.hFile,
                                                   path.data(),
                                                   static_cast<DWORD>(path.size()),
                                                   FILE_NAME_NORMALIZED);
    if (length == 0 || length >= path.size())
    {
        return;
    }
    std::wstring lowered = path.data();
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), ::towlower);
    const std::size_t leaf_start = lowered.find_last_of(L"\\/");
    const std::wstring leaf = leaf_start == std::wstring::npos
                                  ? lowered
                                  : lowered.substr(leaf_start + 1);
    if (leaf == file_name)
    {
        *base = reinterpret_cast<std::uintptr_t>(event.u.LoadDll.lpBaseOfDll);
    }
}

// Idle boundary of the bounded diagnostic debug-event loop. The default keeps
// the historical five-second window so existing diagnostic logs stay
// comparable; slower hosts can widen it from the command line.
constexpr std::uint32_t kDefaultDiagnosticIdleTimeoutMs = 5000;
constexpr std::uint32_t kMinimumDiagnosticIdleTimeoutMs = 1000;
constexpr std::uint32_t kMaximumDiagnosticIdleTimeoutMs = 600000;

constexpr std::uint32_t kEz2dj4thEarlyFieldRva = 0x006cd824;
constexpr DWORD kEarlyFieldBreakpointStatus = static_cast<DWORD>(1u << 3);
constexpr DWORD kEarlyFieldBreakpointEnable = static_cast<DWORD>(1u << 6);
constexpr DWORD kEarlyFieldWriteBreakpointControl =
    static_cast<DWORD>((1u << 28) | (3u << 30));
constexpr DWORD kEarlyFieldBreakpointMask =
    static_cast<DWORD>((0x3u << 6) | (0xfu << 28));

bool SetEarlyNullContextFieldWriterBreakpoint(HANDLE thread,
                                              std::uintptr_t image_base,
                                              std::string* error)
{
    CONTEXT context = {};
    context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (GetThreadContext(thread, &context) == FALSE)
    {
        *error = "cannot read early null-context field debug registers";
        return false;
    }
    context.Dr3 = static_cast<DWORD>(image_base + kEz2dj4thEarlyFieldRva);
    context.Dr6 = 0;
    context.Dr7 &= ~kEarlyFieldBreakpointMask;
    context.Dr7 |= kEarlyFieldBreakpointEnable |
                   kEarlyFieldWriteBreakpointControl;
    if (SetThreadContext(thread, &context) == FALSE)
    {
        *error = "cannot set early null-context field writer breakpoint";
        return false;
    }
    CONTEXT verified = {};
    verified.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (GetThreadContext(thread, &verified) == FALSE ||
        verified.Dr3 != context.Dr3 ||
        (verified.Dr7 & kEarlyFieldBreakpointMask) !=
            (kEarlyFieldBreakpointEnable | kEarlyFieldWriteBreakpointControl))
    {
        *error = "early null-context field writer breakpoint was not retained";
        return false;
    }
    return true;
}

bool HandleEarlyNullContextFieldWriterBreakpoint(
    HANDLE process,
    DWORD thread_id,
    std::uintptr_t image_base,
    std::uint32_t* hit_count,
    bool* handled,
    std::string* error)
{
    *handled = false;
    HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT,
                               FALSE,
                               thread_id);
    CONTEXT context = {};
    context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_DEBUG_REGISTERS;
    if (thread == nullptr || GetThreadContext(thread, &context) == FALSE)
    {
        if (thread != nullptr)
        {
            CloseHandle(thread);
        }
        *error = "cannot capture early null-context field writer context";
        return false;
    }
    if ((context.Dr6 & kEarlyFieldBreakpointStatus) == 0)
    {
        CloseHandle(thread);
        return true;
    }

    ++*hit_count;
    const std::uintptr_t field_address = image_base + kEz2dj4thEarlyFieldRva;
    std::uint32_t field_value = 0;
    SIZE_T copied = 0;
    const bool field_readable =
        ReadProcessMemory(process,
                          reinterpret_cast<const void*>(field_address),
                          &field_value,
                          sizeof(field_value),
                          &copied) != FALSE &&
        copied == sizeof(field_value);
    RecordDiagnostic(
        "{\"event\":\"null_context_field_writer_early_hit\",\"sequence\":%u,\"thread\":%u,\"field\":\"0x%08x\",\"eip_after\":\"0x%08x\",\"eax\":\"0x%08x\",\"ecx\":\"0x%08x\",\"ebp\":\"0x%08x\",\"esp\":\"0x%08x\",\"dr6\":\"0x%08x\",\"field_after\":\"0x%08x\",\"field_readable\":%s}",
        *hit_count,
        static_cast<unsigned>(thread_id),
        static_cast<unsigned>(field_address),
        static_cast<unsigned>(context.Eip),
        static_cast<unsigned>(context.Eax),
        static_cast<unsigned>(context.Ecx),
        static_cast<unsigned>(context.Ebp),
        static_cast<unsigned>(context.Esp),
        static_cast<unsigned>(context.Dr6),
        field_value,
        field_readable ? "true" : "false");

    context.Dr6 &= ~kEarlyFieldBreakpointStatus;
    if (SetThreadContext(thread, &context) == FALSE)
    {
        CloseHandle(thread);
        *error = "cannot clear early null-context field writer status";
        return false;
    }
    CloseHandle(thread);
    *handled = true;
    return true;
}

bool WaitForInitialBreakpoint(HANDLE process,
                              DWORD* process_id,
                              DWORD* thread_id,
                              std::uintptr_t* main_image_base,
                              std::uintptr_t* kernel32_base,
                              std::uintptr_t* kernelbase_base,
                              std::uintptr_t* user32_base,
                              bool early_field_writer_trace,
                              std::string* error)
{
    std::uint32_t early_field_writer_hit_count = 0;
    for (std::uint32_t event_count = 0; event_count < 128; ++event_count)
    {
        DEBUG_EVENT event = {};
        if (WaitForDebugEvent(&event, 5000) == FALSE)
        {
            *error = "cannot wait for original-process debug event";
            return false;
        }
        if (event.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT)
        {
            *main_image_base = reinterpret_cast<std::uintptr_t>(
                event.u.CreateProcessInfo.lpBaseOfImage);
            if (early_field_writer_trace &&
                !SetEarlyNullContextFieldWriterBreakpoint(
                    event.u.CreateProcessInfo.hThread,
                    *main_image_base,
                    error))
            {
                if (event.u.CreateProcessInfo.hFile != nullptr)
                {
                    CloseHandle(event.u.CreateProcessInfo.hFile);
                }
                return false;
            }
            if (event.u.CreateProcessInfo.hFile != nullptr)
            {
                CloseHandle(event.u.CreateProcessInfo.hFile);
            }
        }
        if (event.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT)
        {
            NoteSystemModuleBase(event, L"kernel32.dll", kernel32_base);
            NoteSystemModuleBase(event, L"kernelbase.dll", kernelbase_base);
            NoteSystemModuleBase(event, L"user32.dll", user32_base);
            if (event.u.LoadDll.hFile != nullptr)
            {
                CloseHandle(event.u.LoadDll.hFile);
            }
        }
        if (early_field_writer_trace &&
            event.dwDebugEventCode == CREATE_THREAD_DEBUG_EVENT)
        {
            if (!SetEarlyNullContextFieldWriterBreakpoint(
                    event.u.CreateThread.hThread,
                    *main_image_base,
                    error))
            {
                CloseHandle(event.u.CreateThread.hThread);
                return false;
            }
            RecordDiagnostic(
                "{\"event\":\"null_context_field_writer_early_thread_armed\",\"thread\":%u}",
                static_cast<unsigned>(event.dwThreadId));
            CloseHandle(event.u.CreateThread.hThread);
        }
        if (early_field_writer_trace &&
            event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT &&
            event.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_SINGLE_STEP)
        {
            bool handled = false;
            if (!HandleEarlyNullContextFieldWriterBreakpoint(
                    process,
                    event.dwThreadId,
                    *main_image_base,
                    &early_field_writer_hit_count,
                    &handled,
                    error))
            {
                return false;
            }
            if (handled)
            {
                if (ContinueDebugEvent(event.dwProcessId,
                                       event.dwThreadId,
                                       DBG_CONTINUE) == FALSE)
                {
                    *error = "cannot continue early null-context field writer hit";
                    return false;
                }
                continue;
            }
        }
        if (event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT &&
            event.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_BREAKPOINT)
        {
            *process_id = event.dwProcessId;
            *thread_id = event.dwThreadId;
            return true;
        }
        if (ContinueDebugEvent(event.dwProcessId, event.dwThreadId, DBG_CONTINUE) == FALSE)
        {
            *error = "cannot continue original-process debug event";
            return false;
        }
    }
    *error = "original process did not reach an initial breakpoint";
    return false;
}

bool SetEntryBreakpoint(HANDLE thread, std::uint32_t entry, std::string* error)
{
    CONTEXT context = {};
    context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (GetThreadContext(thread, &context) == FALSE)
    {
        *error = "cannot read x86 thread debug registers";
        return false;
    }
    context.Dr0 = entry;
    context.Dr7 = (context.Dr7 & ~static_cast<DWORD>(3)) | 1;
    if (SetThreadContext(thread, &context) == FALSE)
    {
        *error = "cannot set x86 entry breakpoint";
        return false;
    }
    CONTEXT verified = {};
    verified.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (GetThreadContext(thread, &verified) == FALSE || verified.Dr0 != entry ||
        (verified.Dr7 & static_cast<DWORD>(1)) == 0)
    {
        *error = "x86 entry breakpoint was not retained by the thread";
        return false;
    }
    return true;
}

bool SetSoftwareEntryBreakpoint(HANDLE process,
                                std::uint32_t entry,
                                std::uint8_t* original_byte,
                                std::string* error)
{
    SIZE_T copied = 0;
    if (ReadProcessMemory(process,
                          reinterpret_cast<const void*>(entry),
                          original_byte,
                          sizeof(*original_byte),
                          &copied) == FALSE ||
        copied != sizeof(*original_byte))
    {
        *error = "cannot read original entry byte";
        return false;
    }
    const std::uint8_t breakpoint = 0xcc;
    SIZE_T written = 0;
    if (WriteProcessMemory(process,
                           reinterpret_cast<void*>(entry),
                           &breakpoint,
                           sizeof(breakpoint),
                           &written) == FALSE ||
        written != sizeof(breakpoint) ||
        FlushInstructionCache(process, reinterpret_cast<const void*>(entry), sizeof(breakpoint)) ==
            FALSE)
    {
        *error = "cannot set software entry breakpoint";
        return false;
    }
    return true;
}

bool RestoreSoftwareEntryBreakpoint(HANDLE process,
                                    std::uint32_t entry,
                                    std::uint8_t original_byte,
                                    std::string* error)
{
    SIZE_T written = 0;
    if (WriteProcessMemory(process,
                           reinterpret_cast<void*>(entry),
                           &original_byte,
                           sizeof(original_byte),
                           &written) == FALSE ||
        written != sizeof(original_byte) ||
        FlushInstructionCache(process,
                              reinterpret_cast<const void*>(entry),
                              sizeof(original_byte)) == FALSE)
    {
        *error = "cannot restore original entry byte";
        return false;
    }
    return true;
}

bool EnableSingleStep(HANDLE thread, std::uint32_t entry, std::string* error)
{
    CONTEXT context = {};
    context.ContextFlags = CONTEXT_CONTROL;
    if (GetThreadContext(thread, &context) == FALSE)
    {
        *error = "cannot read x86 thread context for instruction trace";
        return false;
    }
    context.Eip = entry;
    context.EFlags |= 0x100;
    if (SetThreadContext(thread, &context) == FALSE)
    {
        *error = "cannot enable x86 single-step instruction trace";
        return false;
    }
    return true;
}

bool RearmSingleStep(DWORD thread_id, std::string* error)
{
    HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, thread_id);
    if (thread == nullptr)
    {
        *error = "cannot open x86 thread to rearm instruction trace";
        return false;
    }
    CONTEXT context = {};
    context.ContextFlags = CONTEXT_CONTROL;
    const bool updated = GetThreadContext(thread, &context) != FALSE &&
                         ((context.EFlags |= 0x100), SetThreadContext(thread, &context) != FALSE);
    CloseHandle(thread);
    if (!updated)
    {
        *error = "cannot rearm x86 single-step instruction trace";
        return false;
    }
    return true;
}

void TraceDebugEvent(const DEBUG_EVENT& event)
{
    switch (event.dwDebugEventCode)
    {
    case LOAD_DLL_DEBUG_EVENT:
    {
        std::vector<wchar_t> path(32768, L'\0');
        const DWORD length = event.u.LoadDll.hFile == nullptr
                                 ? 0
                                 : GetFinalPathNameByHandleW(event.u.LoadDll.hFile,
                                                             path.data(),
                                                             static_cast<DWORD>(path.size()),
                                                             FILE_NAME_NORMALIZED);
        std::wstring path_text = length > 0 && length < path.size()
                                     ? std::wstring(path.data())
                                     : std::wstring(L"<unavailable>");
        std::replace(path_text.begin(), path_text.end(), L'\\', L'/');
        RecordDiagnostic("{\"debug_event\":\"load_dll\",\"base\":\"0x%08x\",\"path\":\"%ls\"}",
                         static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(event.u.LoadDll.lpBaseOfDll)),
                         path_text.c_str());
        break;
    }
    case UNLOAD_DLL_DEBUG_EVENT:
        RecordDiagnostic("{\"debug_event\":\"unload_dll\",\"base\":\"0x%08x\"}",
                         static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(event.u.UnloadDll.lpBaseOfDll)));
        break;
    case CREATE_THREAD_DEBUG_EVENT:
        RecordDiagnostic("{\"debug_event\":\"create_thread\"}");
        break;
    case EXIT_THREAD_DEBUG_EVENT:
        RecordDiagnostic("{\"debug_event\":\"exit_thread\",\"code\":\"0x%08x\"}",
                         static_cast<unsigned>(event.u.ExitThread.dwExitCode));
        break;
    case EXIT_PROCESS_DEBUG_EVENT:
        // The tick pairs with the one on each Hardlock request line. Some exits
        // end the process below kernel32, where no in-process wrapper can
        // observe them, and comparing the two ticks is what still says whether
        // the protection acted just before the process died.
        RecordDiagnostic("{\"debug_event\":\"exit_process\",\"code\":\"0x%08x\",\"tick_ms\":%llu}",
                         static_cast<unsigned>(event.u.ExitProcess.dwExitCode),
                         static_cast<unsigned long long>(GetTickCount64()));
        break;
    case OUTPUT_DEBUG_STRING_EVENT:
        break;
    case EXCEPTION_DEBUG_EVENT:
        RecordDiagnostic("{\"debug_event\":\"exception\",\"code\":\"0x%08x\",\"address\":\"0x%08x\"}",
                         static_cast<unsigned>(event.u.Exception.ExceptionRecord.ExceptionCode),
                         static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(
                             event.u.Exception.ExceptionRecord.ExceptionAddress)));
        break;
    default:
        RecordDiagnostic("{\"debug_event\":%u}", event.dwDebugEventCode);
        break;
    }
}

bool WaitForEntryBreakpoint(std::uint32_t entry,
                            HANDLE process,
                            DWORD* process_id,
                            DWORD* thread_id,
                            bool software_breakpoint,
                            bool trace,
                            std::uintptr_t* kernel32_base,
                            std::uintptr_t* kernelbase_base,
                            std::uintptr_t* user32_base,
                            std::string* error)
{
    (void)trace;
    for (std::uint32_t event_count = 0; event_count < 128; ++event_count)
    {
        DEBUG_EVENT event = {};
        if (WaitForDebugEvent(&event, 5000) == FALSE)
        {
            *error = "cannot wait for entry breakpoint";
            return false;
        }
        TraceDebugEvent(event);
        if (software_breakpoint)
        {
            std::uint8_t entry_byte = 0;
            SIZE_T copied = 0;
            if (ReadProcessMemory(process,
                                  reinterpret_cast<const void*>(entry),
                                  &entry_byte,
                                  sizeof(entry_byte),
                                  &copied) != FALSE &&
                copied == sizeof(entry_byte))
            {
                RecordDiagnostic("{\"entry_byte\":\"0x%02x\"}", entry_byte);
            }
        }
        if (event.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT)
        {
            NoteSystemModuleBase(event, L"kernel32.dll", kernel32_base);
            NoteSystemModuleBase(event, L"kernelbase.dll", kernelbase_base);
            NoteSystemModuleBase(event, L"user32.dll", user32_base);
            if (event.u.LoadDll.hFile != nullptr)
            {
                CloseHandle(event.u.LoadDll.hFile);
            }
        }
        const DWORD expected_exception = software_breakpoint ? EXCEPTION_BREAKPOINT : EXCEPTION_SINGLE_STEP;
        if (event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT &&
            event.u.Exception.ExceptionRecord.ExceptionCode == expected_exception)
        {
            const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(
                event.u.Exception.ExceptionRecord.ExceptionAddress);
            if (address == entry)
            {
                *process_id = event.dwProcessId;
                *thread_id = event.dwThreadId;
                return true;
            }
            char message[160] = {};
            std::snprintf(message,
                          sizeof(message),
                          "entry breakpoint produced exception at 0x%08x instead of 0x%08x",
                          static_cast<unsigned>(address),
                          entry);
            *error = message;
            return false;
        }
        if (event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT)
        {
            char message[160] = {};
            std::snprintf(message,
                          sizeof(message),
                          "loader raised exception 0x%08x at 0x%08x before entry breakpoint",
                          event.u.Exception.ExceptionRecord.ExceptionCode,
                          static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(
                              event.u.Exception.ExceptionRecord.ExceptionAddress)));
            *error = message;
            return false;
        }
        if (event.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT)
        {
            char message[160] = {};
            std::snprintf(message,
                          sizeof(message),
                          "original process exited with code %u before entry breakpoint",
                          event.u.ExitProcess.dwExitCode);
            *error = message;
            return false;
        }
        if (ContinueDebugEvent(event.dwProcessId, event.dwThreadId, DBG_CONTINUE) == FALSE)
        {
            *error = "cannot continue loader event before entry breakpoint";
            return false;
        }
    }
    *error = "original process did not reach entry breakpoint";
    return false;
}

struct InstructionSample
{
    std::uint32_t address = 0;
    std::array<std::uint8_t, 16> bytes = {};
    SIZE_T byte_count = 0;
    // GP registers at single-step delivery: eax, ebx, ecx, edx, esi, edi,
    // ebp, esp. Filled only by the unload-tail collector.
    std::uint32_t regs[8] = {};
    bool has_regs = false;
    std::string symbol;
};

void RecordInstructionHistory(const std::vector<InstructionSample>& history,
                              std::uint32_t step_count)
{
    RecordDiagnostic("{\"event\":\"instruction_trace\",\"steps\":%u,\"history_count\":%u}",
                     step_count,
                     static_cast<unsigned>(history.size()));
    for (const InstructionSample& sample : history)
    {
        char bytes[sizeof(sample.bytes) * 2 + 1] = {};
        for (SIZE_T index = 0; index < sample.byte_count; ++index)
        {
            std::snprintf(bytes + index * 2, 3, "%02x", sample.bytes[index]);
        }
        if (sample.has_regs)
        {
            RecordDiagnostic("{\"event\":\"instruction_trace_sample\",\"address\":\"0x%08x\",\"bytes\":\"%s\",\"regs\":[\"0x%08x\",\"0x%08x\",\"0x%08x\",\"0x%08x\",\"0x%08x\",\"0x%08x\",\"0x%08x\",\"0x%08x\"],\"symbol\":\"%s\"}",
                             sample.address,
                             bytes,
                             sample.regs[0],
                             sample.regs[1],
                             sample.regs[2],
                             sample.regs[3],
                             sample.regs[4],
                             sample.regs[5],
                             sample.regs[6],
                             sample.regs[7],
                             sample.symbol.c_str());
        }
        else
        {
            RecordDiagnostic("{\"event\":\"instruction_trace_sample\",\"address\":\"0x%08x\",\"bytes\":\"%s\"}",
                             sample.address,
                             bytes);
        }
    }
}

bool WaitForInstructionTrace(HANDLE process,
                             DWORD traced_thread_id,
                             std::uint32_t max_steps,
                             std::string* error)
{
    constexpr std::size_t history_capacity = 32;
    std::vector<InstructionSample> history;
    history.reserve(history_capacity);
    for (std::uint32_t step_count = 0; step_count < max_steps; ++step_count)
    {
        DEBUG_EVENT event = {};
        if (WaitForDebugEvent(&event, 5000) == FALSE)
        {
            *error = "cannot wait for instruction-trace event";
            return false;
        }
        if (event.dwDebugEventCode != EXCEPTION_DEBUG_EVENT ||
            event.u.Exception.ExceptionRecord.ExceptionCode != EXCEPTION_SINGLE_STEP)
        {
            TraceDebugEvent(event);
        }
        if (event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT &&
            event.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_SINGLE_STEP)
        {
            InstructionSample sample;
            sample.address = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(
                event.u.Exception.ExceptionRecord.ExceptionAddress));
            ReadProcessMemory(process,
                              reinterpret_cast<const void*>(sample.address),
                              sample.bytes.data(),
                              sample.bytes.size(),
                              &sample.byte_count);
            if (history.size() == history_capacity)
            {
                history.erase(history.begin());
            }
            history.push_back(sample);
            if (!RearmSingleStep(event.dwThreadId, error) ||
                ContinueDebugEvent(event.dwProcessId, event.dwThreadId, DBG_CONTINUE) == FALSE)
            {
                if (error->empty())
                {
                    *error = "cannot continue x86 instruction trace";
                }
                return false;
            }
            continue;
        }
        if (event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT &&
            event.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION)
        {
            RecordInstructionHistory(history, step_count);
            char message[192] = {};
            std::snprintf(message,
                          sizeof(message),
                          "instruction trace reached illegal instruction at 0x%08x after %u steps",
                          static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(
                              event.u.Exception.ExceptionRecord.ExceptionAddress)),
                          step_count);
            *error = message;
            return false;
        }
        if (event.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT && event.u.LoadDll.hFile != nullptr)
        {
            CloseHandle(event.u.LoadDll.hFile);
        }
        const DWORD continue_status = event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT
                                          ? DBG_EXCEPTION_NOT_HANDLED
                                          : DBG_CONTINUE;
        if ((event.dwThreadId == traced_thread_id && !RearmSingleStep(traced_thread_id, error)) ||
            ContinueDebugEvent(event.dwProcessId, event.dwThreadId, continue_status) == FALSE)
        {
            *error = "cannot continue non-step instruction-trace event";
            return false;
        }
    }
    RecordInstructionHistory(history, max_steps);
    *error = "instruction trace step limit reached";
    return false;
}

void ScanFaultReferences(HANDLE process, std::uint32_t fault_address)
{
    constexpr SIZE_T block_size = 64 * 1024;
    constexpr std::uint32_t result_limit = 64;
    const std::uint32_t page_base = fault_address & ~static_cast<std::uint32_t>(0xfff);
    std::uint64_t scanned_bytes = 0;
    std::uint32_t scanned_regions = 0;
    std::uint32_t result_count = 0;
    std::uint32_t fault_match_count = 0;
    std::uint32_t page_match_count = 0;
    bool capped = false;
    std::uintptr_t address = 0;
    while (address < (std::numeric_limits<std::uint32_t>::max)())
    {
        MEMORY_BASIC_INFORMATION memory = {};
        if (VirtualQueryEx(process,
                           reinterpret_cast<const void*>(address),
                           &memory,
                           sizeof(memory)) != sizeof(memory))
        {
            break;
        }
        const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(memory.BaseAddress);
        const std::uintptr_t next = base + memory.RegionSize;
        if (memory.State == MEM_COMMIT &&
            (memory.Type == MEM_PRIVATE || memory.Type == MEM_IMAGE) &&
            (memory.Protect & (PAGE_NOACCESS | PAGE_GUARD)) == 0)
        {
            ++scanned_regions;
            for (SIZE_T offset = 0; offset < memory.RegionSize; offset += block_size)
            {
                const SIZE_T remaining = memory.RegionSize - offset;
                const SIZE_T requested = (std::min)(remaining, block_size + sizeof(std::uint32_t) - 1);
                std::vector<std::uint8_t> bytes(requested);
                SIZE_T copied = 0;
                if (ReadProcessMemory(process,
                                      reinterpret_cast<const void*>(base + offset),
                                      bytes.data(),
                                      bytes.size(),
                                      &copied) == FALSE ||
                    copied < sizeof(std::uint32_t))
                {
                    continue;
                }
                scanned_bytes += copied;
                for (SIZE_T index = 0; index + sizeof(std::uint32_t) <= copied; ++index)
                {
                    std::uint32_t value = 0;
                    std::memcpy(&value, bytes.data() + index, sizeof(value));
                    if (value != fault_address && value != page_base)
                    {
                        continue;
                    }
                    if (result_count == result_limit)
                    {
                        capped = true;
                        break;
                    }
                    ++result_count;
                    if (value == fault_address)
                    {
                        ++fault_match_count;
                    }
                    else
                    {
                        ++page_match_count;
                    }
                    RecordDiagnostic("{\"event\":\"fault_reference\",\"location\":\"0x%08x\",\"value\":\"0x%08x\",\"match\":\"%s\",\"region\":\"0x%08x\",\"type\":\"0x%08x\",\"protect\":\"0x%08x\"}",
                                     static_cast<unsigned>(base + offset + index),
                                     value,
                                     value == fault_address ? "fault_address" : "page_base",
                                     static_cast<unsigned>(base),
                                     static_cast<unsigned>(memory.Type),
                                     static_cast<unsigned>(memory.Protect));
                }
                if (capped)
                {
                    break;
                }
            }
        }
        if (capped || next <= address)
        {
            break;
        }
        address = next;
    }
    RecordDiagnostic("{\"event\":\"fault_reference_summary\",\"fault_address\":\"0x%08x\",\"page_base\":\"0x%08x\",\"scanned_regions\":%u,\"scanned_bytes\":%llu,\"fault_matches\":%u,\"page_matches\":%u,\"capped\":%s}",
                     fault_address,
                     page_base,
                     scanned_regions,
                     static_cast<unsigned long long>(scanned_bytes),
                     fault_match_count,
                     page_match_count,
                     capped ? "true" : "false");
}

// Finds committed private/image memory locations that store the entry VA.
// Neighboring dwords are logged with each match so planted runs such as
// {entry, entry, ...} stand out from isolated references. The walk mirrors
// ScanFaultReferences with a different match predicate.
void ScanEntryReferences(HANDLE process, std::uint32_t entry_address)
{
    constexpr SIZE_T block_size = 64 * 1024;
    constexpr std::uint32_t result_limit = 48;
    std::uint32_t result_count = 0;
    std::uint32_t run_count = 0;
    bool capped = false;
    std::uintptr_t address = 0;
    while (address < (std::numeric_limits<std::uint32_t>::max)())
    {
        MEMORY_BASIC_INFORMATION memory = {};
        if (VirtualQueryEx(process,
                           reinterpret_cast<const void*>(address),
                           &memory,
                           sizeof(memory)) != sizeof(memory))
        {
            break;
        }
        const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(memory.BaseAddress);
        const std::uintptr_t next = base + memory.RegionSize;
        if (memory.State == MEM_COMMIT &&
            (memory.Type == MEM_PRIVATE || memory.Type == MEM_IMAGE) &&
            (memory.Protect & (PAGE_NOACCESS | PAGE_GUARD)) == 0)
        {
            for (SIZE_T offset = 0; offset < memory.RegionSize; offset += block_size)
            {
                const SIZE_T remaining = memory.RegionSize - offset;
                const SIZE_T requested = (std::min)(remaining, block_size);
                std::vector<std::uint8_t> bytes(requested);
                SIZE_T copied = 0;
                if (ReadProcessMemory(process,
                                      reinterpret_cast<const void*>(base + offset),
                                      bytes.data(),
                                      bytes.size(),
                                      &copied) == FALSE ||
                    copied < sizeof(std::uint32_t))
                {
                    continue;
                }
                for (SIZE_T index = 0; index + sizeof(std::uint32_t) <= copied; ++index)
                {
                    std::uint32_t value = 0;
                    std::memcpy(&value, bytes.data() + index, sizeof(value));
                    if (value != entry_address)
                    {
                        continue;
                    }
                    if (result_count == result_limit)
                    {
                        capped = true;
                        break;
                    }
                    ++result_count;
                    const std::uintptr_t match_address = base + offset + index;
                    std::uint32_t previous = 0;
                    std::uint32_t following = 0;
                    SIZE_T neighbor_copied = 0;
                    const bool have_previous =
                        ReadProcessMemory(process,
                                          reinterpret_cast<const void*>(match_address - 4),
                                          &previous,
                                          sizeof(previous),
                                          &neighbor_copied) != FALSE &&
                        neighbor_copied == sizeof(previous);
                    const bool have_following =
                        ReadProcessMemory(process,
                                          reinterpret_cast<const void*>(match_address + 4),
                                          &following,
                                          sizeof(following),
                                          &neighbor_copied) != FALSE &&
                        neighbor_copied == sizeof(following);
                    const bool in_run =
                        (have_previous && previous == entry_address) ||
                        (have_following && following == entry_address);
                    if (in_run)
                    {
                        ++run_count;
                    }
                    char previous_text[16] = "null";
                    if (have_previous)
                    {
                        std::snprintf(previous_text, sizeof(previous_text), "\"0x%08x\"", previous);
                    }
                    char following_text[16] = "null";
                    if (have_following)
                    {
                        std::snprintf(following_text, sizeof(following_text), "\"0x%08x\"", following);
                    }
                    RecordDiagnostic("{\"event\":\"fault_entry_reference\",\"address\":\"0x%08x\",\"prev\":%s,\"next\":%s,\"in_run\":%s}",
                                     static_cast<unsigned>(match_address),
                                     previous_text,
                                     following_text,
                                     in_run ? "true" : "false");
                }
                if (capped)
                {
                    break;
                }
            }
        }
        if (capped)
        {
            break;
        }
        address = next;
    }
    RecordDiagnostic("{\"event\":\"fault_entry_summary\",\"entry\":\"0x%08x\",\"matches\":%u,\"runs\":%u,\"capped\":%s}",
                     entry_address,
                     result_count,
                     run_count,
                     capped ? "true" : "false");
}

struct ApiWatchPoint
{
    std::string name;
    int string_arg_index = -1;
    std::size_t argument_count = 4;
    std::uint8_t original_byte = 0;
};

using ApiWatchMap = std::map<std::uintptr_t, ApiWatchPoint>;

bool IsAllocationApi(const ApiWatchPoint& watch)
{
    return watch.name == "LocalAlloc" || watch.name == "HeapAlloc" ||
           watch.name == "VirtualAlloc";
}

struct PendingAllocationReturn
{
    std::uintptr_t api_address = 0;
    std::uintptr_t return_address = 0;
    std::uint8_t return_original_byte = 0;
    ApiWatchPoint watch;
    std::array<std::uint32_t, 4> args = {};
};

using PendingAllocationReturnMap = std::map<DWORD, std::vector<PendingAllocationReturn>>;

struct AllocationReturnBreakpoint
{
    std::uint8_t original_byte = 0;
    std::uint32_t pending_count = 0;
};

using AllocationReturnBreakpointMap =
    std::map<std::uintptr_t, AllocationReturnBreakpoint>;

std::size_t PendingAllocationReturnCount(const PendingAllocationReturnMap& pending)
{
    std::size_t count = 0;
    for (const auto& entry : pending)
    {
        count += entry.second.size();
    }
    return count;
}

struct AllocationTraceState
{
    static constexpr std::uint32_t kHitLimit = 256;
    std::uint32_t armed_count = 0;
    std::uint32_t hit_count = 0;
    std::uint32_t recorded_count = 0;
    bool capped = false;
};

struct GuestReturnWatchPoint
{
    enum class Kind
    {
        kD3dInit,
        kKsndLoad,
    };

    const char* name = nullptr;
    Kind kind = Kind::kD3dInit;
    bool nonzero_is_success = false;
    std::uint8_t original_byte = 0;
};

using GuestReturnWatchMap = std::map<std::uintptr_t, GuestReturnWatchPoint>;

bool InstallD3dInitReturnBreakpoints(HANDLE process,
                                     std::uintptr_t image_base,
                                     GuestReturnWatchMap* watches,
                                     std::string* error)
{
    struct Stage
    {
        std::uint32_t return_rva;
        const char* name;
    };
    constexpr Stage kStages[] = {{0x0001f5de, "direct_draw"},
                                 {0x0001f5ee, "direct_3d"},
                                 {0x0001f5fe, "surfaces"},
                                 {0x0001f60e, "device"},
                                 {0x0001f61e, "graphics_state"}};
    for (const Stage& stage : kStages)
    {
        GuestReturnWatchPoint watch;
        watch.name = stage.name;
        const std::uintptr_t address = image_base + stage.return_rva;
        if (!SetSoftwareEntryBreakpoint(process, address, &watch.original_byte, error))
        {
            return false;
        }
        watches->emplace(address, watch);
        RecordDiagnostic("{\"event\":\"d3d_init_watch\",\"stage\":\"%s\",\"address\":\"0x%08x\",\"status\":\"armed\"}",
                         stage.name,
                         static_cast<unsigned>(address));
    }
    return true;
}

bool InstallKsndLoadReturnBreakpoints(HANDLE process,
                                      std::uintptr_t image_base,
                                      GuestReturnWatchMap* watches,
                                      std::string* error)
{
    struct Stage
    {
        std::uint32_t return_rva;
        const char* name;
        bool nonzero_is_success;
    };
    constexpr Stage kStages[] = {{0x0002483d, "wave_parse", true},
                                 {0x000248fe, "create_sound_buffer", false},
                                 {0x00024963, "buffer_lock", false},
                                 {0x000249a0, "buffer_unlock", false}};
    for (const Stage& stage : kStages)
    {
        GuestReturnWatchPoint watch;
        watch.name = stage.name;
        watch.kind = GuestReturnWatchPoint::Kind::kKsndLoad;
        watch.nonzero_is_success = stage.nonzero_is_success;
        const std::uintptr_t address = image_base + stage.return_rva;
        if (!SetSoftwareEntryBreakpoint(process, address, &watch.original_byte, error))
        {
            return false;
        }
        watches->emplace(address, watch);
        RecordDiagnostic("{\"event\":\"ksnd_load_watch\",\"stage\":\"%s\",\"address\":\"0x%08x\",\"status\":\"armed\"}",
                         stage.name,
                         static_cast<unsigned>(address));
    }
    return true;
}

struct RemoteBufferSnapshot
{
    bool readable = false;
    std::string bytes;
};

struct PendingDeviceIoControl
{
    std::uint32_t return_address = 0;
    std::uint8_t original_byte = 0;
    std::array<std::uint32_t, 8> args = {};
    RemoteBufferSnapshot input_before;
    RemoteBufferSnapshot output_before;
    bool have_bytes_returned_before = false;
    std::uint32_t bytes_returned_before = 0;
};

struct PostDeviceIoControlTrace
{
    std::uint32_t code = 0;
    std::uint32_t output_address = 0;
    std::uint32_t output_size = 0;
    std::uintptr_t allocation_base = 0;
    std::uint32_t sequence = 0;
    std::uint32_t remaining = 0;
    std::uint32_t last_address = 0;
    std::array<std::uint8_t, 16> last_bytes = {};
    SIZE_T last_byte_count = 0;
    bool waiting_for_resume = false;
    std::uint32_t resume_address = 0;
    std::uint8_t resume_original_byte = 0;
};

bool IsSyntheticDeviceHandle(std::uint32_t handle)
{
    constexpr std::uint32_t kDeviceMockHandleBase = 0xfeed0000;
    return handle > kDeviceMockHandleBase && handle <= kDeviceMockHandleBase + 0xff;
}

void RecordPostDeviceIoControlSample(HANDLE process,
                                     DWORD thread_id,
                                     const PostDeviceIoControlTrace& trace,
                                     const CONTEXT& context,
                                     const std::uint8_t* restored_first_byte = nullptr)
{
    const std::uint32_t registers[] = {
        context.Eax, context.Ebx, context.Ecx, context.Edx,
        context.Esi, context.Edi, context.Ebp, context.Esp,
    };
    constexpr const char* kRegisterNames[] = {
        "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "esp",
    };
    std::string aliases;
    const std::uint64_t output_end =
        static_cast<std::uint64_t>(trace.output_address) + trace.output_size;
    for (std::size_t index = 0; index < std::size(registers); ++index)
    {
        if (trace.output_size == 0 || registers[index] < trace.output_address ||
            static_cast<std::uint64_t>(registers[index]) >= output_end)
        {
            continue;
        }
        if (!aliases.empty())
        {
            aliases += ',';
        }
        aliases += '"';
        aliases += kRegisterNames[index];
        aliases += '"';
    }

    std::uint8_t instruction[16] = {};
    SIZE_T copied = 0;
    ReadProcessMemory(process,
                      reinterpret_cast<const void*>(context.Eip),
                      instruction,
                      sizeof(instruction),
                      &copied);
    if (copied != 0 && restored_first_byte != nullptr)
    {
        instruction[0] = *restored_first_byte;
    }
    char bytes[sizeof(instruction) * 2 + 1] = {};
    for (SIZE_T index = 0; index < copied; ++index)
    {
        std::snprintf(bytes + index * 2, 3, "%02x", instruction[index]);
    }
    RecordDiagnostic("{\"event\":\"lptdi_post_ioctl_instruction\",\"thread\":%u,\"code\":\"0x%08x\",\"sequence\":%u,\"address\":\"0x%08x\",\"bytes\":\"%s\",\"regs\":[\"0x%08x\",\"0x%08x\",\"0x%08x\",\"0x%08x\",\"0x%08x\",\"0x%08x\",\"0x%08x\",\"0x%08x\"],\"output\":\"0x%08x\",\"output_size\":%u,\"output_aliases\":[%s]}",
                     static_cast<unsigned>(thread_id),
                     trace.code,
                     trace.sequence,
                     static_cast<unsigned>(context.Eip),
                     bytes,
                     registers[0], registers[1], registers[2], registers[3],
                     registers[4], registers[5], registers[6], registers[7],
                     trace.output_address,
                     trace.output_size,
                     aliases.c_str());
}

// One ANSI string argument is decoded for APIs whose arguments identify what
// the guest loads, resolves, or opens. Index counts from the first stack
// argument.
int ApiStringArgumentIndex(const char* name)
{
    if (std::strcmp(name, "LoadLibraryA") == 0 || std::strcmp(name, "CreateFileA") == 0 ||
        std::strcmp(name, "SetCurrentDirectoryA") == 0)
    {
        return 0;
    }
    if (std::strcmp(name, "GetProcAddress") == 0)
    {
        return 1;
    }
    return -1;
}

void SanitizeJsonText(char* text)
{
    for (char* cursor = text; *cursor != '\0'; ++cursor)
    {
        const unsigned char value = static_cast<unsigned char>(*cursor);
        if (value < 0x20 || value > 0x7e || value == '"' || value == '\\')
        {
            *cursor = '.';
        }
    }
}

bool ReadRemoteAnsiString(HANDLE process,
                          std::uint32_t address,
                          char (&buffer)[128])
{
    buffer[0] = '\0';
    SIZE_T copied = 0;
    if (address == 0 ||
        ReadProcessMemory(process,
                          reinterpret_cast<const void*>(address),
                          buffer,
                          sizeof(buffer) - 1,
                          &copied) == FALSE ||
        copied == 0)
    {
        return false;
    }
    buffer[copied] = '\0';
    const std::size_t terminator = strnlen(buffer, copied);
    buffer[terminator] = '\0';
    SanitizeJsonText(buffer);
    return true;
}

void RecordKsndSearchPathState(HANDLE process,
                               std::uint32_t image_base,
                               std::uint32_t caller)
{
    constexpr std::uint32_t kKsndFailureCallerRva = 0x00024813;
    constexpr std::uint32_t kSearchPathEntriesRva = 0x0184c0a0;
    constexpr std::uint32_t kSearchPathCountRva = 0x0184d0e0;
    constexpr std::uint32_t kSearchPathEntryStride = 0x104;
    constexpr std::uint32_t kDiagnosticEntryLimit = 32;
    if (caller != image_base + kKsndFailureCallerRva)
    {
        return;
    }

    std::uint32_t count = 0;
    SIZE_T copied = 0;
    const bool count_readable =
        ReadProcessMemory(process,
                          reinterpret_cast<const void*>(image_base + kSearchPathCountRva),
                          &count,
                          sizeof(count),
                          &copied) != FALSE &&
        copied == sizeof(count);
    const bool capped = count_readable && count > kDiagnosticEntryLimit;
    RecordDiagnostic("{\"event\":\"ksnd_search_path_state\",\"count_address\":\"0x%08x\",\"count_readable\":%s,\"count\":%u,\"diagnostic_limit\":%u,\"capped\":%s}",
                     image_base + kSearchPathCountRva,
                     count_readable ? "true" : "false",
                     count,
                     kDiagnosticEntryLimit,
                     capped ? "true" : "false");
    if (!count_readable)
    {
        return;
    }

    const std::uint32_t observed_count = (std::min)(count, kDiagnosticEntryLimit);
    for (std::uint32_t index = 0; index < observed_count; ++index)
    {
        const std::uint32_t address = image_base + kSearchPathEntriesRva +
                                      index * kSearchPathEntryStride;
        char entry[128] = {};
        const bool readable = ReadRemoteAnsiString(process, address, entry);
        RecordDiagnostic("{\"event\":\"ksnd_search_path_entry\",\"index\":%u,\"address\":\"0x%08x\",\"readable\":%s,\"path\":\"%s\"}",
                         index,
                         address,
                         readable ? "true" : "false",
                         readable ? entry : "");
    }
}

RemoteBufferSnapshot ReadRemoteBufferSnapshot(HANDLE process,
                                              std::uint32_t address,
                                              std::uint32_t size)
{
    RemoteBufferSnapshot snapshot;
    if (address == 0 || size == 0)
    {
        return snapshot;
    }
    constexpr std::size_t kSnapshotLimit = 64;
    std::uint8_t bytes[kSnapshotLimit] = {};
    const SIZE_T requested = (std::min)(static_cast<std::size_t>(size), sizeof(bytes));
    SIZE_T copied = 0;
    if (ReadProcessMemory(process,
                          reinterpret_cast<const void*>(address),
                          bytes,
                          requested,
                          &copied) == FALSE ||
        copied == 0)
    {
        return snapshot;
    }
    snapshot.readable = true;
    snapshot.bytes.resize(copied * 2);
    for (SIZE_T index = 0; index < copied; ++index)
    {
        std::snprintf(snapshot.bytes.data() + index * 2, 3, "%02x", bytes[index]);
    }
    return snapshot;
}

bool ReadRemoteU32(HANDLE process, std::uint32_t address, std::uint32_t* value)
{
    SIZE_T copied = 0;
    return address != 0 && value != nullptr &&
           ReadProcessMemory(process,
                             reinterpret_cast<const void*>(address),
                             value,
                             sizeof(*value),
                             &copied) != FALSE &&
           copied == sizeof(*value);
}

PendingDeviceIoControl RecordDeviceIoControlEntry(DWORD thread_id,
                                                   std::uint32_t caller,
                                                   HANDLE process,
                                                   const std::uint32_t* args)
{
    PendingDeviceIoControl pending;
    pending.return_address = caller;
    std::copy_n(args, pending.args.size(), pending.args.begin());
    pending.input_before = ReadRemoteBufferSnapshot(process, args[2], args[3]);
    pending.output_before = ReadRemoteBufferSnapshot(process, args[4], args[5]);
    pending.have_bytes_returned_before =
        ReadRemoteU32(process, args[6], &pending.bytes_returned_before);
    RecordDiagnostic("{\"event\":\"device_io_control_entry\",\"thread\":%u,\"caller\":\"0x%08x\",\"handle\":\"0x%08x\",\"code\":\"0x%08x\",\"input\":\"0x%08x\",\"input_size\":%u,\"input_readable\":%s,\"input_bytes\":\"%s\",\"output\":\"0x%08x\",\"output_size\":%u,\"output_readable\":%s,\"output_before\":\"%s\",\"bytes_returned\":\"0x%08x\",\"bytes_returned_before_valid\":%s,\"bytes_returned_before\":%u,\"overlapped\":\"0x%08x\"}",
                     static_cast<unsigned>(thread_id),
                     caller,
                     args[0],
                     args[1],
                     args[2],
                     args[3],
                     pending.input_before.readable ? "true" : "false",
                     pending.input_before.bytes.c_str(),
                     args[4],
                     args[5],
                     pending.output_before.readable ? "true" : "false",
                     pending.output_before.bytes.c_str(),
                     args[6],
                     pending.have_bytes_returned_before ? "true" : "false",
                     pending.bytes_returned_before,
                     args[7]);
    return pending;
}

void RecordApiCall(DWORD thread_id,
                   const ApiWatchPoint& watch,
                   std::uintptr_t breakpoint_address,
                   std::uintptr_t caller,
                   HANDLE process,
                   const std::uint32_t* args)
{
    RecordDiagnostic("{\"event\":\"api_call\",\"thread\":%u,\"api\":\"%s\",\"address\":\"0x%08x\",\"caller\":\"0x%08x\",\"args\":[\"0x%08x\",\"0x%08x\",\"0x%08x\",\"0x%08x\"]}",
                     static_cast<unsigned>(thread_id),
                     watch.name.c_str(),
                     static_cast<unsigned>(breakpoint_address),
                     static_cast<unsigned>(caller),
                     args[0],
                     args[1],
                     args[2],
                     args[3]);
    if (watch.string_arg_index >= 0 && watch.string_arg_index < 4)
    {
        char text[128] = {};
        if (ReadRemoteAnsiString(process, args[watch.string_arg_index], text))
        {
            RecordDiagnostic("{\"event\":\"api_call_string\",\"api\":\"%s\",\"arg_index\":%d,\"text\":\"%s\"}",
                             watch.name.c_str(),
                             watch.string_arg_index,
                             text);
        }
    }
}

void RecordAllocationReturn(HANDLE process,
                            DWORD thread_id,
                            const PendingAllocationReturn& pending,
                            const CONTEXT& context,
                            AllocationTraceState* state)
{
    ++state->hit_count;
    if (state->recorded_count >= AllocationTraceState::kHitLimit)
    {
        state->capped = true;
        return;
    }
    const RemoteBufferSnapshot code =
        ReadRemoteBufferSnapshot(process, context.Eip, 24);
    RecordDiagnostic(
        "{\"event\":\"null_context_allocation_return\",\"sequence\":%u,\"thread\":%u,\"api\":\"%s\",\"api_address\":\"0x%08x\",\"return_address\":\"0x%08x\",\"eax\":\"0x%08x\",\"args\":[\"0x%08x\",\"0x%08x\",\"0x%08x\",\"0x%08x\"],\"code_readable\":%s,\"code\":\"%s\"}",
        state->hit_count,
        static_cast<unsigned>(thread_id),
        pending.watch.name.c_str(),
        static_cast<unsigned>(pending.api_address),
        static_cast<unsigned>(pending.return_address),
        static_cast<unsigned>(context.Eax),
        pending.args[0],
        pending.args[1],
        pending.args[2],
        pending.args[3],
        code.readable ? "true" : "false",
        code.readable ? code.bytes.c_str() : "");
    ++state->recorded_count;
}

bool RecordOriginalInitializerWindow(HANDLE process, std::uintptr_t image_base)
{
    constexpr std::uintptr_t kInitializerRva = 0x0005c000;
    std::uint32_t words[8] = {};
    SIZE_T copied = 0;
    if (ReadProcessMemory(process,
                          reinterpret_cast<const void*>(image_base + kInitializerRva),
                          words,
                          sizeof(words),
                          &copied) == FALSE ||
        copied != sizeof(words))
    {
        return false;
    }
    RecordDiagnostic("{\"event\":\"original_initializer_window\",\"base\":\"0x%08x\",\"words\":[\"0x%08x\",\"0x%08x\",\"0x%08x\",\"0x%08x\",\"0x%08x\",\"0x%08x\",\"0x%08x\",\"0x%08x\"]}",
                     static_cast<unsigned>(image_base + kInitializerRva),
                     words[0],
                     words[1],
                     words[2],
                     words[3],
                     words[4],
                     words[5],
                     words[6],
                     words[7]);
    return true;
}

const char* FindSectionNameForRva(const re2dj::exe::PeImageInfo& info, std::uint32_t rva)
{
    for (const re2dj::exe::PeSection& section : info.sections)
    {
        const std::uint32_t span = section.virtual_size > section.raw_size
                                       ? section.virtual_size
                                       : section.raw_size;
        if (span == 0)
        {
            continue;
        }
        if (rva >= section.virtual_address && rva - section.virtual_address < span)
        {
            return section.name.c_str();
        }
    }
    return "";
}

std::uint32_t RecordAccessViolationCallAttribution(
    HANDLE process,
    std::size_t stack_index,
    std::uint32_t return_address,
    const CONTEXT& context,
    std::uintptr_t image_base,
    const re2dj::exe::PeImageInfo& image_info)
{
    if (return_address < 6)
    {
        return 0;
    }

    std::uint8_t bytes[8] = {};
    SIZE_T copied = 0;
    const std::uintptr_t window_base =
        static_cast<std::uintptr_t>(return_address) - 6;
    if (ReadProcessMemory(process,
                          reinterpret_cast<const void*>(window_base),
                          bytes,
                          sizeof(bytes),
                          &copied) == FALSE ||
        copied < 6)
    {
        return 0;
    }

    const char* kind = nullptr;
    const char* register_name = "";
    std::uint32_t call_address = 0;
    std::uint32_t slot_address = 0;
    std::uint32_t target_address = 0;
    bool slot_readable = false;
    std::uint32_t register_values[] = {
        context.Eax, context.Ecx, context.Edx, context.Ebx,
        context.Esp, context.Ebp, context.Esi, context.Edi,
    };
    constexpr const char* kRegisterNames[] = {
        "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi",
    };

    if (bytes[0] == 0xff && bytes[1] == 0x15)
    {
        kind = "absolute_memory";
        call_address = return_address - 6;
        slot_address = static_cast<std::uint32_t>(bytes[2]) |
                       (static_cast<std::uint32_t>(bytes[3]) << 8) |
                       (static_cast<std::uint32_t>(bytes[4]) << 16) |
                       (static_cast<std::uint32_t>(bytes[5]) << 24);
        SIZE_T slot_copied = 0;
        slot_readable = ReadProcessMemory(
                            process,
                            reinterpret_cast<const void*>(
                                static_cast<std::uintptr_t>(slot_address)),
                            &target_address,
                            sizeof(target_address),
                            &slot_copied) != FALSE &&
                        slot_copied == sizeof(target_address);
    }
    else if (bytes[1] == 0xe8)
    {
        kind = "relative_direct";
        call_address = return_address - 5;
        const std::uint32_t relative = static_cast<std::uint32_t>(bytes[2]) |
                                       (static_cast<std::uint32_t>(bytes[3]) << 8) |
                                       (static_cast<std::uint32_t>(bytes[4]) << 16) |
                                       (static_cast<std::uint32_t>(bytes[5]) << 24);
        target_address = static_cast<std::uint32_t>(
            static_cast<std::int64_t>(return_address) +
            static_cast<std::int32_t>(relative));
    }
    else if (bytes[4] == 0xff && (bytes[5] & 0xf8) == 0xd0 &&
             (bytes[5] & 0xc0) == 0xc0)
    {
        kind = "register_indirect";
        call_address = return_address - 2;
        const unsigned register_index = bytes[5] & 0x07;
        register_name = kRegisterNames[register_index];
        target_address = register_values[register_index];
    }
    else
    {
        return 0;
    }

    MEMORY_BASIC_INFORMATION target_region = {};
    const bool target_region_readable =
        target_address != 0 &&
        VirtualQueryEx(process,
                       reinterpret_cast<const void*>(
                           static_cast<std::uintptr_t>(target_address)),
                       &target_region,
                       sizeof(target_region)) == sizeof(target_region);
    std::string target_symbol;
    if (target_region_readable && target_region.Type == MEM_IMAGE &&
        target_region.AllocationBase != nullptr)
    {
        re2dj::tools::windows_x86_launcher_probe::RemoteNearestExport nearest = {};
        std::string nearest_error;
        const std::uintptr_t target_module_base = reinterpret_cast<std::uintptr_t>(
            target_region.AllocationBase);
        if (re2dj::tools::windows_x86_launcher_probe::FindRemotePe32NearestExport(
                process,
                target_module_base,
                target_address,
                &nearest,
                &nearest_error))
        {
            char symbol[288] = {};
            std::snprintf(symbol,
                          sizeof(symbol),
                          "%s!%s+0x%x",
                          nearest.module,
                          nearest.function,
                          static_cast<unsigned>(nearest.offset));
            SanitizeJsonText(symbol);
            target_symbol = symbol;
        }
    }

    std::string target_section;
    if (target_address >= image_base &&
        target_address < image_base + image_info.size_of_image &&
        target_region_readable &&
        reinterpret_cast<std::uintptr_t>(target_region.AllocationBase) == image_base)
    {
        target_section = FindSectionNameForRva(
            image_info,
            static_cast<std::uint32_t>(target_address - image_base));
    }
    const RemoteBufferSnapshot target_code =
        target_region_readable
            ? ReadRemoteBufferSnapshot(process, target_address, 32)
            : RemoteBufferSnapshot{};
    // Follow a short chain of runtime relative jumps because protected image
    // code commonly leaves a stable call target as a dispatch trampoline.
    std::uintptr_t target_window_address = target_address;
    for (unsigned hop = 0; hop < 4 && target_window_address != 0; ++hop)
    {
        std::uint8_t target_window[32] = {};
        SIZE_T target_window_copied = 0;
        const bool target_window_readable =
            ReadProcessMemory(process,
                              reinterpret_cast<const void*>(target_window_address),
                              target_window,
                              sizeof(target_window),
                              &target_window_copied) != FALSE &&
            target_window_copied != 0;
        char target_window_text[sizeof(target_window) * 2 + 1] = {};
        for (SIZE_T index = 0; index < target_window_copied; ++index)
        {
            std::snprintf(target_window_text + index * 2,
                          3,
                          "%02x",
                          target_window[index]);
        }
        RecordDiagnostic(
            "{\"event\":\"av_call_target_window\",\"stack_index\":%u,\"hop\":%u,\"address\":\"0x%08x\",\"readable\":%s,\"bytes\":\"%s\"}",
            static_cast<unsigned>(stack_index),
            hop,
            static_cast<unsigned>(target_window_address),
            target_window_readable ? "true" : "false",
            target_window_text);
        if (!target_window_readable || target_window_copied < 5 ||
            target_window[0] != 0xe9)
        {
            break;
        }
        const std::uint32_t relative_bits =
            static_cast<std::uint32_t>(target_window[1]) |
            (static_cast<std::uint32_t>(target_window[2]) << 8) |
            (static_cast<std::uint32_t>(target_window[3]) << 16) |
            (static_cast<std::uint32_t>(target_window[4]) << 24);
        const std::uintptr_t next_target =
            static_cast<std::uintptr_t>(static_cast<std::int64_t>(
                target_window_address + 5) +
                                        static_cast<std::int32_t>(relative_bits));
        if (next_target == target_window_address)
        {
            break;
        }
        target_window_address = next_target;
    }

    constexpr std::size_t kCallsiteWindowSize = 96;
    std::uint8_t callsite_window[kCallsiteWindowSize] = {};
    const std::uintptr_t callsite_window_base =
        call_address >= image_base + 48 ? call_address - 48 : image_base;
    const SIZE_T callsite_window_requested = static_cast<SIZE_T>(
        (std::min)(kCallsiteWindowSize,
                   static_cast<std::size_t>(image_base + image_info.size_of_image -
                                            callsite_window_base)));
    SIZE_T callsite_window_copied = 0;
    const bool callsite_window_readable =
        callsite_window_requested != 0 &&
        ReadProcessMemory(process,
                          reinterpret_cast<const void*>(callsite_window_base),
                          callsite_window,
                          callsite_window_requested,
                          &callsite_window_copied) != FALSE &&
        callsite_window_copied != 0;
    char callsite_window_text[kCallsiteWindowSize * 2 + 1] = {};
    for (SIZE_T index = 0; index < callsite_window_copied; ++index)
    {
        std::snprintf(callsite_window_text + index * 2,
                      3,
                      "%02x",
                      callsite_window[index]);
    }
    RecordDiagnostic(
        "{\"event\":\"av_callsite_window\",\"stack_index\":%u,\"call_address\":\"0x%08x\",\"base\":\"0x%08x\",\"readable\":%s,\"bytes\":\"%s\"}",
        static_cast<unsigned>(stack_index),
        call_address,
        static_cast<unsigned>(callsite_window_base),
        callsite_window_readable ? "true" : "false",
        callsite_window_text);

    char byte_text[sizeof(bytes) * 2 + 1] = {};
    const std::size_t byte_count =
        (std::min)(static_cast<std::size_t>(copied), sizeof(bytes));
    for (std::size_t index = 0; index < byte_count; ++index)
    {
        std::snprintf(byte_text + index * 2, 3, "%02x", bytes[index]);
    }
    RecordDiagnostic(
        "{\"event\":\"av_indirect_call\",\"stack_index\":%u,\"return_address\":\"0x%08x\",\"call_address\":\"0x%08x\",\"kind\":\"%s\",\"bytes\":\"%s\",\"register\":\"%s\",\"slot\":\"0x%08x\",\"slot_readable\":%s,\"target\":\"0x%08x\",\"target_region_readable\":%s,\"target_region\":\"0x%08x\",\"target_allocation\":\"0x%08x\",\"target_protect\":\"0x%08x\",\"target_state\":\"0x%08x\",\"target_type\":\"0x%08x\",\"target_section\":\"%s\",\"target_symbol\":\"%s\",\"target_code_readable\":%s,\"target_code\":\"%s\"}",
        static_cast<unsigned>(stack_index),
        return_address,
        call_address,
        kind,
        byte_text,
        register_name,
        slot_address,
        slot_readable ? "true" : "false",
        target_address,
        target_region_readable ? "true" : "false",
        static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(
            target_region.BaseAddress)),
        static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(
            target_region.AllocationBase)),
        static_cast<unsigned>(target_region.Protect),
        static_cast<unsigned>(target_region.State),
        static_cast<unsigned>(target_region.Type),
        target_section.c_str(),
        target_symbol.c_str(),
        target_code.readable ? "true" : "false",
        target_code.readable ? target_code.bytes.c_str() : "");
    return slot_address;
}

void ScanAccessViolationSlotReferences(HANDLE process,
                                       std::uint32_t slot_address,
                                       std::uintptr_t image_base,
                                       const re2dj::exe::PeImageInfo& image_info)
{
    constexpr SIZE_T kBlockSize = 64 * 1024;
    constexpr std::uint32_t kResultLimit = 64;
    const std::uintptr_t image_end =
        image_base + static_cast<std::uintptr_t>(image_info.size_of_image);
    std::uint64_t scanned_bytes = 0;
    std::uint32_t result_count = 0;
    bool capped = false;
    std::uintptr_t address = image_base;
    while (address < image_end)
    {
        MEMORY_BASIC_INFORMATION memory = {};
        if (VirtualQueryEx(process,
                           reinterpret_cast<const void*>(address),
                           &memory,
                           sizeof(memory)) != sizeof(memory))
        {
            break;
        }
        const std::uintptr_t region_base =
            (std::max)(reinterpret_cast<std::uintptr_t>(memory.BaseAddress), image_base);
        const std::uintptr_t raw_region_end =
            reinterpret_cast<std::uintptr_t>(memory.BaseAddress) + memory.RegionSize;
        const std::uintptr_t region_end = (std::min)(raw_region_end, image_end);
        if (memory.State == MEM_COMMIT &&
            (memory.Protect & (PAGE_NOACCESS | PAGE_GUARD)) == 0)
        {
            for (std::uintptr_t cursor = region_base; cursor < region_end;
                 cursor += kBlockSize)
            {
                const SIZE_T remaining = static_cast<SIZE_T>(region_end - cursor);
                const SIZE_T logical_size = (std::min)(remaining, kBlockSize);
                const SIZE_T requested =
                    (std::min)(remaining, kBlockSize + sizeof(slot_address) - 1);
                std::vector<std::uint8_t> bytes(requested);
                SIZE_T copied = 0;
                if (ReadProcessMemory(process,
                                      reinterpret_cast<const void*>(cursor),
                                      bytes.data(),
                                      bytes.size(),
                                      &copied) == FALSE ||
                    copied < sizeof(slot_address))
                {
                    continue;
                }
                scanned_bytes += logical_size;
                const SIZE_T candidate_count =
                    (std::min)(logical_size, copied - sizeof(slot_address) + 1);
                for (SIZE_T index = 0; index < candidate_count; ++index)
                {
                    std::uint32_t value = 0;
                    std::memcpy(&value, bytes.data() + index, sizeof(value));
                    if (value != slot_address)
                    {
                        continue;
                    }
                    if (result_count == kResultLimit)
                    {
                        capped = true;
                        break;
                    }
                    ++result_count;
                    const std::uintptr_t match_address = cursor + index;
                    const std::uintptr_t window_base =
                        match_address >= image_base + 8 ? match_address - 8 : image_base;
                    std::uint8_t window[24] = {};
                    const SIZE_T window_requested = static_cast<SIZE_T>(
                        (std::min)(sizeof(window),
                                   static_cast<std::size_t>(image_end - window_base)));
                    SIZE_T window_copied = 0;
                    const bool window_readable =
                        ReadProcessMemory(process,
                                          reinterpret_cast<const void*>(window_base),
                                          window,
                                          window_requested,
                                          &window_copied) != FALSE &&
                        window_copied != 0;
                    char window_text[sizeof(window) * 2 + 1] = {};
                    if (window_readable)
                    {
                        for (SIZE_T byte_index = 0; byte_index < window_copied;
                             ++byte_index)
                        {
                            std::snprintf(window_text + byte_index * 2,
                                          3,
                                          "%02x",
                                          window[byte_index]);
                        }
                    }
                    const std::uint32_t rva = static_cast<std::uint32_t>(
                        match_address - image_base);
                    RecordDiagnostic(
                        "{\"event\":\"av_slot_reference\",\"slot\":\"0x%08x\",\"location\":\"0x%08x\",\"rva\":\"0x%08x\",\"section\":\"%s\",\"window_base\":\"0x%08x\",\"window_readable\":%s,\"bytes\":\"%s\"}",
                        slot_address,
                        static_cast<unsigned>(match_address),
                        rva,
                        FindSectionNameForRva(image_info, rva),
                        static_cast<unsigned>(window_base),
                        window_readable ? "true" : "false",
                        window_text);
                }
                if (capped)
                {
                    break;
                }
            }
        }
        if (capped || region_end <= address)
        {
            break;
        }
        address = region_end;
    }
    RecordDiagnostic(
        "{\"event\":\"av_slot_reference_summary\",\"slot\":\"0x%08x\",\"image_base\":\"0x%08x\",\"scanned_bytes\":%llu,\"matches\":%u,\"capped\":%s}",
        slot_address,
        static_cast<unsigned>(image_base),
        static_cast<unsigned long long>(scanned_bytes),
        result_count,
        capped ? "true" : "false");
}

// Captures enough state to attribute an indirect execute fault to the guest
// instruction and image-resident pointer table that supplied its target.
void RecordAccessViolationContext(HANDLE process,
                                  DWORD thread_id,
                                  const EXCEPTION_RECORD& record,
                                  std::uintptr_t image_base,
                                  const re2dj::exe::PeImageInfo* image_info)
{
    const ULONG_PTR access_code = record.NumberParameters >= 1 ? record.ExceptionInformation[0] : 0;
    const ULONG_PTR access_address =
        record.NumberParameters >= 2 ? record.ExceptionInformation[1] : 0;
    const char* access_kind = access_code == 0 ? "read"
                              : access_code == 1 ? "write"
                              : access_code == 8 ? "execute"
                                                 : "unknown";
    RecordDiagnostic("{\"event\":\"av_access\",\"thread\":%u,\"kind\":\"%s\",\"code\":\"0x%08x\",\"address\":\"0x%08x\"}",
                     static_cast<unsigned>(thread_id),
                     access_kind,
                     static_cast<unsigned>(access_code),
                     static_cast<unsigned>(access_address));

    HANDLE thread = OpenThread(THREAD_GET_CONTEXT, FALSE, thread_id);
    CONTEXT context = {};
    context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS;
    const bool have_context = thread != nullptr && GetThreadContext(thread, &context) != FALSE;
    if (thread != nullptr)
    {
        CloseHandle(thread);
    }
    if (!have_context)
    {
        RecordDiagnostic("{\"event\":\"av_context_error\",\"reason\":\"cannot read fault thread context\"}");
        return;
    }

    RecordDiagnostic("{\"event\":\"av_registers\",\"thread\":%u,\"eax\":\"0x%08x\",\"ebx\":\"0x%08x\",\"ecx\":\"0x%08x\",\"edx\":\"0x%08x\",\"esi\":\"0x%08x\",\"edi\":\"0x%08x\",\"ebp\":\"0x%08x\",\"esp\":\"0x%08x\",\"eip\":\"0x%08x\",\"flags\":\"0x%08x\"}",
                     static_cast<unsigned>(thread_id),
                     static_cast<unsigned>(context.Eax),
                     static_cast<unsigned>(context.Ebx),
                     static_cast<unsigned>(context.Ecx),
                     static_cast<unsigned>(context.Edx),
                     static_cast<unsigned>(context.Esi),
                     static_cast<unsigned>(context.Edi),
                     static_cast<unsigned>(context.Ebp),
                     static_cast<unsigned>(context.Esp),
                     static_cast<unsigned>(context.Eip),
                     static_cast<unsigned>(context.EFlags));

    std::uint32_t caller_ebp = 0;
    std::uint32_t callee_return_address = 0;
    std::uint32_t caller_local_minus8 = 0;
    std::uint32_t caller_local_minus4 = 0;
    std::uint32_t caller_return_address = 0;
    const bool caller_ebp_readable =
        ReadRemoteU32(process, context.Ebp, &caller_ebp);
    const bool callee_return_readable =
        ReadRemoteU32(process, context.Ebp + 4, &callee_return_address);
    const bool caller_local_minus8_readable =
        caller_ebp_readable && caller_ebp >= 8 &&
        ReadRemoteU32(process, caller_ebp - 8, &caller_local_minus8);
    const bool caller_local_minus4_readable =
        caller_ebp_readable && caller_ebp >= 4 &&
        ReadRemoteU32(process, caller_ebp - 4, &caller_local_minus4);
    const bool caller_return_readable =
        caller_ebp_readable &&
        ReadRemoteU32(process, caller_ebp + 4, &caller_return_address);
    RecordDiagnostic(
        "{\"event\":\"av_caller_frame\",\"thread\":%u,\"callee_ebp\":\"0x%08x\",\"callee_return_address\":\"0x%08x\",\"callee_return_readable\":%s,\"caller_ebp\":\"0x%08x\",\"caller_ebp_readable\":%s,\"caller_local_minus8\":\"0x%08x\",\"caller_local_minus8_readable\":%s,\"caller_local_minus4\":\"0x%08x\",\"caller_local_minus4_readable\":%s,\"caller_return_address\":\"0x%08x\",\"caller_return_readable\":%s}",
        static_cast<unsigned>(thread_id),
        static_cast<unsigned>(context.Ebp),
        static_cast<unsigned>(callee_return_address),
        callee_return_readable ? "true" : "false",
        static_cast<unsigned>(caller_ebp),
        caller_ebp_readable ? "true" : "false",
        static_cast<unsigned>(caller_local_minus8),
        caller_local_minus8_readable ? "true" : "false",
        static_cast<unsigned>(caller_local_minus4),
        caller_local_minus4_readable ? "true" : "false",
        static_cast<unsigned>(caller_return_address),
        caller_return_readable ? "true" : "false");

    std::uint32_t outer_ebp = 0;
    std::uint32_t outer_local_minus_0x118 = 0;
    std::uint32_t outer_local_minus_0x11c = 0;
    std::uint32_t outer_ecx_source = 0;
    std::uint32_t outer_return_address = 0;
    const bool outer_ebp_readable =
        caller_ebp_readable && ReadRemoteU32(process, caller_ebp, &outer_ebp);
    const bool outer_local_minus_0x118_readable =
        outer_ebp_readable && outer_ebp >= 0x118 &&
        ReadRemoteU32(process, outer_ebp - 0x118, &outer_local_minus_0x118);
    const bool outer_local_minus_0x11c_readable =
        outer_ebp_readable && outer_ebp >= 0x11c &&
        ReadRemoteU32(process, outer_ebp - 0x11c, &outer_local_minus_0x11c);
    const bool outer_ecx_source_readable =
        outer_local_minus_0x118_readable &&
        outer_local_minus_0x118 <= 0xffffffffu - 0x11c &&
        ReadRemoteU32(process,
                      outer_local_minus_0x118 + 0x11c,
                      &outer_ecx_source);
    const bool outer_return_readable =
        outer_ebp_readable &&
        ReadRemoteU32(process, outer_ebp + 4, &outer_return_address);
    RecordDiagnostic(
        "{\"event\":\"av_outer_frame\",\"thread\":%u,\"outer_ebp\":\"0x%08x\",\"outer_ebp_readable\":%s,\"outer_local_minus_0x118\":\"0x%08x\",\"outer_local_minus_0x118_readable\":%s,\"outer_local_minus_0x11c\":\"0x%08x\",\"outer_local_minus_0x11c_readable\":%s,\"outer_ecx_source\":\"0x%08x\",\"outer_ecx_source_readable\":%s,\"outer_return_address\":\"0x%08x\",\"outer_return_readable\":%s}",
        static_cast<unsigned>(thread_id),
        static_cast<unsigned>(outer_ebp),
        outer_ebp_readable ? "true" : "false",
        static_cast<unsigned>(outer_local_minus_0x118),
        outer_local_minus_0x118_readable ? "true" : "false",
        static_cast<unsigned>(outer_local_minus_0x11c),
        outer_local_minus_0x11c_readable ? "true" : "false",
        static_cast<unsigned>(outer_ecx_source),
        outer_ecx_source_readable ? "true" : "false",
        static_cast<unsigned>(outer_return_address),
        outer_return_readable ? "true" : "false");

    constexpr std::size_t kStackWords = 8;
    std::uint32_t stack[kStackWords] = {};
    SIZE_T stack_copied = 0;
    const bool have_stack =
        ReadProcessMemory(process,
                          reinterpret_cast<const void*>(context.Esp),
                          stack,
                          sizeof(stack),
                          &stack_copied) != FALSE &&
        stack_copied >= sizeof(std::uint32_t);

    if (image_info == nullptr)
    {
        return;
    }
    const std::uintptr_t image_end =
        image_base + static_cast<std::uintptr_t>(image_info->size_of_image);
    if (caller_return_readable && caller_return_address >= image_base + 32 &&
        caller_return_address < image_end)
    {
        constexpr std::size_t kCallerReturnWindowSize = 64;
        const std::uintptr_t window_base = caller_return_address - 32;
        std::uint8_t window[kCallerReturnWindowSize] = {};
        SIZE_T window_copied = 0;
        const bool window_readable =
            ReadProcessMemory(process,
                              reinterpret_cast<const void*>(window_base),
                              window,
                              sizeof(window),
                              &window_copied) != FALSE &&
            window_copied != 0;
        char window_text[kCallerReturnWindowSize * 2 + 1] = {};
        for (SIZE_T index = 0; index < window_copied; ++index)
        {
            std::snprintf(window_text + index * 2,
                          3,
                          "%02x",
                          window[index]);
        }
        RecordDiagnostic(
            "{\"event\":\"av_caller_return_window\",\"thread\":%u,\"return_address\":\"0x%08x\",\"base\":\"0x%08x\",\"readable\":%s,\"bytes\":\"%s\"}",
            static_cast<unsigned>(thread_id),
            static_cast<unsigned>(caller_return_address),
            static_cast<unsigned>(window_base),
            window_readable ? "true" : "false",
            window_text);
    }
    struct NamedRegister
    {
        const char* name;
        std::uint32_t value;
    };
    const NamedRegister registers[] = {
        {"eax", context.Eax}, {"ebx", context.Ebx}, {"ecx", context.Ecx},
        {"edx", context.Edx}, {"esi", context.Esi}, {"edi", context.Edi},
        {"ebp", context.Ebp}, {"esp", context.Esp}, {"eip", context.Eip},
    };
    for (const NamedRegister& reg : registers)
    {
        if (reg.value < image_base || reg.value >= image_end)
        {
            continue;
        }
        const std::uintptr_t aligned = reg.value & ~static_cast<std::uintptr_t>(3);
        const std::uintptr_t start = aligned >= image_base + 8 ? aligned - 8 : image_base;
        std::uint32_t words[8] = {};
        SIZE_T copied = 0;
        const SIZE_T requested = static_cast<SIZE_T>(
            (std::min)(sizeof(words), static_cast<std::size_t>(image_end - start)));
        if (ReadProcessMemory(process,
                              reinterpret_cast<const void*>(start),
                              words,
                              requested,
                              &copied) == FALSE ||
            copied < sizeof(std::uint32_t))
        {
            continue;
        }
        const std::size_t word_count = copied / sizeof(std::uint32_t);
        std::string values;
        for (std::size_t index = 0; index < word_count; ++index)
        {
            char word[16] = {};
            std::snprintf(word, sizeof(word), "\"0x%08x\"", words[index]);
            if (index != 0)
            {
                values += ',';
            }
            values += word;
        }
        const std::uint32_t rva = static_cast<std::uint32_t>(reg.value - image_base);
        RecordDiagnostic("{\"event\":\"av_image_pointer_window\",\"register\":\"%s\",\"address\":\"0x%08x\",\"base\":\"0x%08x\",\"section\":\"%s\",\"words\":[%s]}",
                         reg.name,
                         reg.value,
                         static_cast<unsigned>(start),
                         FindSectionNameForRva(*image_info, rva),
                         values.c_str());
    }

    if (!have_stack)
    {
        return;
    }
    const std::size_t stack_word_count = stack_copied / sizeof(std::uint32_t);
    std::set<std::uint32_t> recorded_addresses;
    std::set<std::uint32_t> recorded_slots;
    for (std::size_t index = 0; index < stack_word_count; ++index)
    {
        const std::uint32_t address = stack[index];
        if (address < image_base || address >= image_end ||
            !recorded_addresses.insert(address).second)
        {
            continue;
        }
        const std::uintptr_t start = address >= image_base + 8 ? address - 8 : image_base;
        std::uint8_t bytes[24] = {};
        SIZE_T copied = 0;
        const SIZE_T requested = static_cast<SIZE_T>(
            (std::min)(sizeof(bytes), static_cast<std::size_t>(image_end - start)));
        if (ReadProcessMemory(process,
                              reinterpret_cast<const void*>(start),
                              bytes,
                              requested,
                              &copied) == FALSE ||
            copied == 0)
        {
            continue;
        }
        char text[sizeof(bytes) * 2 + 1] = {};
        for (SIZE_T byte_index = 0; byte_index < copied; ++byte_index)
        {
            std::snprintf(text + byte_index * 2, 3, "%02x", bytes[byte_index]);
        }
        const std::uint32_t slot_address =
            RecordAccessViolationCallAttribution(process,
                                                 index,
                                                 address,
                                                 context,
                                                 image_base,
                                                 *image_info);
        if (slot_address != 0 && recorded_slots.insert(slot_address).second)
        {
            ScanAccessViolationSlotReferences(process,
                                              slot_address,
                                              image_base,
                                              *image_info);
        }
        const std::uint32_t rva = static_cast<std::uint32_t>(address - image_base);
        RecordDiagnostic("{\"event\":\"av_stack_code_window\",\"index\":%u,\"address\":\"0x%08x\",\"base\":\"0x%08x\",\"section\":\"%s\",\"bytes\":\"%s\"}",
                         static_cast<unsigned>(index),
                         address,
                         static_cast<unsigned>(start),
                         FindSectionNameForRva(*image_info, rva),
                         text);
    }
}

void RecordPrivilegedInstructionContext(HANDLE process,
                                        DWORD thread_id,
                                        const EXCEPTION_RECORD& record,
                                        bool first_chance)
{
    HANDLE thread = OpenThread(THREAD_GET_CONTEXT, FALSE, thread_id);
    CONTEXT context = {};
    context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS;
    const bool have_context = thread != nullptr && GetThreadContext(thread, &context) != FALSE;
    if (thread != nullptr)
    {
        CloseHandle(thread);
    }
    if (!have_context)
    {
        RecordDiagnostic(
            "{\"event\":\"privileged_instruction_context_error\",\"code\":\"0x%08x\",\"first_chance\":%s,\"address\":\"0x%08x\"}",
            static_cast<unsigned>(record.ExceptionCode),
            first_chance ? "true" : "false",
            static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(record.ExceptionAddress)));
        return;
    }

    std::uint8_t bytes[16] = {};
    SIZE_T copied = 0;
    const bool have_bytes = ReadProcessMemory(
                                process,
                                record.ExceptionAddress,
                                bytes,
                                sizeof(bytes),
                                &copied) != FALSE &&
                            copied != 0;
    char byte_text[sizeof(bytes) * 2 + 1] = {};
    if (have_bytes)
    {
        for (SIZE_T index = 0; index < copied; ++index)
        {
            std::snprintf(byte_text + index * 2, 3, "%02x", bytes[index]);
        }
    }
    const char* kind = !have_bytes ? "unknown"
                       : bytes[0] == 0xec ? "in_al_dx"
                       : bytes[0] == 0xee ? "out_dx_al"
                                           : "unknown";
    const std::uint32_t address = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(record.ExceptionAddress));
    RecordDiagnostic(
        "{\"event\":\"privileged_instruction\",\"code\":\"0x%08x\",\"first_chance\":%s,\"address\":\"0x%08x\",\"kind\":\"%s\",\"opcode\":\"%02x\",\"bytes\":\"%s\",\"eax\":\"0x%08x\",\"ebx\":\"0x%08x\",\"ecx\":\"0x%08x\",\"edx\":\"0x%08x\",\"esi\":\"0x%08x\",\"edi\":\"0x%08x\",\"ebp\":\"0x%08x\",\"esp\":\"0x%08x\",\"eip\":\"0x%08x\",\"flags\":\"0x%08x\",\"port\":\"0x%04x\",\"value\":\"0x%02x\"}",
        static_cast<unsigned>(record.ExceptionCode),
        first_chance ? "true" : "false",
        address,
        kind,
        have_bytes ? static_cast<unsigned>(bytes[0]) : 0u,
        byte_text,
        static_cast<unsigned>(context.Eax),
        static_cast<unsigned>(context.Ebx),
        static_cast<unsigned>(context.Ecx),
        static_cast<unsigned>(context.Edx),
        static_cast<unsigned>(context.Esi),
        static_cast<unsigned>(context.Edi),
        static_cast<unsigned>(context.Ebp),
        static_cast<unsigned>(context.Esp),
        static_cast<unsigned>(context.Eip),
        static_cast<unsigned>(context.EFlags),
        static_cast<unsigned>(context.Edx & 0xffffu),
        static_cast<unsigned>(context.Eax & 0xffu));
}

// Dumps registers, stack, fault page bytes, and the containing allocation's
// regions while the child is stopped on the first-chance illegal instruction.
void RecordIllegalInstructionContext(HANDLE process,
                                     DWORD thread_id,
                                     const EXCEPTION_RECORD& record,
                                     std::uintptr_t image_base,
                                     const re2dj::exe::PeImageInfo* image_info)
{
    HANDLE thread = OpenThread(THREAD_GET_CONTEXT, FALSE, thread_id);
    CONTEXT context = {};
    context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS;
    const bool have_context = thread != nullptr && GetThreadContext(thread, &context) != FALSE;
    if (thread != nullptr)
    {
        CloseHandle(thread);
    }
    if (!have_context)
    {
        RecordDiagnostic("{\"event\":\"fault_context_error\",\"reason\":\"cannot read fault thread context\"}");
        return;
    }
    RecordDiagnostic("{\"event\":\"fault_registers\",\"eax\":\"0x%08x\",\"ebx\":\"0x%08x\",\"ecx\":\"0x%08x\",\"edx\":\"0x%08x\",\"esi\":\"0x%08x\",\"edi\":\"0x%08x\",\"ebp\":\"0x%08x\",\"esp\":\"0x%08x\",\"eip\":\"0x%08x\",\"flags\":\"0x%08x\",\"cs\":\"0x%04x\",\"ds\":\"0x%04x\",\"es\":\"0x%04x\",\"fs\":\"0x%04x\",\"gs\":\"0x%04x\",\"ss\":\"0x%04x\"}",
                     static_cast<unsigned>(context.Eax),
                     static_cast<unsigned>(context.Ebx),
                     static_cast<unsigned>(context.Ecx),
                     static_cast<unsigned>(context.Edx),
                     static_cast<unsigned>(context.Esi),
                     static_cast<unsigned>(context.Edi),
                     static_cast<unsigned>(context.Ebp),
                     static_cast<unsigned>(context.Esp),
                     static_cast<unsigned>(context.Eip),
                     static_cast<unsigned>(context.EFlags),
                     static_cast<unsigned>(context.SegCs),
                     static_cast<unsigned>(context.SegDs),
                     static_cast<unsigned>(context.SegEs),
                     static_cast<unsigned>(context.SegFs),
                     static_cast<unsigned>(context.SegGs),
                     static_cast<unsigned>(context.SegSs));

    constexpr std::size_t kStackWords = 64;
    std::uint32_t stack[kStackWords] = {};
    SIZE_T stack_copied = 0;
    if (ReadProcessMemory(process,
                          reinterpret_cast<const void*>(context.Esp),
                          stack,
                          sizeof(stack),
                          &stack_copied) != FALSE &&
        stack_copied >= sizeof(std::uint32_t))
    {
        const std::size_t word_count = stack_copied / sizeof(std::uint32_t);
        std::string words;
        words.reserve(word_count * 14);
        for (std::size_t index = 0; index < word_count; ++index)
        {
            char word[16] = {};
            std::snprintf(word, sizeof(word), "\"0x%08x\"", stack[index]);
            if (index != 0)
            {
                words += ',';
            }
            words += word;
        }
        RecordDiagnostic("{\"event\":\"fault_stack\",\"esp\":\"0x%08x\",\"words\":[%s]}",
                         static_cast<unsigned>(context.Esp),
                         words.c_str());
        if (image_info != nullptr)
        {
            const std::uintptr_t image_end =
                image_base + static_cast<std::uintptr_t>(image_info->size_of_image);
            for (std::size_t index = 0; index < word_count; ++index)
            {
                if (stack[index] < image_base || stack[index] >= image_end)
                {
                    continue;
                }
                const std::uint32_t rva =
                    static_cast<std::uint32_t>(stack[index] - image_base);
                RecordDiagnostic("{\"event\":\"fault_stack_image_reference\",\"index\":%u,\"value\":\"0x%08x\",\"rva\":\"0x%08x\",\"section\":\"%s\"}",
                                 static_cast<unsigned>(index),
                                 stack[index],
                                 rva,
                                 FindSectionNameForRva(*image_info, rva));
            }
        }
    }

    const std::uintptr_t fault_address =
        reinterpret_cast<std::uintptr_t>(record.ExceptionAddress);
    const std::uintptr_t page_base = fault_address & ~static_cast<std::uintptr_t>(0xfff);
    std::uint8_t page_bytes[128] = {};
    SIZE_T page_copied = 0;
    if (ReadProcessMemory(process,
                          reinterpret_cast<const void*>(page_base),
                          page_bytes,
                          sizeof(page_bytes),
                          &page_copied) != FALSE &&
        page_copied > 0)
    {
        char text[sizeof(page_bytes) * 2 + 1] = {};
        for (SIZE_T index = 0; index < page_copied; ++index)
        {
            std::snprintf(text + index * 2, 3, "%02x", page_bytes[index]);
        }
        RecordDiagnostic("{\"event\":\"fault_page_dump\",\"page\":\"0x%08x\",\"bytes\":\"%s\"}",
                         static_cast<unsigned>(page_base),
                         text);
    }

    MEMORY_BASIC_INFORMATION seed = {};
    if (VirtualQueryEx(process,
                       record.ExceptionAddress,
                       &seed,
                       sizeof(seed)) == sizeof(seed) &&
        seed.AllocationBase != nullptr)
    {
        const std::uintptr_t allocation_base =
            reinterpret_cast<std::uintptr_t>(seed.AllocationBase);
        std::uintptr_t walk = allocation_base;
        for (unsigned region_count = 0; region_count < 64; ++region_count)
        {
            MEMORY_BASIC_INFORMATION region = {};
            if (VirtualQueryEx(process,
                               reinterpret_cast<const void*>(walk),
                               &region,
                               sizeof(region)) != sizeof(region) ||
                reinterpret_cast<std::uintptr_t>(region.AllocationBase) != allocation_base)
            {
                break;
            }
            RecordDiagnostic("{\"event\":\"fault_allocation_region\",\"allocation\":\"0x%08x\",\"base\":\"0x%08x\",\"size\":\"0x%08x\",\"protect\":\"0x%08x\",\"state\":\"0x%08x\",\"type\":\"0x%08x\"}",
                             static_cast<unsigned>(allocation_base),
                             static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(region.BaseAddress)),
                             static_cast<unsigned>(region.RegionSize),
                             static_cast<unsigned>(region.Protect),
                             static_cast<unsigned>(region.State),
                             static_cast<unsigned>(region.Type));
            walk += region.RegionSize;
            if (walk < allocation_base)
            {
                break;
            }
        }
    }

    // Trace where the run-invariant entry value is stored, including planted
    // consecutive runs that could feed the fault-signature registers.
    if (image_info != nullptr &&
        image_base + static_cast<std::uintptr_t>(image_info->entry_point_rva) <=
            (std::numeric_limits<std::uint32_t>::max)())
    {
        ScanEntryReferences(
            process,
            static_cast<std::uint32_t>(image_base +
                                       static_cast<std::uintptr_t>(image_info->entry_point_rva)));
    }
}

bool InstallApiTraceBreakpoints(HANDLE process,
                                std::uintptr_t kernel32_base,
                                std::uintptr_t kernelbase_base,
                                std::uintptr_t user32_base,
                                ApiWatchMap* watches,
                                bool allocation_only,
                                std::string* error)
{
    struct WatchedModule
    {
        const char* name;
        std::uintptr_t base;
    };
    // kernelbase comes first: forwarder chains bind to its real code, so it is
    // the address calls actually reach.
    const WatchedModule modules[] = {
        {"kernelbase.dll", kernelbase_base},
        {"kernel32.dll", kernel32_base},
        {"user32.dll", user32_base},
    };
    constexpr const char* kWatchedApis[] = {
        "LoadLibraryA",
        "LoadLibraryW",
        "GetProcAddress",
        "VirtualAlloc",
        "HeapAlloc",
        "LocalAlloc",
        "VirtualProtect",
        "VirtualFree",
        "GetVersion",
        "GetVersionExA",
        "CreateFileA",
        "FreeLibrary",
        "DeviceIoControl",
        "ReadFile",
        "WriteFile",
        "CloseHandle",
        "SetCurrentDirectoryA",
        "RegisterClassA",
        "CreateWindowExA",
        "ShowCursor",
        "ShowWindow",
        "UpdateWindow",
        "SendMessageA",
        "PostQuitMessage",
        "EnumDisplaySettingsA",
        "ChangeDisplaySettingsExA",
        "GetAsyncKeyState",
    };
    bool any_resolved = false;
    for (const char* api : kWatchedApis)
    {
        if (allocation_only && std::strcmp(api, "VirtualAlloc") != 0 &&
            std::strcmp(api, "HeapAlloc") != 0 &&
            std::strcmp(api, "LocalAlloc") != 0)
        {
            continue;
        }
        bool recorded = false;
        for (const WatchedModule& module : modules)
        {
            if (module.base == 0 || (allocation_only && recorded))
            {
                continue;
            }
            re2dj::tools::windows_x86_launcher_probe::RemoteExportResolution resolution;
            if (!re2dj::tools::windows_x86_launcher_probe::ResolveRemotePe32Export(
                    process, module.base, api, &resolution, error))
            {
                RecordDiagnostic("{\"event\":\"api_watch\",\"api\":\"%s\",\"module\":\"%s\",\"status\":\"missing\"}",
                                 api,
                                 module.name);
                error->clear();
                continue;
            }
            if (resolution.forwarded)
            {
                RecordDiagnostic("{\"event\":\"api_watch\",\"api\":\"%s\",\"module\":\"%s\",\"status\":\"forwarded\"}",
                                 api,
                                 module.name);
                continue;
            }
            if (watches->count(resolution.address) != 0)
            {
                RecordDiagnostic("{\"event\":\"api_watch\",\"api\":\"%s\",\"module\":\"%s\",\"address\":\"0x%08x\",\"status\":\"duplicate_address\"}",
                                 api,
                                 module.name,
                                 static_cast<unsigned>(resolution.address));
                recorded = true;
                continue;
            }
            ApiWatchPoint watch;
            watch.name = api;
            watch.string_arg_index = ApiStringArgumentIndex(api);
            watch.argument_count = std::strcmp(api, "CreateWindowExA") == 0 ? 12
                                   : std::strcmp(api, "DeviceIoControl") == 0 ? 8
                                                                             : 4;
            SIZE_T copied = 0;
            if (ReadProcessMemory(process,
                                  reinterpret_cast<const void*>(resolution.address),
                                  &watch.original_byte,
                                  sizeof(watch.original_byte),
                                  &copied) == FALSE ||
                copied != sizeof(watch.original_byte))
            {
                *error = "cannot read watched API original byte";
                return false;
            }
            const std::uint8_t breakpoint = 0xcc;
            SIZE_T written = 0;
            if (WriteProcessMemory(process,
                                   reinterpret_cast<void*>(resolution.address),
                                   &breakpoint,
                                   sizeof(breakpoint),
                                   &written) == FALSE ||
                written != sizeof(breakpoint) ||
                FlushInstructionCache(process,
                                      reinterpret_cast<const void*>(resolution.address),
                                      sizeof(breakpoint)) == FALSE)
            {
                *error = "cannot set watched API breakpoint";
                return false;
            }
            RecordDiagnostic("{\"event\":\"api_watch\",\"api\":\"%s\",\"module\":\"%s\",\"address\":\"0x%08x\",\"status\":\"armed\"}",
                             api,
                             module.name,
                             static_cast<unsigned>(resolution.address));
            watches->emplace(resolution.address, std::move(watch));
            any_resolved = true;
            recorded = true;
        }
        if (!recorded)
        {
            RecordDiagnostic("{\"event\":\"api_watch\",\"api\":\"%s\",\"status\":\"unresolved\"}", api);
        }
    }
    if (!any_resolved)
    {
        *error = "no watched API could be resolved in child system modules";
        return false;
    }
    return true;
}

bool InstallRuntimeApiTraceBreakpoint(HANDLE process,
                                      std::uintptr_t address,
                                      const char* name,
                                      std::size_t argument_count,
                                      ApiWatchMap* watches,
                                      std::string* error)
{
    if (address == 0 || watches == nullptr || watches->count(address) != 0)
    {
        *error = "invalid or duplicate runtime API watch address";
        return false;
    }
    ApiWatchPoint watch;
    watch.name = name;
    watch.string_arg_index = ApiStringArgumentIndex(name);
    watch.argument_count = argument_count;
    SIZE_T copied = 0;
    if (ReadProcessMemory(process,
                          reinterpret_cast<const void*>(address),
                          &watch.original_byte,
                          sizeof(watch.original_byte),
                          &copied) == FALSE ||
        copied != sizeof(watch.original_byte))
    {
        *error = "cannot read runtime API watch original byte";
        return false;
    }
    const std::uint8_t breakpoint = 0xcc;
    SIZE_T written = 0;
    if (WriteProcessMemory(process,
                           reinterpret_cast<void*>(address),
                           &breakpoint,
                           sizeof(breakpoint),
                           &written) == FALSE ||
        written != sizeof(breakpoint) ||
        FlushInstructionCache(process,
                              reinterpret_cast<const void*>(address),
                              sizeof(breakpoint)) == FALSE)
    {
        *error = "cannot set runtime API watch breakpoint";
        return false;
    }
    RecordDiagnostic("{\"event\":\"api_watch\",\"api\":\"%s\",\"module\":\"injected_runtime\",\"address\":\"0x%08x\",\"status\":\"armed\"}",
                     name,
                     static_cast<unsigned>(address));
    watches->emplace(address, std::move(watch));
    return true;
}

struct LegacyIoTrapPolicy
{
    std::uintptr_t in_byte_rva = 0;
    std::uintptr_t out_byte_rva = 0;
};

enum class IoPortTrapResult
{
    kNotHandled,
    kHandled,
    kError,
};

IoPortTrapResult HandleLegacyIoPortTrap(
    HANDLE process,
    DWORD thread_id,
    const EXCEPTION_RECORD& exception,
    std::uintptr_t image_base,
    const LegacyIoTrapPolicy& policy,
    re2dj::input::LegacyIoPortBus* bus,
    bool trace,
    std::string* error)
{
    if (bus == nullptr || exception.ExceptionCode != EXCEPTION_PRIV_INSTRUCTION)
    {
        return IoPortTrapResult::kNotHandled;
    }

    const std::uintptr_t address =
        reinterpret_cast<std::uintptr_t>(exception.ExceptionAddress);
    const bool is_read = policy.in_byte_rva != 0 &&
                         address == image_base + policy.in_byte_rva;
    const bool is_write = policy.out_byte_rva != 0 &&
                          address == image_base + policy.out_byte_rva;
    if (!is_read && !is_write)
    {
        return IoPortTrapResult::kNotHandled;
    }

    std::uint8_t opcode = 0;
    SIZE_T copied = 0;
    const std::uint8_t expected_opcode = is_read ? 0xec : 0xee;
    if (ReadProcessMemory(process,
                          exception.ExceptionAddress,
                          &opcode,
                          sizeof(opcode),
                          &copied) == FALSE ||
        copied != sizeof(opcode) || opcode != expected_opcode)
    {
        *error = "legacy I/O port trap opcode does not match target profile";
        return IoPortTrapResult::kError;
    }

    HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT,
                               FALSE,
                               thread_id);
    CONTEXT context = {};
    context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    if (thread == nullptr || GetThreadContext(thread, &context) == FALSE)
    {
        if (thread != nullptr)
        {
            CloseHandle(thread);
        }
        *error = "cannot capture legacy I/O port trap context";
        return IoPortTrapResult::kError;
    }

    const std::uint16_t port = static_cast<std::uint16_t>(context.Edx);
    const std::uint32_t eax_before = context.Eax;
    const std::uint32_t eip_before = context.Eip;
    std::uint32_t return_address = 0;
    const bool return_address_readable =
        ReadRemoteU32(process, context.Esp, &return_address);
    const RemoteBufferSnapshot return_code =
        return_address_readable
            ? ReadRemoteBufferSnapshot(process, return_address, 24)
            : RemoteBufferSnapshot{};
    std::uint8_t value = static_cast<std::uint8_t>(context.Eax);
    const bool accepted = is_read ? bus->ReadByte(port, &value)
                                  : bus->WriteByte(port, value);
    if (!accepted)
    {
        CloseHandle(thread);
        return IoPortTrapResult::kNotHandled;
    }

    if (is_read)
    {
        context.Eax = (context.Eax & 0xffffff00u) | value;
    }
    context.Eip += 1;
    if (SetThreadContext(thread, &context) == FALSE)
    {
        CloseHandle(thread);
        *error = "cannot advance legacy I/O port trap context";
        return IoPortTrapResult::kError;
    }
    CloseHandle(thread);

    if (trace)
    {
        RecordDiagnostic("{\"event\":\"io_port_%s\",\"thread\":%u,\"address\":\"0x%08x\",\"port\":\"0x%03x\",\"width\":1,\"value\":\"0x%02x\",\"eax_before\":\"0x%08x\",\"eax_after\":\"0x%08x\",\"eip_after\":\"0x%08x\",\"return_address\":\"0x%08x\",\"return_readable\":%s,\"return_bytes\":\"%s\"}",
                         is_read ? "read" : "write",
                         static_cast<unsigned>(thread_id),
                         static_cast<unsigned>(address),
                         static_cast<unsigned>(port),
                         static_cast<unsigned>(value),
                         static_cast<unsigned>(eax_before),
                         static_cast<unsigned>(context.Eax),
                         static_cast<unsigned>(eip_before + 1),
                         static_cast<unsigned>(return_address),
                         return_address_readable ? "true" : "false",
                         return_code.readable ? return_code.bytes.c_str() : "");
    }
    return IoPortTrapResult::kHandled;
}

constexpr std::array<std::uint32_t, 3> kEz2dj4thSlotWriterRvas = {
    0x006ef5f0,
    0x006efe62,
    0x006f061a,
};
constexpr std::uint32_t kEz2dj4thPointerSlotRva = 0x006f0cf4;
constexpr std::uint32_t kSlotWriterHitLimit = 64;

struct SlotWriterTraceState
{
    std::uint32_t hit_count = 0;
    std::uint32_t recorded_count = 0;
    bool capped = false;
};

bool SetSlotWriterBreakpoints(HANDLE thread,
                              std::uintptr_t image_base,
                              std::string* error)
{
    CONTEXT context = {};
    context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (GetThreadContext(thread, &context) == FALSE)
    {
        *error = "cannot read slot-writer thread debug registers";
        return false;
    }
    context.Dr0 = image_base + kEz2dj4thSlotWriterRvas[0];
    context.Dr1 = image_base + kEz2dj4thSlotWriterRvas[1];
    context.Dr2 = image_base + kEz2dj4thSlotWriterRvas[2];
    context.Dr6 = 0;
    for (unsigned index = 0; index < kEz2dj4thSlotWriterRvas.size(); ++index)
    {
        context.Dr7 &= ~(static_cast<DWORD>(3) << (index * 2));
        context.Dr7 &= ~(static_cast<DWORD>(0xf) << (16 + index * 4));
        context.Dr7 |= static_cast<DWORD>(1) << (index * 2);
    }
    if (SetThreadContext(thread, &context) == FALSE)
    {
        *error = "cannot set slot-writer execution breakpoints";
        return false;
    }
    CONTEXT verified = {};
    verified.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (GetThreadContext(thread, &verified) == FALSE ||
        verified.Dr0 != context.Dr0 || verified.Dr1 != context.Dr1 ||
        verified.Dr2 != context.Dr2 ||
        (verified.Dr7 & static_cast<DWORD>(0x15)) != static_cast<DWORD>(0x15))
    {
        *error = "slot-writer execution breakpoints were not retained";
        return false;
    }
    return true;
}

bool HandleSlotWriterBreakpoint(HANDLE process,
                                DWORD thread_id,
                                std::uintptr_t image_base,
                                SlotWriterTraceState* state,
                                bool* handled,
                                std::string* error)
{
    *handled = false;
    HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT,
                               FALSE,
                               thread_id);
    CONTEXT context = {};
    context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_DEBUG_REGISTERS;
    if (thread == nullptr || GetThreadContext(thread, &context) == FALSE)
    {
        if (thread != nullptr)
        {
            CloseHandle(thread);
        }
        *error = "cannot capture slot-writer breakpoint context";
        return false;
    }
    std::size_t writer_index = kEz2dj4thSlotWriterRvas.size();
    for (std::size_t index = 0; index < kEz2dj4thSlotWriterRvas.size(); ++index)
    {
        if (context.Eip == image_base + kEz2dj4thSlotWriterRvas[index])
        {
            writer_index = index;
            break;
        }
    }
    if (writer_index == kEz2dj4thSlotWriterRvas.size())
    {
        CloseHandle(thread);
        return true;
    }

    ++state->hit_count;
    if (state->recorded_count < kSlotWriterHitLimit)
    {
        const std::uintptr_t slot_address = image_base + kEz2dj4thPointerSlotRva;
        std::uint32_t slot_value = 0;
        SIZE_T slot_copied = 0;
        const bool slot_readable =
            ReadProcessMemory(process,
                              reinterpret_cast<const void*>(slot_address),
                              &slot_value,
                              sizeof(slot_value),
                              &slot_copied) != FALSE &&
            slot_copied == sizeof(slot_value);
        std::uint8_t bytes[8] = {};
        SIZE_T byte_count = 0;
        const bool bytes_readable =
            ReadProcessMemory(process,
                              reinterpret_cast<const void*>(context.Eip),
                              bytes,
                              sizeof(bytes),
                              &byte_count) != FALSE &&
            byte_count != 0;
        char byte_text[sizeof(bytes) * 2 + 1] = {};
        if (bytes_readable)
        {
            for (SIZE_T index = 0; index < byte_count; ++index)
            {
                std::snprintf(byte_text + index * 2, 3, "%02x", bytes[index]);
            }
        }
        RecordDiagnostic("{\"event\":\"slot_writer_hit\",\"sequence\":%u,\"thread\":%u,\"writer_index\":%u,\"address\":\"0x%08x\",\"rva\":\"0x%08x\",\"eax\":\"0x%08x\",\"slot\":\"0x%08x\",\"slot_readable\":%s,\"slot_before\":\"0x%08x\",\"dr6\":\"0x%08x\",\"bytes_readable\":%s,\"bytes\":\"%s\"}",
                         state->hit_count,
                         static_cast<unsigned>(thread_id),
                         static_cast<unsigned>(writer_index),
                         static_cast<unsigned>(context.Eip),
                         kEz2dj4thSlotWriterRvas[writer_index],
                         static_cast<unsigned>(context.Eax),
                         static_cast<unsigned>(slot_address),
                         slot_readable ? "true" : "false",
                         slot_value,
                         static_cast<unsigned>(context.Dr6),
                         bytes_readable ? "true" : "false",
                         byte_text);
        ++state->recorded_count;
    }
    else
    {
        state->capped = true;
    }

    context.Dr6 = 0;
    context.EFlags |= 0x10000;
    if (SetThreadContext(thread, &context) == FALSE)
    {
        CloseHandle(thread);
        *error = "cannot resume slot-writer breakpoint instruction";
        return false;
    }
    CloseHandle(thread);
    *handled = true;
    return true;
}

constexpr std::uint32_t kEz2dj4thNullContextFieldRva = 0x006cd824;
constexpr std::uint32_t kEz2dj4thNullContextObjectRva = 0x006cd708;
// Global pointer that callers read the singleton receiver from, confirmed by
// the Task 157 caller window `mov ecx, [0x00ac29b4]`.
constexpr std::uint32_t kEz2dj4thNullContextObjectGlobalRva = 0x006c29b4;
constexpr std::uint32_t kEz2dj4thNullContextFieldAccessRva = 0x001a699;
constexpr std::uint32_t kEz2dj4thNullContextObjectSourcePrologueRva = 0x001a649;
constexpr std::uint32_t kEz2dj4thNullContextObjectSourceBoundaryRva = 0x001a64c;
constexpr std::uint32_t kEz2dj4thTextRva = 0x00001000;
constexpr std::uint32_t kEz2dj4thTextVirtualSize = 0x000db022;
constexpr std::uint32_t kNullContextFieldWriterHitLimit = 64;

// Finds the instructions that reference one 32-bit constant in the guest's
// decrypted .text.
//
// The question this answers is which code writes a field that a fault shows to
// be empty. A write watchpoint cannot answer it: in the failing run nobody
// writes the field, so the watchpoint never fires. Locating the writer has to
// come first, and because the executable is packed the search runs against the
// live process rather than the file.
//
// This is a byte search, not a disassembler. A constant can appear at a
// position that is not an instruction boundary, so every result carries the
// surrounding bytes for a person to confirm.

// Classifies what an instruction does to the operand the constant addresses.
// Anything not recognised stays "other" rather than being guessed at: a
// misfiled write would send the investigation to the wrong code.
const char* ClassifyFieldReferenceOpcode(std::uint8_t opcode, std::uint8_t modrm)
{
    switch (opcode)
    {
        case 0x8b:  // mov r32, r/m32
        case 0x3b:  // cmp r32, r/m32
        case 0x03:  // add r32, r/m32
        case 0x2b:  // sub r32, r/m32
        case 0x33:  // xor r32, r/m32
        case 0x0b:  // or  r32, r/m32
        case 0x23:  // and r32, r/m32
        case 0x39:  // cmp r/m32, r32
            return "read";
        case 0x89:  // mov r/m32, r32
        case 0xc7:  // mov r/m32, imm32
            return "write";
        case 0x01:  // add r/m32, r32
        case 0x29:  // sub r/m32, r32
        case 0x31:  // xor r/m32, r32
        case 0x09:  // or  r/m32, r32
        case 0x21:  // and r/m32, r32
            return "modify";
        case 0x83:  // group 1 r/m32, imm8
        case 0x81:  // group 1 r/m32, imm32
        case 0x80:  // group 1 r/m8, imm8
        {
            // Group 1 packs eight operations into one opcode and the reg field
            // selects which. Seven of them write the operand; the eighth, cmp,
            // only reads it. Calling a cmp a write would point the search at a
            // null check instead of at an assignment.
            const std::uint8_t reg = static_cast<std::uint8_t>((modrm >> 3) & 0x07u);
            return reg == 0x07u ? "read" : "modify";
        }
        case 0x8d:  // lea r32, m
            return "address";
        case 0xff:
        {
            // The reg field selects the operation: 0 inc, 1 dec, 2 and 3 call,
            // 4 and 5 jmp, 6 push.
            const std::uint8_t reg = static_cast<std::uint8_t>((modrm >> 3) & 0x07u);
            if (reg == 0 || reg == 1)
            {
                return "modify";
            }
            if (reg == 6)
            {
                return "read";
            }
            return "indirect";
        }
        default:
            return "other";
    }
}

// A hardware write watchpoint on one guest address.
//
// A byte search of the guest's code can only find writes it can recognise as
// instructions. This traps the write itself, so it sees a bulk copy, a computed
// address, and code in any section alike - which is what separates "the field is
// installed somewhere the search cannot see" from "the install never runs".
//
// Debug register 1 carries the watch. All four registers are claimed by one
// diagnostic or another, so the modes that also use Dr1 are rejected on the
// command line rather than silently overwriting each other.
constexpr DWORD kFieldWriteWatchStatus = static_cast<DWORD>(1u << 1);
constexpr DWORD kFieldWriteWatchEnable = static_cast<DWORD>(1u << 2);
// R/W1 = 01 (write) at bits 20-21, LEN1 = 11 (four bytes) at bits 22-23.
constexpr DWORD kFieldWriteWatchControl =
    static_cast<DWORD>(0x1u << 20) | static_cast<DWORD>(0x3u << 22);
constexpr DWORD kFieldWriteWatchMask =
    kFieldWriteWatchEnable | static_cast<DWORD>(0xfu << 20);
constexpr std::uint32_t kFieldWriteWatchHitLimit = 64;

struct FieldWriteWatchState
{
    std::uint32_t hit_count = 0;
    std::uint32_t recorded_count = 0;
};

bool SetFieldWriteWatch(HANDLE thread, std::uintptr_t address, std::string* error)
{
    CONTEXT context = {};
    context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (GetThreadContext(thread, &context) == FALSE)
    {
        *error = "cannot read debug registers for the field write watch";
        return false;
    }
    context.Dr1 = static_cast<DWORD>(address);
    context.Dr6 = 0;
    context.Dr7 &= ~kFieldWriteWatchMask;
    context.Dr7 |= kFieldWriteWatchEnable | kFieldWriteWatchControl;
    if (SetThreadContext(thread, &context) == FALSE)
    {
        *error = "cannot set the field write watch";
        return false;
    }
    CONTEXT verified = {};
    verified.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (GetThreadContext(thread, &verified) == FALSE ||
        verified.Dr1 != context.Dr1 ||
        (verified.Dr7 & kFieldWriteWatchMask) !=
            (kFieldWriteWatchEnable | kFieldWriteWatchControl))
    {
        *error = "the field write watch was not retained";
        return false;
    }
    return true;
}

bool HandleFieldWriteWatchHit(HANDLE process,
                              DWORD thread_id,
                              std::uintptr_t address,
                              std::uintptr_t image_base,
                              const re2dj::exe::PeImageInfo* image_info,
                              FieldWriteWatchState* state,
                              bool* handled,
                              std::string* error)
{
    *handled = false;
    HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, thread_id);
    if (thread == nullptr)
    {
        *error = "cannot open the thread that hit the field write watch";
        return false;
    }
    CONTEXT context = {};
    context.ContextFlags = CONTEXT_DEBUG_REGISTERS | CONTEXT_CONTROL | CONTEXT_INTEGER;
    if (GetThreadContext(thread, &context) == FALSE)
    {
        CloseHandle(thread);
        *error = "cannot read the context of a field write watch hit";
        return false;
    }
    if ((context.Dr6 & kFieldWriteWatchStatus) == 0)
    {
        CloseHandle(thread);
        return true;
    }

    ++state->hit_count;
    // A data breakpoint traps after the store completes, so the value at the
    // address is the value that was just written and EIP is the instruction
    // after the one that wrote it.
    std::uint32_t written_value = 0;
    const bool value_readable = ReadRemoteU32(process, address, &written_value);
    std::uint32_t return_address = 0;
    const bool return_readable =
        context.Ebp != 0 && ReadRemoteU32(process, context.Ebp + 4, &return_address);
    const bool eip_in_image = context.Eip >= image_base;
    const std::uint32_t eip_rva =
        eip_in_image ? static_cast<std::uint32_t>(context.Eip - image_base) : 0;
    const bool return_in_image = return_readable && return_address >= image_base;
    const std::uint32_t return_rva =
        return_in_image ? static_cast<std::uint32_t>(return_address - image_base) : 0;
    // Sixteen bytes before EIP so the storing instruction can be decoded, and a
    // few after for context.
    const std::uintptr_t window_base = context.Eip >= 16 ? context.Eip - 16 : image_base;
    const RemoteBufferSnapshot window = ReadRemoteBufferSnapshot(process, window_base, 24);

    if (state->recorded_count < kFieldWriteWatchHitLimit)
    {
        RecordDiagnostic(
            "{\"event\":\"field_write_watch_hit\",\"sequence\":%u,\"thread\":%u,\"address\":\"0x%08x\",\"value\":\"0x%08x\",\"value_readable\":%s,\"eip\":\"0x%08x\",\"eip_in_image\":%s,\"eip_rva\":\"0x%08x\",\"section\":\"%s\",\"eax\":\"0x%08x\",\"ecx\":\"0x%08x\",\"edx\":\"0x%08x\",\"ebx\":\"0x%08x\",\"esi\":\"0x%08x\",\"edi\":\"0x%08x\",\"ebp\":\"0x%08x\",\"esp\":\"0x%08x\",\"return\":\"0x%08x\",\"return_readable\":%s,\"return_rva\":\"0x%08x\",\"window_base\":\"0x%08x\",\"window_readable\":%s,\"bytes\":\"%s\"}",
            state->hit_count,
            static_cast<unsigned>(thread_id),
            static_cast<unsigned>(address),
            static_cast<unsigned>(written_value),
            value_readable ? "true" : "false",
            static_cast<unsigned>(context.Eip),
            eip_in_image ? "true" : "false",
            eip_rva,
            image_info == nullptr || !eip_in_image
                ? "<unknown>"
                : FindSectionNameForRva(*image_info, eip_rva),
            static_cast<unsigned>(context.Eax),
            static_cast<unsigned>(context.Ecx),
            static_cast<unsigned>(context.Edx),
            static_cast<unsigned>(context.Ebx),
            static_cast<unsigned>(context.Esi),
            static_cast<unsigned>(context.Edi),
            static_cast<unsigned>(context.Ebp),
            static_cast<unsigned>(context.Esp),
            static_cast<unsigned>(return_address),
            return_readable ? "true" : "false",
            return_rva,
            static_cast<unsigned>(window_base),
            window.readable ? "true" : "false",
            window.bytes.c_str());
        ++state->recorded_count;
    }

    context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    context.Dr6 = 0;
    if (SetThreadContext(thread, &context) == FALSE)
    {
        CloseHandle(thread);
        *error = "cannot clear the field write watch status";
        return false;
    }
    CloseHandle(thread);
    *handled = true;
    return true;
}

// Reads a byte window around one guest address.
//
// The 4th's .text is ciphertext on disk and is decrypted only in the running
// process, so its code cannot be read from the file at all. Every address this
// investigation needs - a constructor's caller, the frames above a fault - is
// known only as a return address, which points just past the call that produced
// it. The window is therefore centred on the given address rather than starting
// at it, so the call and the branch before it come into view.
struct GuestCodeWindowRequest
{
    std::uintptr_t address = 0;
    std::uint32_t length = 0;
};

constexpr std::uint32_t kGuestCodeWindowDefaultLength = 128;
constexpr std::uint32_t kGuestCodeWindowMaxLength = 512;

void CaptureGuestCodeWindow(HANDLE process,
                            std::uintptr_t image_base,
                            const re2dj::exe::PeImageInfo* image_info,
                            const GuestCodeWindowRequest& request)
{
    const std::uint32_t length = request.length == 0 ? kGuestCodeWindowDefaultLength
                                                     : request.length;
    const std::uintptr_t lead = length / 2;
    const std::uintptr_t window_base =
        request.address > lead ? request.address - lead : request.address;
    std::vector<std::uint8_t> bytes(length);
    SIZE_T copied = 0;
    const bool readable = ReadProcessMemory(process,
                                            reinterpret_cast<const void*>(window_base),
                                            bytes.data(),
                                            bytes.size(),
                                            &copied) != FALSE &&
                          copied != 0;
    const bool in_image = request.address >= image_base;
    const std::uint32_t rva =
        in_image ? static_cast<std::uint32_t>(request.address - image_base) : 0;
    std::string text;
    if (readable)
    {
        text.reserve(static_cast<std::size_t>(copied) * 2);
        char pair[3] = {};
        for (SIZE_T index = 0; index < copied; ++index)
        {
            std::snprintf(pair, sizeof(pair), "%02x", bytes[index]);
            text.append(pair);
        }
    }
    RecordDiagnostic(
        "{\"event\":\"guest_code_window\",\"address\":\"0x%08x\",\"rva\":\"0x%08x\",\"in_image\":%s,\"section\":\"%s\",\"window_base\":\"0x%08x\",\"window_rva\":\"0x%08x\",\"requested\":%u,\"copied\":%u,\"readable\":%s,\"bytes\":\"%s\"}",
        static_cast<unsigned>(request.address),
        rva,
        in_image ? "true" : "false",
        image_info == nullptr || !in_image ? "<unknown>"
                                           : FindSectionNameForRva(*image_info, rva),
        static_cast<unsigned>(window_base),
        static_cast<unsigned>(window_base >= image_base ? window_base - image_base : 0),
        length,
        static_cast<unsigned>(copied),
        readable ? "true" : "false",
        text.c_str());
}

void ScanGuestFieldReferences(HANDLE process,
                              std::uintptr_t image_base,
                              const re2dj::exe::PeImageInfo* image_info,
                              std::uint32_t constant)
{
    constexpr std::uint32_t kMatchLimit = 96;
    const std::uintptr_t text_base = image_base + kEz2dj4thTextRva;
    std::vector<std::uint8_t> bytes(kEz2dj4thTextVirtualSize);
    SIZE_T copied = 0;
    const bool readable =
        ReadProcessMemory(process,
                          reinterpret_cast<const void*>(text_base),
                          bytes.data(),
                          bytes.size(),
                          &copied) != FALSE &&
        copied == bytes.size();
    if (!readable)
    {
        RecordDiagnostic(
            "{\"event\":\"field_reference_scan\",\"constant\":\"0x%08x\",\"readable\":false,\"text_base\":\"0x%08x\",\"bytes_copied\":%u,\"matches\":0,\"writes\":0,\"recorded\":0,\"capped\":false}",
            constant,
            static_cast<unsigned>(text_base),
            static_cast<unsigned>(copied));
        return;
    }

    const auto read_u32 = [&bytes](std::size_t at) {
        return static_cast<std::uint32_t>(bytes[at]) |
               (static_cast<std::uint32_t>(bytes[at + 1]) << 8) |
               (static_cast<std::uint32_t>(bytes[at + 2]) << 16) |
               (static_cast<std::uint32_t>(bytes[at + 3]) << 24);
    };

    std::uint32_t match_count = 0;
    std::uint32_t write_count = 0;
    std::uint32_t recorded = 0;
    bool capped = false;
    for (std::size_t index = 0; index + 7 <= bytes.size(); ++index)
    {
        const std::uint8_t opcode = bytes[index];
        const std::uint8_t modrm = bytes[index + 1];
        const std::uint8_t mod = static_cast<std::uint8_t>(modrm & 0xc0u);
        const std::uint8_t rm = static_cast<std::uint8_t>(modrm & 0x07u);

        std::size_t displacement_at = 0;
        const char* form = nullptr;
        // Forms that carry the constant as an immediate rather than as a
        // memory displacement. Code that computes a field address before
        // storing through it leaves the offset here instead.
        if (opcode >= 0xb8u && opcode <= 0xbfu)
        {
            // mov r32, imm32
            if (index + 5 <= bytes.size() && read_u32(index + 1) == constant)
            {
                displacement_at = index + 1;
                form = "imm32-mov";
            }
            else
            {
                continue;
            }
        }
        else if (opcode == 0x68u)
        {
            // push imm32
            if (index + 5 <= bytes.size() && read_u32(index + 1) == constant)
            {
                displacement_at = index + 1;
                form = "imm32-push";
            }
            else
            {
                continue;
            }
        }
        else if (opcode == 0x81u && mod == 0xc0u)
        {
            // group 1 r32, imm32 - register destination, so the constant is
            // being folded into an address rather than addressing memory.
            if (index + 6 <= bytes.size() && read_u32(index + 2) == constant)
            {
                displacement_at = index + 2;
                form = "imm32-alu";
            }
            else
            {
                continue;
            }
        }
        else if (mod == 0x80u)
        {
            // A base register with a 32-bit displacement. rm == 100 means a SIB
            // byte sits between the ModRM and the displacement; missing that
            // case would drop every indexed access to the field.
            displacement_at = index + (rm == 0x04u ? 3u : 2u);
            form = rm == 0x04u ? "base+sib+disp32" : "base+disp32";
        }
        else if (mod == 0x00u && rm == 0x05u)
        {
            displacement_at = index + 2;
            form = "absolute";
        }
        else
        {
            continue;
        }
        if (displacement_at + 4 > bytes.size() || read_u32(displacement_at) != constant)
        {
            continue;
        }

        const char* const kind = std::strncmp(form, "imm32", 5) == 0
                                     ? "offset-load"
                                     : ClassifyFieldReferenceOpcode(opcode, modrm);
        ++match_count;
        if (std::strcmp(kind, "write") == 0 || std::strcmp(kind, "modify") == 0)
        {
            ++write_count;
        }
        if (recorded >= kMatchLimit)
        {
            capped = true;
            continue;
        }

        // Eight bytes before and after, so the instruction and its neighbours
        // can be decoded by hand from the record alone.
        const std::size_t window_start = index >= 8 ? index - 8 : 0;
        const std::size_t window_end = (std::min)(bytes.size(), displacement_at + 12);
        char window_text[80] = {};
        std::size_t written = 0;
        for (std::size_t at = window_start;
             at < window_end && written + 2 < sizeof(window_text);
             ++at)
        {
            std::snprintf(window_text + written, 3, "%02x", bytes[at]);
            written += 2;
        }
        const std::uint32_t rva = static_cast<std::uint32_t>(kEz2dj4thTextRva + index);
        RecordDiagnostic(
            "{\"event\":\"field_reference\",\"constant\":\"0x%08x\",\"rva\":\"0x%08x\",\"address\":\"0x%08x\",\"kind\":\"%s\",\"form\":\"%s\",\"opcode\":\"0x%02x\",\"modrm\":\"0x%02x\",\"section\":\"%s\",\"window_rva\":\"0x%08x\",\"bytes\":\"%s\"}",
            constant,
            rva,
            static_cast<unsigned>(text_base + index),
            kind,
            form,
            static_cast<unsigned>(opcode),
            static_cast<unsigned>(modrm),
            image_info == nullptr ? "<unknown>"
                                 : FindSectionNameForRva(*image_info, rva),
            static_cast<unsigned>(kEz2dj4thTextRva + window_start),
            window_text);
        ++recorded;
    }

    RecordDiagnostic(
        "{\"event\":\"field_reference_scan\",\"constant\":\"0x%08x\",\"readable\":true,\"text_base\":\"0x%08x\",\"bytes_copied\":%u,\"matches\":%u,\"writes\":%u,\"recorded\":%u,\"capped\":%s}",
        constant,
        static_cast<unsigned>(text_base),
        static_cast<unsigned>(copied),
        match_count,
        write_count,
        recorded,
        capped ? "true" : "false");
}

constexpr std::uint32_t kNullContextFieldReferenceLimit = 128;
constexpr DWORD kNullContextFieldBreakpointStatus = static_cast<DWORD>(1u << 3);
constexpr DWORD kNullContextFieldBreakpointEnable = static_cast<DWORD>(1u << 6);
constexpr DWORD kNullContextFieldWriteBreakpointControl =
    static_cast<DWORD>((1u << 28) | (3u << 30));
constexpr DWORD kNullContextFieldAccessBreakpointControl =
    static_cast<DWORD>((3u << 28) | (3u << 30));
constexpr DWORD kNullContextFieldBreakpointMask =
    static_cast<DWORD>((0x3u << 6) | (0xfu << 28));
constexpr DWORD kNullContextFieldReferenceExecutionStatusMask =
    static_cast<DWORD>(0x0fu);
constexpr DWORD kNullContextFieldReferenceExecutionBreakpointMask =
    static_cast<DWORD>(0x000000ffu | 0xffff0000u);
constexpr DWORD kNullContextFieldReferenceExecutionBreakpointEnable =
    static_cast<DWORD>((1u << 0) | (1u << 2) | (1u << 4) | (1u << 6));
constexpr std::uint32_t kNullContextFieldReferenceExecutionHitLimit = 64;

enum class NullContextFieldReferenceRegister
{
    kEax,
    kEcx,
    kEdx,
};

struct NullContextFieldReferenceExecutionCandidate
{
    std::uint32_t rva;
    NullContextFieldReferenceRegister receiver_register;
    const char* receiver_name;
    NullContextFieldReferenceRegister write_register;
    const char* write_source;
    bool immediate_write;
    std::uint32_t instruction_size;
};

constexpr std::array<NullContextFieldReferenceExecutionCandidate, 4>
    kNullContextFieldReferenceExecutionCandidates = {{
        {0x0000fdbd,
         NullContextFieldReferenceRegister::kEcx,
         "ecx",
         NullContextFieldReferenceRegister::kEax,
         "eax",
         false,
         6},
        {0x0000fde1,
         NullContextFieldReferenceRegister::kEcx,
         "ecx",
         NullContextFieldReferenceRegister::kEax,
         "immediate",
         true,
         10},
        {0x0001825f,
         NullContextFieldReferenceRegister::kEax,
         "eax",
         NullContextFieldReferenceRegister::kEax,
         "immediate",
         true,
         10},
        {0x0001dbd3,
         NullContextFieldReferenceRegister::kEdx,
         "edx",
         NullContextFieldReferenceRegister::kEcx,
         "ecx",
         false,
         6},
    }};

// Function entries whose execution order answers why the singleton field stays
// zero: the virtual method that reaches the initializer, the initializer that
// writes the field, the reader that faults on it, and the constructor that
// bounds the whole sequence. RVAs come from the Task 160 call chain.
struct NullContextEntryPoint
{
    std::uint32_t rva;
    const char* name;
};

constexpr std::array<NullContextEntryPoint, 4> kNullContextEntryPoints = {{
    // Task 169 showed guard 1's callee returns 0x81000004 because all four of
    // its candidate slots stay zero. These anchors answer why: the loop head
    // gives the iteration count, the two helper call sites give the compared
    // pointer in stack_arg0 and the match result in EAX on the way back, and
    // the decision start bounds the loop.
    {0x00010174, "guard1_loop_head"},
    {0x000101cf, "guard1_helper_call_0"},
    {0x00010217, "guard1_helper_call_1"},
    {0x0001024c, "guard1_decision_start"},
}};

constexpr std::uint32_t kNullContextEntryHitLimit = 8;

struct NullContextEntryTraceState
{
    std::array<std::uint32_t, kNullContextEntryPoints.size()> hit_counts = {};
    std::array<std::uint32_t, kNullContextEntryPoints.size()> recorded_counts = {};
    std::uint32_t hit_count = 0;
    std::uint32_t recorded_count = 0;
    std::uint32_t singleton_receiver_count = 0;
    bool capped = false;
    std::set<DWORD> pending_single_step_threads;
};

struct NullContextFieldWriterTraceState
{
    std::uint32_t hit_count = 0;
    std::uint32_t recorded_count = 0;
    bool capped = false;
    std::uint32_t last_field_value = 0;
    bool last_field_readable = false;
};

struct NullContextFieldReferenceExecutionTraceState
{
    std::uint32_t hit_count = 0;
    std::uint32_t recorded_count = 0;
    std::uint32_t target_match_count = 0;
    bool capped = false;
    std::set<DWORD> pending_single_step_threads;
};

std::uint32_t ReadNullContextFieldReferenceRegister(
    const CONTEXT& context,
    NullContextFieldReferenceRegister register_name)
{
    switch (register_name)
    {
    case NullContextFieldReferenceRegister::kEax:
        return context.Eax;
    case NullContextFieldReferenceRegister::kEcx:
        return context.Ecx;
    case NullContextFieldReferenceRegister::kEdx:
        return context.Edx;
    }
    return 0;
}

bool SetNullContextFieldReferenceExecutionBreakpoints(HANDLE thread,
                                                       std::uintptr_t image_base,
                                                       std::string* error)
{
    CONTEXT context = {};
    context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (GetThreadContext(thread, &context) == FALSE)
    {
        *error = "cannot read null-context field reference debug registers";
        return false;
    }
    context.Dr0 = static_cast<DWORD>(
        image_base + kNullContextFieldReferenceExecutionCandidates[0].rva);
    context.Dr1 = static_cast<DWORD>(
        image_base + kNullContextFieldReferenceExecutionCandidates[1].rva);
    context.Dr2 = static_cast<DWORD>(
        image_base + kNullContextFieldReferenceExecutionCandidates[2].rva);
    context.Dr3 = static_cast<DWORD>(
        image_base + kNullContextFieldReferenceExecutionCandidates[3].rva);
    context.Dr6 &= ~kNullContextFieldReferenceExecutionStatusMask;
    context.Dr7 &= ~kNullContextFieldReferenceExecutionBreakpointMask;
    context.Dr7 |= kNullContextFieldReferenceExecutionBreakpointEnable;
    if (SetThreadContext(thread, &context) == FALSE)
    {
        *error = "cannot set null-context field reference execution breakpoints";
        return false;
    }
    CONTEXT verified = {};
    verified.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (GetThreadContext(thread, &verified) == FALSE ||
        verified.Dr0 != context.Dr0 || verified.Dr1 != context.Dr1 ||
        verified.Dr2 != context.Dr2 || verified.Dr3 != context.Dr3 ||
        (verified.Dr7 & kNullContextFieldReferenceExecutionBreakpointMask) !=
            (kNullContextFieldReferenceExecutionBreakpointEnable))
    {
        *error = "null-context field reference execution breakpoints were not retained";
        return false;
    }
    return true;
}

bool HandleNullContextFieldReferenceExecutionBreakpoint(
    HANDLE process,
    DWORD thread_id,
    std::uintptr_t image_base,
    NullContextFieldReferenceExecutionTraceState* state,
    bool* handled,
    std::string* error)
{
    *handled = false;
    HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT,
                               FALSE,
                               thread_id);
    CONTEXT context = {};
    context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_DEBUG_REGISTERS;
    if (thread == nullptr || GetThreadContext(thread, &context) == FALSE)
    {
        if (thread != nullptr)
        {
            CloseHandle(thread);
        }
        *error = "cannot capture null-context field reference execution context";
        return false;
    }

    const DWORD execution_status = context.Dr6 &
                                    kNullContextFieldReferenceExecutionStatusMask;
    const auto pending_step = state->pending_single_step_threads.find(thread_id);
    if (pending_step != state->pending_single_step_threads.end() && execution_status == 0)
    {
        context.EFlags &= ~0x100u;
        if (SetThreadContext(thread, &context) == FALSE ||
            !SetNullContextFieldReferenceExecutionBreakpoints(thread, image_base, error))
        {
            CloseHandle(thread);
            if (error->empty())
            {
                *error = "cannot restore null-context field reference execution breakpoints";
            }
            return false;
        }
        state->pending_single_step_threads.erase(pending_step);
        RecordDiagnostic(
            "{\"event\":\"null_context_field_reference_execution_rearmed\",\"thread\":%u,\"eip_after\":\"0x%08x\"}",
            static_cast<unsigned>(thread_id),
            static_cast<unsigned>(context.Eip));
        CloseHandle(thread);
        *handled = true;
        return true;
    }
    if (execution_status == 0)
    {
        CloseHandle(thread);
        return true;
    }

    std::size_t candidate_index = kNullContextFieldReferenceExecutionCandidates.size();
    for (std::size_t index = 0;
         index < kNullContextFieldReferenceExecutionCandidates.size();
         ++index)
    {
        const auto& candidate = kNullContextFieldReferenceExecutionCandidates[index];
        const DWORD candidate_address = static_cast<DWORD>(image_base + candidate.rva);
        if ((execution_status & static_cast<DWORD>(1u << index)) != 0 &&
            context.Eip == candidate_address)
        {
            candidate_index = index;
            break;
        }
    }
    if (candidate_index == kNullContextFieldReferenceExecutionCandidates.size())
    {
        for (std::size_t index = 0;
             index < kNullContextFieldReferenceExecutionCandidates.size();
             ++index)
        {
            if ((execution_status & static_cast<DWORD>(1u << index)) != 0)
            {
                candidate_index = index;
                break;
            }
        }
    }
    if (candidate_index == kNullContextFieldReferenceExecutionCandidates.size())
    {
        context.Dr6 &= ~kNullContextFieldReferenceExecutionStatusMask;
        if (SetThreadContext(thread, &context) == FALSE)
        {
            CloseHandle(thread);
            *error = "cannot clear unknown null-context field reference status";
            return false;
        }
        CloseHandle(thread);
        *handled = true;
        return true;
    }

    const auto& candidate = kNullContextFieldReferenceExecutionCandidates[candidate_index];
    ++state->hit_count;
    const std::uint32_t receiver =
        ReadNullContextFieldReferenceRegister(context, candidate.receiver_register);
    const bool target_valid = receiver <= (std::numeric_limits<std::uint32_t>::max)() - 0x11c;
    const std::uint32_t target = target_valid ? receiver + 0x11c : 0;
    const std::uint32_t target_field = static_cast<std::uint32_t>(
        image_base + kEz2dj4thNullContextFieldRva);
    const bool target_matches = target_valid && target == target_field;
    if (target_matches)
    {
        ++state->target_match_count;
    }

    std::uint32_t write_value = 0;
    bool write_value_readable = true;
    std::uint32_t immediate_address = 0;
    if (candidate.immediate_write)
    {
        const bool immediate_address_valid =
            context.Eip <= (std::numeric_limits<DWORD>::max)() - 6;
        immediate_address = immediate_address_valid ? context.Eip + 6 : 0;
        write_value_readable = immediate_address_valid &&
                               ReadRemoteU32(process, immediate_address, &write_value);
    }
    else
    {
        write_value = ReadNullContextFieldReferenceRegister(
            context, candidate.write_register);
    }
    const RemoteBufferSnapshot instruction = ReadRemoteBufferSnapshot(
        process, context.Eip, candidate.instruction_size);
    const bool eip_matches_candidate =
        context.Eip == static_cast<DWORD>(image_base + candidate.rva);
    if (state->recorded_count < kNullContextFieldReferenceExecutionHitLimit)
    {
        RecordDiagnostic(
            "{\"event\":\"null_context_field_reference_execution_hit\",\"sequence\":%u,\"thread\":%u,\"candidate_index\":%u,\"candidate_rva\":\"0x%08x\",\"eip\":\"0x%08x\",\"eip_matches_candidate\":%s,\"receiver_register\":\"%s\",\"receiver\":\"0x%08x\",\"target\":\"0x%08x\",\"target_valid\":%s,\"target_field\":\"0x%08x\",\"target_matches\":%s,\"write_source\":\"%s\",\"write_value\":\"0x%08x\",\"write_value_readable\":%s,\"immediate_address\":\"0x%08x\",\"immediate_readable\":%s,\"dr6\":\"0x%08x\",\"code_readable\":%s,\"code\":\"%s\"}",
            state->hit_count,
            static_cast<unsigned>(thread_id),
            static_cast<unsigned>(candidate_index),
            candidate.rva,
            static_cast<unsigned>(context.Eip),
            eip_matches_candidate ? "true" : "false",
            candidate.receiver_name,
            receiver,
            target,
            target_valid ? "true" : "false",
            target_field,
            target_matches ? "true" : "false",
            candidate.write_source,
            write_value,
            write_value_readable ? "true" : "false",
            immediate_address,
            candidate.immediate_write && write_value_readable ? "true" : "false",
            static_cast<unsigned>(context.Dr6),
            instruction.readable ? "true" : "false",
            instruction.readable ? instruction.bytes.c_str() : "");
        ++state->recorded_count;
    }
    else
    {
        state->capped = true;
    }

    context.Dr7 &= ~kNullContextFieldReferenceExecutionBreakpointMask;
    context.Dr6 &= ~kNullContextFieldReferenceExecutionStatusMask;
    context.EFlags |= 0x100u;
    if (SetThreadContext(thread, &context) == FALSE)
    {
        CloseHandle(thread);
        *error = "cannot single-step null-context field reference instruction";
        return false;
    }
    state->pending_single_step_threads.insert(thread_id);
    CloseHandle(thread);
    *handled = true;
    return true;
}

bool SetNullContextFieldAccessBreakpoint(HANDLE thread,
                                         std::uintptr_t image_base,
                                         DWORD breakpoint_control,
                                         std::string* error)
{
    CONTEXT context = {};
    context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (GetThreadContext(thread, &context) == FALSE)
    {
        *error = "cannot read null-context field debug registers";
        return false;
    }
    context.Dr3 = image_base + kEz2dj4thNullContextFieldRva;
    context.Dr6 = 0;
    context.Dr7 &= ~kNullContextFieldBreakpointMask;
    context.Dr7 |= kNullContextFieldBreakpointEnable | breakpoint_control;
    if (SetThreadContext(thread, &context) == FALSE)
    {
        *error = "cannot set null-context field access breakpoint";
        return false;
    }
    CONTEXT verified = {};
    verified.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (GetThreadContext(thread, &verified) == FALSE ||
        verified.Dr3 != context.Dr3 ||
        (verified.Dr7 & kNullContextFieldBreakpointMask) !=
            (kNullContextFieldBreakpointEnable | breakpoint_control))
    {
        *error = "null-context field access breakpoint was not retained";
        return false;
    }
    return true;
}

void ScanRuntimeNullContextFieldReferences(HANDLE process,
                                           std::uintptr_t image_base)
{
    const std::uintptr_t text_base = image_base + kEz2dj4thTextRva;
    const std::size_t text_size = kEz2dj4thTextVirtualSize;
    std::vector<std::uint8_t> bytes(text_size);
    SIZE_T copied = 0;
    const bool readable =
        ReadProcessMemory(process,
                          reinterpret_cast<const void*>(text_base),
                          bytes.data(),
                          bytes.size(),
                          &copied) != FALSE &&
        copied == bytes.size();
    if (!readable)
    {
        RecordDiagnostic(
            "{\"event\":\"null_context_field_reference_scan\",\"readable\":false,\"text_base\":\"0x%08x\",\"text_rva\":\"0x%08x\",\"text_size\":\"0x%08x\",\"bytes_copied\":%u,\"candidates\":0,\"read_candidates\":0,\"write_candidates\":0,\"recorded\":0,\"capped\":false}",
            static_cast<unsigned>(text_base),
            kEz2dj4thTextRva,
            kEz2dj4thTextVirtualSize,
            static_cast<unsigned>(copied));
        return;
    }

    std::uint32_t candidate_count = 0;
    std::uint32_t read_candidate_count = 0;
    std::uint32_t write_candidate_count = 0;
    std::uint32_t recorded_count = 0;
    bool capped = false;
    for (std::size_t index = 0; index + 6 <= bytes.size(); ++index)
    {
        if ((bytes[index + 1] & 0xc0u) != 0x80u ||
            bytes[index + 2] != 0x1c || bytes[index + 3] != 0x01 ||
            bytes[index + 4] != 0x00 || bytes[index + 5] != 0x00)
        {
            continue;
        }

        const std::uint8_t opcode = bytes[index];
        const char* access = "other";
        if (opcode == 0x8b)
        {
            access = "read";
            ++read_candidate_count;
        }
        else if (opcode == 0x89 || opcode == 0xc7)
        {
            access = "write";
            ++write_candidate_count;
        }
        ++candidate_count;

        if (recorded_count < kNullContextFieldReferenceLimit)
        {
            char byte_text[13] = {};
            for (std::size_t byte_index = 0; byte_index < 6; ++byte_index)
            {
                std::snprintf(byte_text + byte_index * 2,
                              3,
                              "%02x",
                              bytes[index + byte_index]);
            }
            const std::uintptr_t address = text_base + index;
            RecordDiagnostic(
                "{\"event\":\"null_context_field_reference\",\"address\":\"0x%08x\",\"rva\":\"0x%08x\",\"access\":\"%s\",\"opcode\":\"0x%02x\",\"modrm\":\"0x%02x\",\"displacement\":\"0x0000011c\",\"bytes\":\"%s\"}",
                static_cast<unsigned>(address),
                static_cast<unsigned>(kEz2dj4thTextRva + index),
                access,
                static_cast<unsigned>(opcode),
                static_cast<unsigned>(bytes[index + 1]),
                byte_text);
            ++recorded_count;
        }
        else
        {
            capped = true;
        }
    }

    RecordDiagnostic(
        "{\"event\":\"null_context_field_reference_scan\",\"readable\":true,\"text_base\":\"0x%08x\",\"text_rva\":\"0x%08x\",\"text_size\":\"0x%08x\",\"bytes_copied\":%u,\"candidates\":%u,\"read_candidates\":%u,\"write_candidates\":%u,\"recorded\":%u,\"capped\":%s}",
        static_cast<unsigned>(text_base),
        kEz2dj4thTextRva,
        kEz2dj4thTextVirtualSize,
        static_cast<unsigned>(copied),
        candidate_count,
        read_candidate_count,
        write_candidate_count,
        recorded_count,
        capped ? "true" : "false");
}

bool FindNullContextObjectSourceBoundary(HANDLE process,
                                         std::uintptr_t image_base,
                                         std::uintptr_t* prologue,
                                         std::uintptr_t* boundary,
                                         std::string* error);

bool HandleNullContextFieldAccessBreakpoint(
    HANDLE process,
    DWORD thread_id,
    std::uintptr_t image_base,
    NullContextFieldWriterTraceState* state,
    const char* event_name,
    const std::set<std::uint32_t>* allocation_return_values,
    bool* handled,
    std::string* error)
{
    *handled = false;
    HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT,
                               FALSE,
                               thread_id);
    CONTEXT context = {};
    context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_DEBUG_REGISTERS;
    if (thread == nullptr || GetThreadContext(thread, &context) == FALSE)
    {
        if (thread != nullptr)
        {
            CloseHandle(thread);
        }
        *error = "cannot capture null-context field access context";
        return false;
    }
    if ((context.Dr6 & kNullContextFieldBreakpointStatus) == 0)
    {
        CloseHandle(thread);
        return true;
    }

    ++state->hit_count;
    const std::uintptr_t field_address =
        image_base + kEz2dj4thNullContextFieldRva;
    std::uint32_t field_value = 0;
    SIZE_T field_copied = 0;
    const bool field_readable =
        ReadProcessMemory(process,
                          reinterpret_cast<const void*>(field_address),
                          &field_value,
                          sizeof(field_value),
                          &field_copied) != FALSE &&
        field_copied == sizeof(field_value);
    std::uint32_t object_address = 0;
    const bool object_readable =
        context.Ebp >= 0x118 &&
        ReadRemoteU32(process, context.Ebp - 0x118, &object_address);
    std::uint32_t object_field_address = 0;
    std::uint32_t object_field_value = 0;
    bool object_field_readable = false;
    if (object_readable && object_address <= 0xffffffffu - 0x11c)
    {
        object_field_address = object_address + 0x11c;
        object_field_readable =
            ReadRemoteU32(process, object_field_address, &object_field_value);
    }
    std::uint32_t stack_return_address = 0;
    const bool stack_return_readable =
        ReadRemoteU32(process, context.Esp, &stack_return_address);
    const bool allocation_return_match =
        allocation_return_values != nullptr && object_readable && object_address != 0 &&
        allocation_return_values->count(object_address) != 0;
    const std::uintptr_t code_base =
        context.Eip >= image_base + 8 ? context.Eip - 8 : image_base;
    const RemoteBufferSnapshot code_window =
        ReadRemoteBufferSnapshot(process, code_base, 24);
    if (state->hit_count == 1)
    {
        std::uintptr_t runtime_prologue = 0;
        std::uintptr_t runtime_boundary = 0;
        std::string runtime_scan_error;
        const bool runtime_boundary_found =
            FindNullContextObjectSourceBoundary(process,
                                                image_base,
                                                &runtime_prologue,
                                                &runtime_boundary,
                                                &runtime_scan_error);
        RecordDiagnostic(
            "{\"event\":\"null_context_object_source_runtime_scan\",\"found\":%s,\"field_access_anchor\":\"0x%08x\",\"prologue\":\"0x%08x\",\"boundary\":\"0x%08x\"}",
            runtime_boundary_found ? "true" : "false",
            static_cast<unsigned>(image_base + kEz2dj4thNullContextFieldAccessRva),
            static_cast<unsigned>(runtime_prologue),
            static_cast<unsigned>(runtime_boundary));
        ScanRuntimeNullContextFieldReferences(process, image_base);
    }
    if (state->recorded_count < kNullContextFieldWriterHitLimit)
    {
        RecordDiagnostic(
            "{\"event\":\"%s\",\"sequence\":%u,\"thread\":%u,\"field\":\"0x%08x\",\"rva\":\"0x%08x\",\"eip_after\":\"0x%08x\",\"eax\":\"0x%08x\",\"ecx\":\"0x%08x\",\"ebp\":\"0x%08x\",\"esp\":\"0x%08x\",\"dr6\":\"0x%08x\",\"field_before_observed\":\"0x%08x\",\"field_before_observed_readable\":%s,\"field_after\":\"0x%08x\",\"field_after_readable\":%s,\"object\":\"0x%08x\",\"object_readable\":%s,\"object_field_address\":\"0x%08x\",\"object_field\":\"0x%08x\",\"object_field_readable\":%s,\"allocation_return_match\":%s,\"stack_return_address\":\"0x%08x\",\"stack_return_readable\":%s,\"code_base\":\"0x%08x\",\"code_readable\":%s,\"code\":\"%s\"}",
            event_name,
            state->hit_count,
            static_cast<unsigned>(thread_id),
            static_cast<unsigned>(field_address),
            kEz2dj4thNullContextFieldRva,
            static_cast<unsigned>(context.Eip),
            static_cast<unsigned>(context.Eax),
            static_cast<unsigned>(context.Ecx),
            static_cast<unsigned>(context.Ebp),
            static_cast<unsigned>(context.Esp),
            static_cast<unsigned>(context.Dr6),
            static_cast<unsigned>(state->last_field_value),
            state->last_field_readable ? "true" : "false",
            static_cast<unsigned>(field_value),
            field_readable ? "true" : "false",
            object_address,
            object_readable ? "true" : "false",
            object_field_address,
            object_field_value,
            object_field_readable ? "true" : "false",
            allocation_return_match ? "true" : "false",
            stack_return_address,
            stack_return_readable ? "true" : "false",
            static_cast<unsigned>(code_base),
            code_window.readable ? "true" : "false",
            code_window.readable ? code_window.bytes.c_str() : "");
        ++state->recorded_count;
    }
    else
    {
        state->capped = true;
    }
    state->last_field_value = field_value;
    state->last_field_readable = field_readable;

    context.Dr6 &= ~kNullContextFieldBreakpointStatus;
    if (SetThreadContext(thread, &context) == FALSE)
    {
        CloseHandle(thread);
        *error = "cannot clear null-context field access status";
        return false;
    }
    CloseHandle(thread);
    *handled = true;
    return true;
}

constexpr std::uint32_t kNullContextObjectSourceBoundaryHitLimit = 64;
constexpr std::uint32_t kNullContextObjectSourceHitLimit = 256;
constexpr DWORD kNullContextObjectSourceBoundaryStatus = static_cast<DWORD>(1u << 0);
constexpr DWORD kNullContextObjectSourceBoundaryEnable = static_cast<DWORD>(1u << 0);
constexpr DWORD kNullContextObjectSourceBoundaryMask =
    static_cast<DWORD>((0x3u << 0) | (0xfu << 16));
constexpr DWORD kNullContextObjectSourceBreakpointStatus = static_cast<DWORD>(1u << 2);
constexpr DWORD kNullContextObjectSourceBreakpointEnable = static_cast<DWORD>(1u << 4);
constexpr DWORD kNullContextObjectSourceBreakpointControl =
    static_cast<DWORD>((1u << 24) | (3u << 26));
constexpr DWORD kNullContextObjectSourceBreakpointMask =
    static_cast<DWORD>((0x3u << 4) | (0xfu << 24));

struct NullContextObjectSourceTraceState
{
    std::uint32_t boundary_hit_count = 0;
    std::uint32_t boundary_recorded_count = 0;
    bool boundary_capped = false;
    std::uint32_t hit_count = 0;
    std::uint32_t recorded_count = 0;
    std::uint32_t target_match_count = 0;
    bool capped = false;
    std::map<DWORD, std::uintptr_t> dynamic_stack_slots;
};

bool FindNullContextObjectSourceBoundary(HANDLE process,
                                         std::uintptr_t image_base,
                                         std::uintptr_t* prologue,
                                         std::uintptr_t* boundary,
                                         std::string* error)
{
    constexpr std::size_t kScanBackBytes = 0x1000;
    const std::uintptr_t field_access =
        image_base + kEz2dj4thNullContextFieldAccessRva;
    if (field_access < image_base + kScanBackBytes)
    {
        *error = "null-context field access anchor is outside image scan range";
        return false;
    }
    std::vector<std::uint8_t> bytes(kScanBackBytes);
    SIZE_T copied = 0;
    if (ReadProcessMemory(
            process,
            reinterpret_cast<const void*>(field_access - kScanBackBytes),
            bytes.data(),
            bytes.size(),
            &copied) == FALSE ||
        copied != bytes.size())
    {
        *error = "cannot read runtime bytes for null-context source boundary";
        return false;
    }
    for (std::size_t index = kScanBackBytes - 3; index != 0; --index)
    {
        if (bytes[index] == 0x55 && bytes[index + 1] == 0x8b &&
            bytes[index + 2] == 0xec)
        {
            *prologue = field_access - kScanBackBytes + index;
            *boundary = *prologue + 3;
            return true;
        }
    }
    *error = "cannot find null-context source function prologue";
    return false;
}

bool SetNullContextObjectSourceBoundaryBreakpoint(HANDLE thread,
                                                  std::uintptr_t boundary,
                                                  std::string* error)
{
    CONTEXT context = {};
    context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (GetThreadContext(thread, &context) == FALSE)
    {
        *error = "cannot read null-context object source debug registers";
        return false;
    }
    context.Dr0 = static_cast<DWORD>(boundary);
    context.Dr6 = 0;
    context.Dr7 &= ~kNullContextObjectSourceBoundaryMask;
    context.Dr7 |= kNullContextObjectSourceBoundaryEnable;
    context.Dr7 &= ~kNullContextObjectSourceBreakpointMask;
    if (SetThreadContext(thread, &context) == FALSE)
    {
        *error = "cannot set null-context object source boundary breakpoint";
        return false;
    }
    CONTEXT verified = {};
    verified.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (GetThreadContext(thread, &verified) == FALSE ||
        verified.Dr0 != context.Dr0 ||
        (verified.Dr7 & kNullContextObjectSourceBoundaryMask) !=
            kNullContextObjectSourceBoundaryEnable ||
        (verified.Dr7 & kNullContextObjectSourceBreakpointMask) != 0)
    {
        *error = "null-context object source boundary breakpoint was not retained";
        return false;
    }
    return true;
}

bool HandleNullContextObjectSourceBoundaryBreakpoint(
    HANDLE process,
    DWORD thread_id,
    std::uintptr_t image_base,
    std::uintptr_t boundary,
    NullContextObjectSourceTraceState* state,
    bool* handled,
    std::string* error)
{
    *handled = false;
    HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT,
                               FALSE,
                               thread_id);
    CONTEXT context = {};
    context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_DEBUG_REGISTERS;
    if (thread == nullptr || GetThreadContext(thread, &context) == FALSE)
    {
        if (thread != nullptr)
        {
            CloseHandle(thread);
        }
        *error = "cannot capture null-context object source boundary context";
        return false;
    }
    if ((context.Dr6 & kNullContextObjectSourceBoundaryStatus) == 0)
    {
        CloseHandle(thread);
        return true;
    }

    ++state->boundary_hit_count;
    std::uint32_t frame_slot = 0;
    std::uint32_t frame_slot_value = 0;
    const bool frame_slot_valid = context.Ebp >= 0x118;
    const bool frame_slot_readable =
        frame_slot_valid &&
        ReadRemoteU32(process,
                      context.Ebp - 0x118,
                      &frame_slot_value);
    if (frame_slot_valid)
    {
        frame_slot = context.Ebp - 0x118;
    }
    const std::uint32_t target_object = static_cast<std::uint32_t>(
        image_base + kEz2dj4thNullContextObjectRva);
    const bool frame_slot_matches_target =
        frame_slot_readable && frame_slot_value == target_object;
    if (state->boundary_recorded_count < kNullContextObjectSourceBoundaryHitLimit)
    {
        const std::uintptr_t code_base =
            context.Eip >= image_base + 8 ? context.Eip - 8 : image_base;
        const RemoteBufferSnapshot code_window =
            ReadRemoteBufferSnapshot(process, code_base, 24);
        RecordDiagnostic(
            "{\"event\":\"null_context_object_source_boundary_hit\",\"sequence\":%u,\"thread\":%u,\"boundary\":\"0x%08x\",\"eip\":\"0x%08x\",\"eax\":\"0x%08x\",\"ecx\":\"0x%08x\",\"ebp\":\"0x%08x\",\"esp\":\"0x%08x\",\"dr6\":\"0x%08x\",\"frame_slot\":\"0x%08x\",\"frame_slot_valid\":%s,\"frame_slot_readable\":%s,\"frame_slot_value\":\"0x%08x\",\"target_object\":\"0x%08x\",\"frame_slot_matches_target\":%s,\"code_base\":\"0x%08x\",\"code_readable\":%s,\"code\":\"%s\"}",
            state->boundary_hit_count,
            static_cast<unsigned>(thread_id),
            static_cast<unsigned>(boundary),
            static_cast<unsigned>(context.Eip),
            static_cast<unsigned>(context.Eax),
            static_cast<unsigned>(context.Ecx),
            static_cast<unsigned>(context.Ebp),
            static_cast<unsigned>(context.Esp),
            static_cast<unsigned>(context.Dr6),
            frame_slot,
            frame_slot_valid ? "true" : "false",
            frame_slot_readable ? "true" : "false",
            frame_slot_value,
            target_object,
            frame_slot_matches_target ? "true" : "false",
            static_cast<unsigned>(code_base),
            code_window.readable ? "true" : "false",
            code_window.readable ? code_window.bytes.c_str() : "");
        ++state->boundary_recorded_count;
    }
    else
    {
        state->boundary_capped = true;
    }

    context.Dr7 &= ~kNullContextObjectSourceBoundaryMask;
    context.Dr7 &= ~kNullContextObjectSourceBreakpointMask;
    if (frame_slot_valid)
    {
        context.Dr2 = frame_slot;
        context.Dr7 |= kNullContextObjectSourceBreakpointEnable |
                       kNullContextObjectSourceBreakpointControl;
        state->dynamic_stack_slots[thread_id] = frame_slot;
    }
    else
    {
        state->dynamic_stack_slots.erase(thread_id);
    }
    context.Dr6 &= ~(kNullContextObjectSourceBoundaryStatus |
                     kNullContextObjectSourceBreakpointStatus);
    if (SetThreadContext(thread, &context) == FALSE)
    {
        CloseHandle(thread);
        *error = "cannot arm null-context object source data breakpoint";
        return false;
    }
    CloseHandle(thread);
    *handled = true;
    return true;
}

bool HandleNullContextObjectSourceBreakpoint(
    HANDLE process,
    DWORD thread_id,
    std::uintptr_t image_base,
    std::uintptr_t stack_slot,
    NullContextObjectSourceTraceState* state,
    bool* handled,
    std::string* error)
{
    *handled = false;
    HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT,
                               FALSE,
                               thread_id);
    CONTEXT context = {};
    context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_DEBUG_REGISTERS;
    if (thread == nullptr || GetThreadContext(thread, &context) == FALSE)
    {
        if (thread != nullptr)
        {
            CloseHandle(thread);
        }
        *error = "cannot capture null-context object source context";
        return false;
    }
    if ((context.Dr6 & kNullContextObjectSourceBreakpointStatus) == 0)
    {
        CloseHandle(thread);
        return true;
    }

    ++state->hit_count;
    std::uint32_t stack_slot_value = 0;
    const bool stack_slot_readable =
        ReadRemoteU32(process,
                      static_cast<std::uint32_t>(stack_slot),
                      &stack_slot_value);
    std::uint32_t frame_slot = 0;
    std::uint32_t frame_slot_value = 0;
    const bool frame_slot_valid = context.Ebp >= 0x118;
    const bool frame_slot_readable =
        frame_slot_valid &&
        ReadRemoteU32(process,
                      context.Ebp - 0x118,
                      &frame_slot_value);
    if (frame_slot_valid)
    {
        frame_slot = context.Ebp - 0x118;
    }
    const std::uint32_t target_object = static_cast<std::uint32_t>(
        image_base + kEz2dj4thNullContextObjectRva);
    const bool stack_slot_matches_target =
        stack_slot_readable && stack_slot_value == target_object;
    const bool frame_slot_matches_target =
        frame_slot_readable && frame_slot_value == target_object;
    if (stack_slot_matches_target || frame_slot_matches_target)
    {
        ++state->target_match_count;
    }
    const bool frame_slot_matches_configured =
        frame_slot_valid && frame_slot == static_cast<std::uint32_t>(stack_slot);
    const std::uintptr_t code_base =
        context.Eip >= image_base + 8 ? context.Eip - 8 : image_base;
    const RemoteBufferSnapshot code_window =
        ReadRemoteBufferSnapshot(process, code_base, 24);
    if (state->recorded_count < kNullContextObjectSourceHitLimit)
    {
        RecordDiagnostic(
            "{\"event\":\"null_context_object_source_hit\",\"sequence\":%u,\"thread\":%u,\"configured_stack_slot\":\"0x%08x\",\"configured_stack_slot_readable\":%s,\"configured_stack_slot_value\":\"0x%08x\",\"frame_slot\":\"0x%08x\",\"frame_slot_valid\":%s,\"frame_slot_matches_configured\":%s,\"frame_slot_readable\":%s,\"frame_slot_value\":\"0x%08x\",\"target_object\":\"0x%08x\",\"stack_slot_matches_target\":%s,\"frame_slot_matches_target\":%s,\"eip_after\":\"0x%08x\",\"eax\":\"0x%08x\",\"ecx\":\"0x%08x\",\"ebp\":\"0x%08x\",\"esp\":\"0x%08x\",\"dr6\":\"0x%08x\",\"code_base\":\"0x%08x\",\"code_readable\":%s,\"code\":\"%s\"}",
            state->hit_count,
            static_cast<unsigned>(thread_id),
            static_cast<unsigned>(stack_slot),
            stack_slot_readable ? "true" : "false",
            stack_slot_value,
            frame_slot,
            frame_slot_valid ? "true" : "false",
            frame_slot_matches_configured ? "true" : "false",
            frame_slot_readable ? "true" : "false",
            frame_slot_value,
            target_object,
            stack_slot_matches_target ? "true" : "false",
            frame_slot_matches_target ? "true" : "false",
            static_cast<unsigned>(context.Eip),
            static_cast<unsigned>(context.Eax),
            static_cast<unsigned>(context.Ecx),
            static_cast<unsigned>(context.Ebp),
            static_cast<unsigned>(context.Esp),
            static_cast<unsigned>(context.Dr6),
            static_cast<unsigned>(code_base),
            code_window.readable ? "true" : "false",
            code_window.readable ? code_window.bytes.c_str() : "");
        ++state->recorded_count;
    }
    else
    {
        state->capped = true;
    }

    context.Dr6 &= ~kNullContextObjectSourceBreakpointStatus;
    if (SetThreadContext(thread, &context) == FALSE)
    {
        CloseHandle(thread);
        *error = "cannot clear null-context object source status";
        return false;
    }
    CloseHandle(thread);
    *handled = true;
    return true;
}

constexpr std::uint32_t kNullContextObjectStateHitLimit = 4;
constexpr std::uint32_t kNullContextObjectStateWindowBytes = 0x200;
constexpr std::size_t kNullContextObjectStateEntryLimit = 32;
constexpr std::size_t kNullContextObjectStateFrameLimit = 8;
// Heuristic span used only to label observed values as stack-resident. The
// probe never relies on it to decide what to read.
constexpr std::uintptr_t kNullContextObjectStateStackSpan = 0x00100000;

struct NullContextObjectStateTraceState
{
    std::uint32_t hit_count = 0;
    std::uint32_t recorded_count = 0;
    std::uint32_t frame_count = 0;
    bool window_readable = false;
    bool disarmed = false;
};

bool HandleNullContextObjectStateBreakpoint(
    HANDLE process,
    DWORD thread_id,
    std::uintptr_t image_base,
    std::uintptr_t image_end,
    NullContextObjectStateTraceState* state,
    bool* handled,
    std::string* error)
{
    *handled = false;
    HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT,
                               FALSE,
                               thread_id);
    CONTEXT context = {};
    context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_DEBUG_REGISTERS;
    if (thread == nullptr || GetThreadContext(thread, &context) == FALSE)
    {
        if (thread != nullptr)
        {
            CloseHandle(thread);
        }
        *error = "cannot read null-context object state thread context";
        return false;
    }
    if ((context.Dr6 & kNullContextObjectSourceBoundaryStatus) == 0)
    {
        CloseHandle(thread);
        return true;
    }

    ++state->hit_count;
    const std::uintptr_t object_address =
        image_base + kEz2dj4thNullContextObjectRva;
    const std::uintptr_t field_address =
        image_base + kEz2dj4thNullContextFieldRva;
    re2dj::tools::launcher_probe::AddressRanges ranges;
    ranges.image_base = image_base;
    ranges.image_end = image_end;
    ranges.stack_base = context.Esp > kNullContextObjectStateStackSpan
                            ? static_cast<std::uintptr_t>(context.Esp) -
                                  kNullContextObjectStateStackSpan
                            : 0;
    ranges.stack_end =
        static_cast<std::uintptr_t>(context.Esp) + kNullContextObjectStateStackSpan;

    if (state->recorded_count < kNullContextObjectStateHitLimit)
    {
        RecordDiagnostic(
            "{\"event\":\"null_context_object_state_hit\",\"sequence\":%u,\"thread\":%u,\"eip\":\"0x%08x\",\"ecx\":\"0x%08x\",\"ebp\":\"0x%08x\",\"esp\":\"0x%08x\",\"object\":\"0x%08x\",\"field\":\"0x%08x\",\"ecx_matches_object\":%s}",
            state->hit_count,
            static_cast<unsigned>(thread_id),
            static_cast<unsigned>(context.Eip),
            static_cast<unsigned>(context.Ecx),
            static_cast<unsigned>(context.Ebp),
            static_cast<unsigned>(context.Esp),
            static_cast<unsigned>(object_address),
            static_cast<unsigned>(field_address),
            static_cast<std::uintptr_t>(context.Ecx) == object_address ? "true"
                                                                      : "false");

        const std::vector<re2dj::tools::launcher_probe::CallerFrame> frames =
            re2dj::tools::launcher_probe::CollectCallerFrames(
                process,
                static_cast<std::uintptr_t>(context.Ebp),
                ranges,
                kNullContextObjectStateFrameLimit);
        for (std::size_t index = 0; index < frames.size(); ++index)
        {
            const auto& frame = frames[index];
            RecordDiagnostic(
                "{\"event\":\"null_context_object_state_frame\",\"sequence\":%u,\"depth\":%u,\"frame\":\"0x%08x\",\"saved_frame\":\"0x%08x\",\"return\":\"0x%08x\",\"return_in_image\":%s,\"return_rva\":\"0x%08x\"}",
                state->hit_count,
                static_cast<unsigned>(index),
                static_cast<unsigned>(frame.frame),
                static_cast<unsigned>(frame.saved_frame),
                static_cast<unsigned>(frame.return_address),
                frame.return_in_image ? "true" : "false",
                frame.return_in_image
                    ? static_cast<unsigned>(frame.return_address - image_base)
                    : 0u);
        }
        state->frame_count = static_cast<std::uint32_t>(frames.size());

        const re2dj::tools::launcher_probe::ObjectWindowScan scan =
            re2dj::tools::launcher_probe::ScanObjectWindow(
                process,
                object_address,
                kNullContextObjectStateWindowBytes,
                kEz2dj4thNullContextFieldRva - kEz2dj4thNullContextObjectRva,
                ranges,
                kNullContextObjectStateEntryLimit);
        state->window_readable = scan.readable;
        RecordDiagnostic(
            "{\"event\":\"null_context_object_state_window\",\"sequence\":%u,\"object\":\"0x%08x\",\"readable\":%s,\"bytes\":%u,\"dwords\":%u,\"nonzero\":%u,\"field_offset\":\"0x%08x\",\"field_scanned\":%s,\"field_value\":\"0x%08x\",\"entries\":%u,\"capped\":%s}",
            state->hit_count,
            static_cast<unsigned>(object_address),
            scan.readable ? "true" : "false",
            scan.bytes_scanned,
            scan.dwords_scanned,
            scan.nonzero_count,
            scan.field_offset,
            scan.field_offset_scanned ? "true" : "false",
            scan.field_value,
            static_cast<unsigned>(scan.entries.size()),
            scan.capped ? "true" : "false");
        for (const auto& entry : scan.entries)
        {
            RecordDiagnostic(
                "{\"event\":\"null_context_object_state_entry\",\"sequence\":%u,\"offset\":\"0x%08x\",\"value\":\"0x%08x\",\"class\":\"%s\"}",
                state->hit_count,
                entry.offset,
                entry.value,
                entry.classification);
        }
        ++state->recorded_count;
    }

    if (state->hit_count >= kNullContextObjectStateHitLimit)
    {
        context.Dr7 &= ~kNullContextObjectSourceBoundaryMask;
        state->disarmed = true;
    }
    context.Dr6 &= ~kNullContextObjectSourceBoundaryStatus;
    if (SetThreadContext(thread, &context) == FALSE)
    {
        CloseHandle(thread);
        *error = "cannot clear null-context object state status";
        return false;
    }
    CloseHandle(thread);
    *handled = true;
    return true;
}

bool SetNullContextEntryBreakpoints(HANDLE thread,
                                    std::uintptr_t image_base,
                                    std::string* error)
{
    CONTEXT context = {};
    context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (GetThreadContext(thread, &context) == FALSE)
    {
        *error = "cannot read null-context entry debug registers";
        return false;
    }
    context.Dr0 = static_cast<DWORD>(image_base + kNullContextEntryPoints[0].rva);
    context.Dr1 = static_cast<DWORD>(image_base + kNullContextEntryPoints[1].rva);
    context.Dr2 = static_cast<DWORD>(image_base + kNullContextEntryPoints[2].rva);
    context.Dr3 = static_cast<DWORD>(image_base + kNullContextEntryPoints[3].rva);
    context.Dr6 &= ~kNullContextFieldReferenceExecutionStatusMask;
    context.Dr7 &= ~kNullContextFieldReferenceExecutionBreakpointMask;
    context.Dr7 |= kNullContextFieldReferenceExecutionBreakpointEnable;
    if (SetThreadContext(thread, &context) == FALSE)
    {
        *error = "cannot set null-context entry breakpoints";
        return false;
    }
    CONTEXT verified = {};
    verified.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (GetThreadContext(thread, &verified) == FALSE || verified.Dr0 != context.Dr0 ||
        verified.Dr1 != context.Dr1 || verified.Dr2 != context.Dr2 ||
        verified.Dr3 != context.Dr3 ||
        (verified.Dr7 & kNullContextFieldReferenceExecutionBreakpointMask) !=
            kNullContextFieldReferenceExecutionBreakpointEnable)
    {
        *error = "null-context entry breakpoints were not retained";
        return false;
    }
    return true;
}

bool HandleNullContextEntryBreakpoint(HANDLE process,
                                      DWORD thread_id,
                                      std::uintptr_t image_base,
                                      NullContextEntryTraceState* state,
                                      bool* handled,
                                      std::string* error)
{
    *handled = false;
    HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, thread_id);
    CONTEXT context = {};
    context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_DEBUG_REGISTERS;
    if (thread == nullptr || GetThreadContext(thread, &context) == FALSE)
    {
        if (thread != nullptr)
        {
            CloseHandle(thread);
        }
        *error = "cannot read null-context entry thread context";
        return false;
    }

    const DWORD execution_status =
        context.Dr6 & kNullContextFieldReferenceExecutionStatusMask;
    const auto pending_step = state->pending_single_step_threads.find(thread_id);
    if (pending_step != state->pending_single_step_threads.end() && execution_status == 0)
    {
        context.EFlags &= ~0x100u;
        if (SetThreadContext(thread, &context) == FALSE ||
            !SetNullContextEntryBreakpoints(thread, image_base, error))
        {
            CloseHandle(thread);
            if (error->empty())
            {
                *error = "cannot restore null-context entry breakpoints";
            }
            return false;
        }
        state->pending_single_step_threads.erase(pending_step);
        CloseHandle(thread);
        *handled = true;
        return true;
    }
    if (execution_status == 0)
    {
        CloseHandle(thread);
        return true;
    }

    std::size_t entry_index = kNullContextEntryPoints.size();
    for (std::size_t index = 0; index < kNullContextEntryPoints.size(); ++index)
    {
        if ((execution_status & (1u << index)) != 0 &&
            context.Eip == static_cast<DWORD>(image_base + kNullContextEntryPoints[index].rva))
        {
            entry_index = index;
            break;
        }
    }
    if (entry_index == kNullContextEntryPoints.size())
    {
        for (std::size_t index = 0; index < kNullContextEntryPoints.size(); ++index)
        {
            if (context.Eip ==
                static_cast<DWORD>(image_base + kNullContextEntryPoints[index].rva))
            {
                entry_index = index;
                break;
            }
        }
    }
    if (entry_index == kNullContextEntryPoints.size())
    {
        context.Dr6 &= ~kNullContextFieldReferenceExecutionStatusMask;
        SetThreadContext(thread, &context);
        CloseHandle(thread);
        return true;
    }

    ++state->hit_count;
    ++state->hit_counts[entry_index];
    const std::uintptr_t singleton = image_base + kEz2dj4thNullContextObjectRva;
    const bool receiver_matches = context.Ecx == static_cast<DWORD>(singleton);
    if (receiver_matches)
    {
        ++state->singleton_receiver_count;
    }
    // The breakpoint fires on the function's first byte, before `push ebp`, so
    // the caller's return address is still on top of the stack.
    std::uint32_t return_address = 0;
    const bool return_readable =
        ReadRemoteU32(process, static_cast<std::uint32_t>(context.Esp), &return_address);

    std::uint32_t virtual_func_target = 0;
    bool virtual_func_readable = false;
    std::string virtual_func_symbol;
    std::uint32_t stack_arg0 = 0;
    bool stack_arg0_readable = false;
    std::uint32_t this_ptr = 0;
    bool this_ptr_readable = false;

    if (entry_index == 0)  // virtual_call_site: call dword ptr [ecx+0x54]
    {
        virtual_func_readable = ReadRemoteU32(
            process, static_cast<std::uint32_t>(context.Ecx + 0x54), &virtual_func_target);
        if (virtual_func_readable)
        {
            MEMORY_BASIC_INFORMATION region = {};
            if (VirtualQueryEx(process,
                               reinterpret_cast<const void*>(virtual_func_target),
                               &region,
                               sizeof(region)) == sizeof(region) &&
                region.Type == MEM_IMAGE && region.AllocationBase != nullptr)
            {
                const std::uintptr_t mod_base =
                    reinterpret_cast<std::uintptr_t>(region.AllocationBase);
                re2dj::tools::windows_x86_launcher_probe::RemoteNearestExport nearest = {};
                std::string nearest_err;
                if (re2dj::tools::windows_x86_launcher_probe::FindRemotePe32NearestExport(
                        process, mod_base, virtual_func_target, &nearest, &nearest_err))
                {
                    char sym_buf[288] = {};
                    std::snprintf(sym_buf,
                                  sizeof(sym_buf),
                                  "%s!%s+0x%x",
                                  nearest.module,
                                  nearest.function,
                                  static_cast<unsigned>(nearest.offset));
                    SanitizeJsonText(sym_buf);
                    virtual_func_symbol = sym_buf;
                }
                else
                {
                    char sym_buf[64] = {};
                    std::snprintf(sym_buf,
                                  sizeof(sym_buf),
                                  "module_base_0x%08x",
                                  static_cast<unsigned>(mod_base));
                    virtual_func_symbol = sym_buf;
                }
            }
        }
        stack_arg0_readable = ReadRemoteU32(
            process, static_cast<std::uint32_t>(context.Esp), &stack_arg0);
        this_ptr_readable = ReadRemoteU32(
            process, static_cast<std::uint32_t>(context.Ebp - 0xa8), &this_ptr);

        // 1. Read code bytes around call site: 0x00410a50 to 0x00410a75 (38 bytes)
        const std::uintptr_t code_win_start = image_base + 0x00010a50;
        std::uint8_t code_buf[40] = {};
        SIZE_T code_copied = 0;
        std::string code_hex;
        if (ReadProcessMemory(process,
                              reinterpret_cast<const void*>(code_win_start),
                              code_buf,
                              sizeof(code_buf),
                              &code_copied) != FALSE &&
            code_copied > 0)
        {
            char hex_buf[sizeof(code_buf) * 2 + 1] = {};
            for (SIZE_T i = 0; i < code_copied; ++i)
            {
                std::snprintf(hex_buf + i * 2, 3, "%02x", code_buf[i]);
            }
            code_hex = hex_buf;
        }

        // 2. Read stack arguments: 6 DWORDs from ESP
        char stack_buf[128] = {};
        std::size_t stack_pos = 0;
        for (std::size_t i = 0; i < 6; ++i)
        {
            std::uint32_t val = 0;
            if (ReadRemoteU32(process, static_cast<std::uint32_t>(context.Esp + i * 4), &val))
            {
                stack_pos += std::snprintf(stack_buf + stack_pos,
                                           sizeof(stack_buf) - stack_pos,
                                           "%s0x%08x",
                                           i > 0 ? "," : "",
                                           val);
            }
        }

        // 3. Resolve IAT slots 0x006d1908 and 0x006d1724
        char iat_buf[512] = {};
        std::size_t iat_pos = 0;
        for (std::uint32_t slot_rva : {0x006d1908u, 0x006d1724u})
        {
            const std::uintptr_t slot_va = image_base + slot_rva;
            std::uint32_t func_ptr = 0;
            const bool ptr_readable =
                ReadRemoteU32(process, static_cast<std::uint32_t>(slot_va), &func_ptr);
            std::string sym_name;
            if (ptr_readable && func_ptr != 0)
            {
                MEMORY_BASIC_INFORMATION reg = {};
                if (VirtualQueryEx(process,
                                   reinterpret_cast<const void*>(func_ptr),
                                   &reg,
                                   sizeof(reg)) == sizeof(reg) &&
                    reg.Type == MEM_IMAGE && reg.AllocationBase != nullptr)
                {
                    const std::uintptr_t mod_base =
                        reinterpret_cast<std::uintptr_t>(reg.AllocationBase);
                    re2dj::tools::windows_x86_launcher_probe::RemoteNearestExport near_exp = {};
                    std::string err;
                    if (re2dj::tools::windows_x86_launcher_probe::FindRemotePe32NearestExport(
                            process, mod_base, func_ptr, &near_exp, &err))
                    {
                        char sym_buf[288] = {};
                        std::snprintf(sym_buf,
                                      sizeof(sym_buf),
                                      "%s!%s+0x%x",
                                      near_exp.module,
                                      near_exp.function,
                                      static_cast<unsigned>(near_exp.offset));
                        SanitizeJsonText(sym_buf);
                        sym_name = sym_buf;
                    }
                }
            }
            iat_pos += std::snprintf(iat_buf + iat_pos,
                                     sizeof(iat_buf) - iat_pos,
                                     "%s{\"slot_rva\":\"0x%08x\",\"ptr\":\"0x%08x\",\"sym\":\"%s\"}",
                                     iat_pos > 0 ? "," : "",
                                     slot_rva,
                                     func_ptr,
                                     sym_name.c_str());
        }

        // 4. Inspect vtable entries: index 0 (0x00), 6 (0x18), 20 (0x50), 21 (0x54), 22 (0x58)
        char vt_buf[512] = {};
        std::size_t vt_pos = 0;
        for (std::uint32_t idx : {0u, 6u, 20u, 21u, 22u})
        {
            const std::uint32_t vt_slot = static_cast<std::uint32_t>(context.Ecx + idx * 4);
            std::uint32_t fn_addr = 0;
            if (ReadRemoteU32(process, vt_slot, &fn_addr))
            {
                std::string sym_name;
                MEMORY_BASIC_INFORMATION reg = {};
                if (VirtualQueryEx(process,
                                   reinterpret_cast<const void*>(fn_addr),
                                   &reg,
                                   sizeof(reg)) == sizeof(reg) &&
                    reg.Type == MEM_IMAGE && reg.AllocationBase != nullptr)
                {
                    const std::uintptr_t mod_base =
                        reinterpret_cast<std::uintptr_t>(reg.AllocationBase);
                    re2dj::tools::windows_x86_launcher_probe::RemoteNearestExport near_exp = {};
                    std::string err;
                    if (re2dj::tools::windows_x86_launcher_probe::FindRemotePe32NearestExport(
                            process, mod_base, fn_addr, &near_exp, &err))
                    {
                        char sym_buf[288] = {};
                        std::snprintf(sym_buf,
                                      sizeof(sym_buf),
                                      "%s!%s+0x%x",
                                      near_exp.module,
                                      near_exp.function,
                                      static_cast<unsigned>(near_exp.offset));
                        SanitizeJsonText(sym_buf);
                        sym_name = sym_buf;
                    }
                }
                vt_pos += std::snprintf(vt_buf + vt_pos,
                                        sizeof(vt_buf) - vt_pos,
                                        "%s{\"idx\":%u,\"addr\":\"0x%08x\",\"sym\":\"%s\"}",
                                        vt_pos > 0 ? "," : "",
                                        idx,
                                        fn_addr,
                                        sym_name.c_str());
            }
        }

        RecordDiagnostic(
            "{\"event\":\"null_context_virtual_call_context\",\"code_hex\":\"%s\",\"stack\":[%s],\"iat\":[%s],\"vtable\":[%s]}",
            code_hex.c_str(),
            stack_buf,
            iat_buf,
            vt_buf);

        // 5. Scan unpacked .idata range (0x006d1000 to 0x006d3000) for DDRAW.dll pointers
        const std::uintptr_t idata_base = image_base + 0x006d1000;
        constexpr std::size_t idata_size = 0x2000;
        std::vector<std::uint32_t> idata_words(idata_size / sizeof(std::uint32_t));
        SIZE_T idata_read = 0;
        if (ReadProcessMemory(process,
                              reinterpret_cast<const void*>(idata_base),
                              idata_words.data(),
                              idata_size,
                              &idata_read) != FALSE &&
            idata_read > 0)
        {
            for (std::size_t i = 0; i < idata_words.size(); ++i)
            {
                const std::uint32_t ptr = idata_words[i];
                if (ptr == 0)
                {
                    continue;
                }
                MEMORY_BASIC_INFORMATION reg = {};
                if (VirtualQueryEx(process,
                                   reinterpret_cast<const void*>(ptr),
                                   &reg,
                                   sizeof(reg)) == sizeof(reg) &&
                    reg.Type == MEM_IMAGE && reg.AllocationBase != nullptr)
                {
                    const std::uintptr_t mod_base =
                        reinterpret_cast<std::uintptr_t>(reg.AllocationBase);
                    re2dj::tools::windows_x86_launcher_probe::RemoteNearestExport near_exp = {};
                    std::string err;
                    if (re2dj::tools::windows_x86_launcher_probe::FindRemotePe32NearestExport(
                            process, mod_base, ptr, &near_exp, &err))
                    {
                        if (_stricmp(near_exp.module, "ddraw.dll") == 0 ||
                            std::strstr(near_exp.function, "DirectDraw") != nullptr)
                        {
                            RecordDiagnostic(
                                "{\"event\":\"unpacked_idata_ddraw_slot\",\"slot_rva\":\"0x%08x\",\"ptr\":\"0x%08x\",\"module\":\"%s\",\"function\":\"%s\",\"offset\":\"0x%x\"}",
                                static_cast<unsigned>(0x006d1000 + i * sizeof(std::uint32_t)),
                                ptr,
                                near_exp.module,
                                near_exp.function,
                                static_cast<unsigned>(near_exp.offset));
                        }
                    }
                }
            }
        }
    }

    if (state->recorded_counts[entry_index] < kNullContextEntryHitLimit)
    {
        RecordDiagnostic(
            "{\"event\":\"null_context_entry_hit\",\"sequence\":%u,\"thread\":%u,\"name\":\"%s\",\"rva\":\"0x%08x\",\"eip\":\"0x%08x\",\"eax\":\"0x%08x\",\"ecx\":\"0x%08x\",\"edx\":\"0x%08x\",\"receiver_is_singleton\":%s,\"esp\":\"0x%08x\",\"return\":\"0x%08x\",\"return_readable\":%s,\"return_rva\":\"0x%08x\",\"virtual_func_target\":\"0x%08x\",\"virtual_func_readable\":%s,\"virtual_func_symbol\":\"%s\",\"stack_arg0\":\"0x%08x\",\"stack_arg0_readable\":%s,\"this_ptr\":\"0x%08x\",\"this_ptr_readable\":%s}",
            state->hit_count,
            static_cast<unsigned>(thread_id),
            kNullContextEntryPoints[entry_index].name,
            kNullContextEntryPoints[entry_index].rva,
            static_cast<unsigned>(context.Eip),
            static_cast<unsigned>(context.Eax),
            static_cast<unsigned>(context.Ecx),
            static_cast<unsigned>(context.Edx),
            receiver_matches ? "true" : "false",
            static_cast<unsigned>(context.Esp),
            return_address,
            return_readable ? "true" : "false",
            return_readable && return_address >= image_base
                ? static_cast<unsigned>(return_address - image_base)
                : 0u,
            virtual_func_target,
            virtual_func_readable ? "true" : "false",
            virtual_func_symbol.c_str(),
            stack_arg0,
            stack_arg0_readable ? "true" : "false",
            this_ptr,
            this_ptr_readable ? "true" : "false");
        ++state->recorded_counts[entry_index];
        ++state->recorded_count;
    }
    else
    {
        state->capped = true;
    }

    context.Dr7 &= ~kNullContextFieldReferenceExecutionBreakpointMask;
    context.Dr6 &= ~kNullContextFieldReferenceExecutionStatusMask;
    context.EFlags |= 0x100u;
    if (SetThreadContext(thread, &context) == FALSE)
    {
        CloseHandle(thread);
        *error = "cannot arm null-context entry single step";
        return false;
    }
    state->pending_single_step_threads.insert(thread_id);
    CloseHandle(thread);
    *handled = true;
    return true;
}

constexpr std::size_t kNullContextObjectReferenceLimit = 128;
constexpr std::uint32_t kNullContextObjectVtableEntries = 16;
// The device enumeration record table guard 1 walks, confirmed by Task 170.
// The selection loop takes the base and the count from these two globals,
// indexes with the stride, and drops every record whose gate reads zero.
constexpr std::uint32_t kEz2dj4thDeviceRecordTableRva = 0x00546d50;
constexpr std::uint32_t kEz2dj4thDeviceRecordCountRva = 0x0054cd9c;
constexpr std::uint32_t kEz2dj4thDeviceRecordStride = 0x000004d0;
constexpr std::uint32_t kEz2dj4thDeviceRecordGateOffset = 0x000004c8;
// Records summarized by the window scan, and the per-record cap on reported
// nonzero dwords. A full record holds 0x134 dwords, so the cap only truncates
// a record that is almost entirely filled.
constexpr std::uint32_t kEz2dj4thDeviceRecordWindowCount = 2;
constexpr std::size_t kEz2dj4thDeviceRecordEntryLimit = 128;
// Backward search range for the enclosing `push ebp; mov ebp, esp` and the
// size of the recorded code region around an anchor.
constexpr std::size_t kNullContextPrologueScanBack = 0x2000;
constexpr std::size_t kNullContextAnchorLeadBytes = 0x20;
constexpr std::size_t kNullContextAnchorTrailBytes = 0x10;
constexpr std::size_t kNullContextCodeWindowBytes = 0xc0;
constexpr std::size_t kNullContextBranchSiteLimit = 32;
constexpr std::size_t kNullContextBodyBranchLimit = 192;
// Bytes read ahead of a caller return address, enough to contain the call and
// the branch that selected it.
constexpr std::uint32_t kNullContextObjectCallerLeadBytes = 24;
constexpr std::uint32_t kNullContextObjectCallerWindowBytes = 32;

struct NullContextObjectReferenceScanState
{
    std::uint32_t hit_count = 0;
    bool scanned = false;
    bool text_readable = false;
    std::uint32_t match_count = 0;
    bool capped = false;
    bool disarmed = false;
};

bool HandleNullContextObjectReferenceScanBreakpoint(
    HANDLE process,
    DWORD thread_id,
    std::uintptr_t image_base,
    std::uintptr_t image_end,
    NullContextObjectReferenceScanState* state,
    bool* handled,
    std::string* error)
{
    *handled = false;
    HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT,
                               FALSE,
                               thread_id);
    CONTEXT context = {};
    context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_DEBUG_REGISTERS;
    if (thread == nullptr || GetThreadContext(thread, &context) == FALSE)
    {
        if (thread != nullptr)
        {
            CloseHandle(thread);
        }
        *error = "cannot read null-context object reference scan thread context";
        return false;
    }
    if ((context.Dr6 & kNullContextObjectSourceBoundaryStatus) == 0)
    {
        CloseHandle(thread);
        return true;
    }

    ++state->hit_count;
    if (!state->scanned)
    {
        const std::uintptr_t object_address =
            image_base + kEz2dj4thNullContextObjectRva;
        const std::uintptr_t global_address =
            image_base + kEz2dj4thNullContextObjectGlobalRva;
        const std::uintptr_t text_base = image_base + kEz2dj4thTextRva;
        const std::uintptr_t text_end = text_base + kEz2dj4thTextVirtualSize;
        std::uint32_t vtable_pointer = 0;
        const bool vtable_readable = ReadRemoteU32(
            process, static_cast<std::uint32_t>(object_address), &vtable_pointer);
        const bool vtable_in_image =
            vtable_readable && vtable_pointer >= image_base && vtable_pointer < image_end;
        std::uint32_t global_value = 0;
        const bool global_readable = ReadRemoteU32(
            process, static_cast<std::uint32_t>(global_address), &global_value);
        RecordDiagnostic(
            "{\"event\":\"null_context_object_reference_scan_context\",\"thread\":%u,\"eip\":\"0x%08x\",\"ecx\":\"0x%08x\",\"object\":\"0x%08x\",\"vtable\":\"0x%08x\",\"vtable_readable\":%s,\"vtable_in_image\":%s,\"vtable_rva\":\"0x%08x\",\"global\":\"0x%08x\",\"global_value\":\"0x%08x\",\"global_readable\":%s,\"global_matches_object\":%s}",
            static_cast<unsigned>(thread_id),
            static_cast<unsigned>(context.Eip),
            static_cast<unsigned>(context.Ecx),
            static_cast<unsigned>(object_address),
            vtable_pointer,
            vtable_readable ? "true" : "false",
            vtable_in_image ? "true" : "false",
            vtable_in_image ? static_cast<unsigned>(vtable_pointer - image_base) : 0u,
            static_cast<unsigned>(global_address),
            global_value,
            global_readable ? "true" : "false",
            global_readable && global_value == static_cast<std::uint32_t>(object_address)
                ? "true"
                : "false");

        if (vtable_in_image)
        {
            for (std::uint32_t index = 0; index < kNullContextObjectVtableEntries;
                 ++index)
            {
                const std::uint32_t slot =
                    vtable_pointer + index * static_cast<std::uint32_t>(sizeof(std::uint32_t));
                std::uint32_t entry = 0;
                const bool entry_readable = ReadRemoteU32(process, slot, &entry);
                const bool entry_in_text =
                    entry_readable && entry >= text_base && entry < text_end;
                RecordDiagnostic(
                    "{\"event\":\"null_context_object_vtable_entry\",\"index\":%u,\"slot\":\"0x%08x\",\"entry\":\"0x%08x\",\"readable\":%s,\"in_text\":%s,\"rva\":\"0x%08x\"}",
                    index,
                    slot,
                    entry,
                    entry_readable ? "true" : "false",
                    entry_in_text ? "true" : "false",
                    entry_in_text ? static_cast<unsigned>(entry - image_base) : 0u);
            }
        }

        std::vector<std::uint8_t> text(kEz2dj4thTextVirtualSize);
        SIZE_T copied = 0;
        state->text_readable =
            ReadProcessMemory(process,
                              reinterpret_cast<const void*>(text_base),
                              text.data(),
                              text.size(),
                              &copied) != FALSE &&
            copied == text.size();
        if (state->text_readable)
        {
            // One pass per value so a heavily referenced address cannot hide
            // the totals of the others behind the record cap.
            const struct
            {
                const char* kind;
                std::uint32_t value;
                bool enabled;
            } scans[] = {
                {"object", static_cast<std::uint32_t>(object_address), true},
                {"global", static_cast<std::uint32_t>(global_address), true},
                {"vtable", vtable_pointer, vtable_in_image},
                // The application-defined failure codes the guard calls
                // returned, so every site that produces them can be located.
                {"error_code", 0x8200000au, true},
                {"guard1_error_code", 0x81000004u, true},
                // The device record table guard 1 walks. The two absolute
                // addresses find every site that touches the table, the stride
                // finds the sites that index it, and the gate displacement
                // finds the sites that touch the field that skipped every
                // record. A `0x4c8` access inside a function that also indexes
                // with `0x4d0` is the gate writer candidate.
                {"device_table_base",
                 static_cast<std::uint32_t>(image_base + kEz2dj4thDeviceRecordTableRva),
                 true},
                {"device_count_global",
                 static_cast<std::uint32_t>(image_base + kEz2dj4thDeviceRecordCountRva),
                 true},
                {"device_record_stride", kEz2dj4thDeviceRecordStride, true},
                {"device_gate_displacement", kEz2dj4thDeviceRecordGateOffset, true},
            };
            for (const auto& scan : scans)
            {
                if (!scan.enabled)
                {
                    continue;
                }
                bool capped = false;
                std::size_t total = 0;
                const std::vector<re2dj::exe::ImmediateReference> matches =
                    re2dj::exe::ScanImmediateReferences(text.data(),
                                                        text.size(),
                                                        &scan.value,
                                                        1,
                                                        kNullContextObjectReferenceLimit,
                                                        &capped,
                                                        &total);
                state->match_count += static_cast<std::uint32_t>(total);
                state->capped = state->capped || capped;
                for (const auto& match : matches)
                {
                    char leading[re2dj::exe::ImmediateReference::kContextBytes * 2 + 1] = {};
                    for (std::size_t index = 0; index < match.leading_count; ++index)
                    {
                        std::snprintf(leading + index * 2, 3, "%02x", match.leading[index]);
                    }
                    char trailing[re2dj::exe::ImmediateReference::kContextBytes * 2 + 1] = {};
                    for (std::size_t index = 0; index < match.trailing_count; ++index)
                    {
                        std::snprintf(trailing + index * 2, 3, "%02x", match.trailing[index]);
                    }
                    // A `call rel32` directly after the operand names the
                    // function that received this value, so resolve it. Any
                    // other following byte leaves the callee unreported.
                    const std::uintptr_t next_instruction =
                        text_base + match.offset + sizeof(std::uint32_t);
                    const bool call_follows =
                        match.trailing_count >= 5 && match.trailing[0] == 0xe8;
                    std::uint32_t callee = 0;
                    if (call_follows)
                    {
                        const std::uint32_t relative =
                            static_cast<std::uint32_t>(match.trailing[1]) |
                            (static_cast<std::uint32_t>(match.trailing[2]) << 8) |
                            (static_cast<std::uint32_t>(match.trailing[3]) << 16) |
                            (static_cast<std::uint32_t>(match.trailing[4]) << 24);
                        callee = static_cast<std::uint32_t>(next_instruction) + 5 + relative;
                    }
                    RecordDiagnostic(
                        "{\"event\":\"null_context_object_reference\",\"address\":\"0x%08x\",\"rva\":\"0x%08x\",\"value\":\"0x%08x\",\"kind\":\"%s\",\"leading\":\"%s\",\"trailing\":\"%s\",\"call_follows\":%s,\"callee\":\"0x%08x\",\"callee_rva\":\"0x%08x\"}",
                        static_cast<unsigned>(text_base + match.offset),
                        static_cast<unsigned>(kEz2dj4thTextRva + match.offset),
                        match.value,
                        scan.kind,
                        leading,
                        trailing,
                        call_follows ? "true" : "false",
                        callee,
                        call_follows ? static_cast<unsigned>(callee - image_base) : 0u);
                }
                RecordDiagnostic(
                    "{\"event\":\"null_context_object_reference_kind\",\"kind\":\"%s\",\"value\":\"0x%08x\",\"total\":%u,\"recorded\":%u,\"capped\":%s}",
                    scan.kind,
                    scan.value,
                    static_cast<unsigned>(total),
                    static_cast<unsigned>(matches.size()),
                    capped ? "true" : "false");
            }
        }

        const std::vector<re2dj::tools::launcher_probe::CallerFrame> frames =
            re2dj::tools::launcher_probe::CollectCallerFrames(
                process,
                static_cast<std::uintptr_t>(context.Ebp),
                re2dj::tools::launcher_probe::AddressRanges{image_base, image_end, 0, 0},
                kNullContextObjectStateFrameLimit);
        for (std::size_t index = 0; index < frames.size(); ++index)
        {
            const auto& frame = frames[index];
            if (frame.return_address < kNullContextObjectCallerLeadBytes)
            {
                continue;
            }
            const std::uint32_t window_base = static_cast<std::uint32_t>(
                frame.return_address - kNullContextObjectCallerLeadBytes);
            const RemoteBufferSnapshot window = ReadRemoteBufferSnapshot(
                process, window_base, kNullContextObjectCallerWindowBytes);
            RecordDiagnostic(
                "{\"event\":\"null_context_object_caller_window\",\"depth\":%u,\"return\":\"0x%08x\",\"return_rva\":\"0x%08x\",\"window_base\":\"0x%08x\",\"lead_bytes\":%u,\"readable\":%s,\"bytes\":\"%s\"}",
                static_cast<unsigned>(index),
                static_cast<unsigned>(frame.return_address),
                frame.return_in_image
                    ? static_cast<unsigned>(frame.return_address - image_base)
                    : 0u,
                window_base,
                kNullContextObjectCallerLeadBytes,
                window.readable ? "true" : "false",
                window.readable ? window.bytes.c_str() : "");
        }

        if (state->text_readable)
        {
            // Anchors whose enclosing function bodies decide the remaining
            // question: the field read itself, the two `+0x11c` writers that
            // actually execute, and the sites that install this class vtable.
            const struct
            {
                const char* name;
                std::uint32_t rva;
            } anchors[] = {
                {"field_read", kEz2dj4thNullContextFieldAccessRva},
                {"write_candidate_0", 0x0000fdbd},
                {"write_candidate_1", 0x0000fde1},
                {"vtable_store_0", 0x00010381},
                {"vtable_store_1", 0x000104a1},
                {"write_candidate_2", 0x0001825f},
                {"write_candidate_3", 0x0001dbd3},
                // The only call site of the function that writes the field on
                // its own receiver, found by the Task 160 branch scan.
                {"candidate_2_call_site", 0x00011c23},
                {"slot2_early_exit_0", 0x00011714},
                {"slot2_early_exit_1", 0x00011738},
                {"slot2_early_exit_2", 0x00011838},
                {"guard2_target_entry", 0x000106d2},
                {"guard2_failure_site", 0x00010a7b},
                {"guard1_target_entry", 0x0001010f},
                // The loop that precedes the decision, and the four-way
                // comparison chain whose last mismatch falls into the
                // 0x81000004 return at 0x000102a0.
                {"guard1_loop_head", 0x00010190},
                {"guard1_loop_body", 0x000101d0},
                {"guard1_loop_tail", 0x00010220},
                {"guard1_chain_0", 0x00010250},
                {"guard1_chain_1", 0x00010270},
                {"guard1_chain_2", 0x00010290},
                {"guard1_chain_3", 0x000102b0},
                // Task 171: the driver stage that builds the context whose
                // +0x4c8 the device callback copies, and the callback itself.
                {"driver_stage_context_zero", 0x0000f93e},
                {"device_enum_callback", 0x0000fc57},
                // Task 178: the instruction that faults on a null object
                // during panel-sprite loading.
                {"panel_null_object", 0x0001290e},
            };
            for (const auto& anchor : anchors)
            {
                if (anchor.rva < kEz2dj4thTextRva)
                {
                    continue;
                }
                const std::size_t offset = anchor.rva - kEz2dj4thTextRva;
                if (offset >= text.size())
                {
                    continue;
                }
                const re2dj::exe::PrologueSearchResult prologue =
                    re2dj::exe::FindPrologueBefore(text.data(),
                                                   text.size(),
                                                   offset,
                                                   kNullContextPrologueScanBack);
                // Two windows: one from the function start, and, when the
                // anchor sits beyond that window, one centered on the anchor so
                // the instruction of interest is never missing.
                const std::size_t anchor_start =
                    offset > kNullContextAnchorLeadBytes ? offset - kNullContextAnchorLeadBytes
                                                         : 0;
                const std::size_t window_start = prologue.found ? prologue.offset : offset;
                const std::size_t window_end =
                    (std::min)(text.size(), offset + kNullContextAnchorTrailBytes);
                const std::size_t window_size =
                    (std::min)(window_end - window_start, kNullContextCodeWindowBytes);
                std::string bytes_text(window_size * 2, '\0');
                for (std::size_t index = 0; index < window_size; ++index)
                {
                    std::snprintf(bytes_text.data() + index * 2,
                                  3,
                                  "%02x",
                                  text[window_start + index]);
                }
                RecordDiagnostic(
                    "{\"event\":\"null_context_code_region\",\"name\":\"%s\",\"anchor\":\"0x%08x\",\"anchor_rva\":\"0x%08x\",\"prologue_found\":%s,\"prologue_rva\":\"0x%08x\",\"prologue_distance\":%u,\"window_rva\":\"0x%08x\",\"window_bytes\":%u,\"truncated\":%s,\"bytes\":\"%s\"}",
                    anchor.name,
                    static_cast<unsigned>(text_base + offset),
                    anchor.rva,
                    prologue.found ? "true" : "false",
                    prologue.found
                        ? static_cast<unsigned>(kEz2dj4thTextRva + prologue.offset)
                        : 0u,
                    static_cast<unsigned>(prologue.distance),
                    static_cast<unsigned>(kEz2dj4thTextRva + window_start),
                    static_cast<unsigned>(window_size),
                    window_size < window_end - window_start ? "true" : "false",
                    bytes_text.c_str());
                if (prologue.found)
                {
                    // Callers usually reach a function through an incremental-
                    // link `jmp rel32` thunk, so resolve the branches to the
                    // function first and then the branches to each thunk found.
                    const std::uint32_t function_address =
                        static_cast<std::uint32_t>(text_base + prologue.offset);
                    bool direct_capped = false;
                    std::size_t direct_total = 0;
                    const std::vector<re2dj::exe::RelativeBranchSite> direct =
                        re2dj::exe::ScanRelativeBranches(text.data(),
                                                         text.size(),
                                                         static_cast<std::uint32_t>(text_base),
                                                         function_address,
                                                         kNullContextBranchSiteLimit,
                                                         &direct_capped,
                                                         &direct_total);
                    RecordDiagnostic(
                        "{\"event\":\"null_context_branch_scan\",\"name\":\"%s\",\"stage\":\"function\",\"target\":\"0x%08x\",\"target_rva\":\"0x%08x\",\"total\":%u,\"recorded\":%u,\"capped\":%s}",
                        anchor.name,
                        function_address,
                        static_cast<unsigned>(kEz2dj4thTextRva + prologue.offset),
                        static_cast<unsigned>(direct_total),
                        static_cast<unsigned>(direct.size()),
                        direct_capped ? "true" : "false");
                    for (const auto& site : direct)
                    {
                        RecordDiagnostic(
                            "{\"event\":\"null_context_branch_site\",\"name\":\"%s\",\"stage\":\"function\",\"site\":\"0x%08x\",\"site_rva\":\"0x%08x\",\"opcode\":\"0x%02x\",\"kind\":\"%s\"}",
                            anchor.name,
                            static_cast<unsigned>(text_base + site.offset),
                            static_cast<unsigned>(kEz2dj4thTextRva + site.offset),
                            static_cast<unsigned>(site.opcode),
                            site.opcode == 0xe9 ? "thunk" : "call");
                        if (site.opcode != 0xe9)
                        {
                            continue;
                        }
                        const std::uint32_t thunk_address =
                            static_cast<std::uint32_t>(text_base + site.offset);
                        bool thunk_capped = false;
                        std::size_t thunk_total = 0;
                        const std::vector<re2dj::exe::RelativeBranchSite> callers =
                            re2dj::exe::ScanRelativeBranches(text.data(),
                                                             text.size(),
                                                             static_cast<std::uint32_t>(text_base),
                                                             thunk_address,
                                                             kNullContextBranchSiteLimit,
                                                             &thunk_capped,
                                                             &thunk_total);
                        RecordDiagnostic(
                            "{\"event\":\"null_context_branch_scan\",\"name\":\"%s\",\"stage\":\"thunk\",\"target\":\"0x%08x\",\"target_rva\":\"0x%08x\",\"total\":%u,\"recorded\":%u,\"capped\":%s}",
                            anchor.name,
                            thunk_address,
                            static_cast<unsigned>(kEz2dj4thTextRva + site.offset),
                            static_cast<unsigned>(thunk_total),
                            static_cast<unsigned>(callers.size()),
                            thunk_capped ? "true" : "false");
                        for (const auto& caller : callers)
                        {
                            RecordDiagnostic(
                                "{\"event\":\"null_context_branch_site\",\"name\":\"%s\",\"stage\":\"thunk\",\"site\":\"0x%08x\",\"site_rva\":\"0x%08x\",\"opcode\":\"0x%02x\",\"kind\":\"%s\"}",
                                anchor.name,
                                static_cast<unsigned>(text_base + caller.offset),
                                static_cast<unsigned>(kEz2dj4thTextRva + caller.offset),
                                static_cast<unsigned>(caller.opcode),
                                caller.opcode == 0xe9 ? "thunk" : "call");
                        }
                    }
                }
                if (window_start + window_size <= offset)
                {
                    const std::size_t anchor_size =
                        (std::min)(text.size() - anchor_start,
                                   kNullContextAnchorLeadBytes + kNullContextAnchorTrailBytes);
                    std::string anchor_text(anchor_size * 2, '\0');
                    for (std::size_t index = 0; index < anchor_size; ++index)
                    {
                        std::snprintf(anchor_text.data() + index * 2,
                                      3,
                                      "%02x",
                                      text[anchor_start + index]);
                    }
                    RecordDiagnostic(
                        "{\"event\":\"null_context_code_region\",\"name\":\"%s_anchor\",\"anchor\":\"0x%08x\",\"anchor_rva\":\"0x%08x\",\"prologue_found\":%s,\"prologue_rva\":\"0x%08x\",\"prologue_distance\":%u,\"window_rva\":\"0x%08x\",\"window_bytes\":%u,\"truncated\":false,\"bytes\":\"%s\"}",
                        anchor.name,
                        static_cast<unsigned>(text_base + offset),
                        anchor.rva,
                        prologue.found ? "true" : "false",
                        prologue.found
                            ? static_cast<unsigned>(kEz2dj4thTextRva + prologue.offset)
                            : 0u,
                        static_cast<unsigned>(prologue.distance),
                        static_cast<unsigned>(kEz2dj4thTextRva + anchor_start),
                        static_cast<unsigned>(anchor_size),
                        anchor_text.c_str());
                }
            }
        }

        if (state->text_readable)
        {
            // The method that should reach the field initializer, listed as
            // branches so the early exit before its call site is visible
            // without decoding every instruction.
            const struct
            {
                const char* name;
                std::uint32_t rva;
                std::uint32_t length;
                std::uint32_t call_rva;
            } bodies[] = {
                {"vtable_slot2_method", 0x000116c8, 0x00000580, 0x00011c23},
                {"slot2_guard_thunk", 0x0000317f, 0x00000008, 0},
                {"guard2_target", 0x000106d2, 0x00000400, 0},
                {"slot2_error_thunk", 0x000038dc, 0x00000008, 0},
                // The thunk guard 1 calls, from the Task 169 branch listing of
                // the slot 2 method. Listing its eight bytes resolves the jump
                // to the function that actually fails.
                {"guard1_thunk", 0x00003913, 0x00000008, 0},
                // Where thunk 0x00003913 lands. This is the function whose
                // 0x81000004 return makes guard 1 exit.
                {"guard1_target", 0x0001010f, 0x00000400, 0},
                // The three helpers guard 1's target calls: two from inside its
                // loop and one at the loop tail. Listing their eight bytes
                // resolves each incremental-link thunk to a real function.
                {"guard1_helper_12a8", 0x000012a8, 0x00000008, 0},
                {"guard1_helper_2595", 0x00002595, 0x00000008, 0},
                {"guard1_helper_2b34", 0x00002b34, 0x00000008, 0},
                // What the loop compares with. Its shape says whether the
                // match is a string compare or something else.
                {"guard1_match_helper", 0x00012820, 0x00000100, 0},
            };
            for (const auto& body : bodies)
            {
                if (body.rva < kEz2dj4thTextRva)
                {
                    continue;
                }
                const std::size_t start = body.rva - kEz2dj4thTextRva;
                bool branch_capped = false;
                std::size_t branch_total = 0;
                const std::vector<re2dj::exe::NearBranch> branches =
                    re2dj::exe::ListNearBranches(text.data(),
                                                 text.size(),
                                                 static_cast<std::uint32_t>(text_base),
                                                 start,
                                                 body.length,
                                                 kNullContextBodyBranchLimit,
                                                 &branch_capped,
                                                 &branch_total);
                RecordDiagnostic(
                    "{\"event\":\"null_context_body_branch_scan\",\"name\":\"%s\",\"rva\":\"0x%08x\",\"length\":\"0x%08x\",\"call_rva\":\"0x%08x\",\"total\":%u,\"recorded\":%u,\"capped\":%s}",
                    body.name,
                    body.rva,
                    body.length,
                    body.call_rva,
                    static_cast<unsigned>(branch_total),
                    static_cast<unsigned>(branches.size()),
                    branch_capped ? "true" : "false");
                for (const auto& branch : branches)
                {
                    const std::uint32_t branch_rva =
                        static_cast<std::uint32_t>(kEz2dj4thTextRva + branch.offset);
                    const bool target_in_image = branch.target >= image_base &&
                                                 branch.target < image_end;
                    const std::uint32_t target_rva =
                        target_in_image
                            ? static_cast<std::uint32_t>(branch.target - image_base)
                            : 0;
                    // A forward branch from before the call site to after it is
                    // how the initializer would be skipped.
                    const bool skips_call = target_in_image && branch_rva < body.call_rva &&
                                            target_rva > body.call_rva;
                    RecordDiagnostic(
                        "{\"event\":\"null_context_body_branch\",\"name\":\"%s\",\"rva\":\"0x%08x\",\"opcode\":\"0x%02x\",\"near\":%s,\"target\":\"0x%08x\",\"target_rva\":\"0x%08x\",\"target_in_image\":%s,\"skips_call\":%s}",
                        body.name,
                        branch_rva,
                        static_cast<unsigned>(branch.opcode),
                        branch.near_form ? "true" : "false",
                        branch.target,
                        target_rva,
                        target_in_image ? "true" : "false",
                        skips_call ? "true" : "false");
                }
            }
        }

        // Read-only data the guest compares against. `.rdata` is encrypted on
        // disk exactly as `.text` is, so a named constant can only be read from
        // the child after the packer has unpacked the image.
        {
            const struct
            {
                const char* name;
                std::uint32_t rva;
                std::uint32_t length;
            } data_windows[] = {
                // The two operands the selection loop pushes at 0x000101b9 and
                // 0x00010201 before calling its comparison helper.
                {"guard1_match_string_0", 0x000e4da0, 32},
                {"guard1_match_string_1", 0x000e4dc0, 32},
                // The device table the selection loop walks. 0x000100ea hands
                // the loop the fixed base 0x00946d50 and the count held at
                // 0x0094cd9c, so both are globals with known RVAs. The record
                // stride is 0x4d0; +0x28 is the pointer the GUID comparison
                // takes, +0x118 selects which GUID, and +0x4c8 is the gate that
                // skipped every element.
                {"device_table_count", 0x0054cd9c, 4},
                {"device_record_0_head", 0x00546d50, 48},
                {"device_record_0_select", 0x00546e68, 16},
                {"device_record_0_gate", 0x00547218, 16},
                {"device_record_1_head", 0x00547220, 48},
                {"device_record_1_gate", 0x005476e8, 16},
                // The names the caller of the faulting function passes to the
                // singleton loader. Reading them says which resource the load
                // that left its out parameter empty was asked for.
                {"loader_names_0", 0x000f8800, 64},
                {"loader_names_1", 0x000f8840, 64},
                {"loader_names_2", 0x000f8880, 64},
            };
            for (const auto& window : data_windows)
            {
                const RemoteBufferSnapshot snapshot =
                    ReadRemoteBufferSnapshot(process, image_base + window.rva, window.length);
                std::string printable;
                printable.reserve(window.length);
                for (std::size_t index = 0; index + 1 < snapshot.bytes.size(); index += 2)
                {
                    const unsigned value = static_cast<unsigned>(
                        std::stoul(snapshot.bytes.substr(index, 2), nullptr, 16));
                    // Only bare printable ASCII, so nothing in the recorded
                    // text can close or escape the surrounding JSON string.
                    printable.push_back(value >= 0x20 && value < 0x7f && value != '"' && value != 0x5cu
                                            ? static_cast<char>(value)
                                            : '.');
                }
                RecordDiagnostic(
                    "{\"event\":\"null_context_data_window\",\"name\":\"%s\",\"rva\":\"0x%08x\",\"address\":\"0x%08x\",\"length\":%u,\"readable\":%s,\"bytes\":\"%s\",\"printable\":\"%s\"}",
                    window.name,
                    window.rva,
                    static_cast<unsigned>(image_base + window.rva),
                    static_cast<unsigned>(window.length),
                    snapshot.readable ? "true" : "false",
                    snapshot.bytes.c_str(),
                    printable.c_str());
            }
        }

        // Whole code ranges, chunked so one JSON line stays readable. The gate
        // the selection loop tests is copied from a context that looks like a
        // stack local, so its writer carries a folded displacement no scan can
        // find; only reading the range recovers the condition.
        if (state->text_readable)
        {
            constexpr std::uint32_t kCodeChunkBytes = 64;
            const struct
            {
                const char* name;
                std::uint32_t rva;
                std::uint32_t length;
            } code_ranges[] = {
                // The function that faults on a null object once the game
                // reaches panel-sprite loading, and the two frames above it
                // taken from the fault's recorded return addresses. The
                // incremental-link thunk table resolves the calls between them.
                {"av2_callee", 0x00012880, 0x00000100},
                {"av2_caller", 0x00038200, 0x00000220},
                {"av2_outer", 0x0003f780, 0x00000120},
                {"link_thunks", 0x00001840, 0x00000040},
            };
            for (const auto& range : code_ranges)
            {
                if (range.rva < kEz2dj4thTextRva)
                {
                    continue;
                }
                const std::size_t start = range.rva - kEz2dj4thTextRva;
                const std::size_t end =
                    (std::min)(text.size(), start + static_cast<std::size_t>(range.length));
                RecordDiagnostic(
                    "{\"event\":\"device_enum_code_range\",\"name\":\"%s\",\"rva\":\"0x%08x\",\"address\":\"0x%08x\",\"requested\":\"0x%08x\",\"available\":\"0x%08x\",\"chunk_bytes\":%u}",
                    range.name,
                    range.rva,
                    static_cast<unsigned>(text_base + start),
                    range.length,
                    static_cast<unsigned>(start < end ? end - start : 0),
                    kCodeChunkBytes);
                for (std::size_t offset = start; offset < end; offset += kCodeChunkBytes)
                {
                    const std::size_t chunk =
                        (std::min)(static_cast<std::size_t>(kCodeChunkBytes), end - offset);
                    std::string bytes_text(chunk * 2, '\0');
                    for (std::size_t index = 0; index < chunk; ++index)
                    {
                        std::snprintf(bytes_text.data() + index * 2,
                                      3,
                                      "%02x",
                                      text[offset + index]);
                    }
                    RecordDiagnostic(
                        "{\"event\":\"device_enum_code_chunk\",\"name\":\"%s\",\"rva\":\"0x%08x\",\"address\":\"0x%08x\",\"bytes\":%u,\"code\":\"%s\"}",
                        range.name,
                        static_cast<unsigned>(kEz2dj4thTextRva + offset),
                        static_cast<unsigned>(text_base + offset),
                        static_cast<unsigned>(chunk),
                        bytes_text.c_str());
                }
            }
        }

        // How much of a device record the enumeration callback actually filled.
        // Task 170 read only three small windows per record, which cannot say
        // whether the tail that holds the gate is filled at all.
        for (std::uint32_t record = 0; record < kEz2dj4thDeviceRecordWindowCount;
             ++record)
        {
            const std::uint32_t record_rva =
                kEz2dj4thDeviceRecordTableRva + record * kEz2dj4thDeviceRecordStride;
            const std::uintptr_t record_address = image_base + record_rva;
            const re2dj::tools::launcher_probe::ObjectWindowScan window =
                re2dj::tools::launcher_probe::ScanObjectWindow(
                    process,
                    record_address,
                    kEz2dj4thDeviceRecordStride,
                    kEz2dj4thDeviceRecordGateOffset,
                    re2dj::tools::launcher_probe::AddressRanges{image_base,
                                                                image_end,
                                                                0,
                                                                0},
                    kEz2dj4thDeviceRecordEntryLimit);
            RecordDiagnostic(
                "{\"event\":\"device_record_window\",\"record\":%u,\"address\":\"0x%08x\",\"rva\":\"0x%08x\",\"readable\":%s,\"bytes\":\"0x%08x\",\"nonzero\":%u,\"recorded\":%u,\"capped\":%s,\"gate_offset\":\"0x%08x\",\"gate_scanned\":%s,\"gate\":\"0x%08x\"}",
                static_cast<unsigned>(record),
                static_cast<unsigned>(record_address),
                record_rva,
                window.readable ? "true" : "false",
                window.bytes_scanned,
                window.nonzero_count,
                static_cast<unsigned>(window.entries.size()),
                window.capped ? "true" : "false",
                kEz2dj4thDeviceRecordGateOffset,
                window.field_offset_scanned ? "true" : "false",
                window.field_value);
            for (const auto& entry : window.entries)
            {
                RecordDiagnostic(
                    "{\"event\":\"device_record_field\",\"record\":%u,\"offset\":\"0x%08x\",\"value\":\"0x%08x\",\"classification\":\"%s\"}",
                    static_cast<unsigned>(record),
                    entry.offset,
                    entry.value,
                    entry.classification);
            }
        }

        RecordDiagnostic(
            "{\"event\":\"null_context_object_reference_scan\",\"text_base\":\"0x%08x\",\"text_rva\":\"0x%08x\",\"text_size\":\"0x%08x\",\"readable\":%s,\"matches\":%u,\"capped\":%s,\"frames\":%u}",
            static_cast<unsigned>(text_base),
            kEz2dj4thTextRva,
            kEz2dj4thTextVirtualSize,
            state->text_readable ? "true" : "false",
            state->match_count,
            state->capped ? "true" : "false",
            static_cast<unsigned>(frames.size()));
        state->scanned = true;
    }

    context.Dr7 &= ~kNullContextObjectSourceBoundaryMask;
    state->disarmed = true;
    context.Dr6 &= ~kNullContextObjectSourceBoundaryStatus;
    if (SetThreadContext(thread, &context) == FALSE)
    {
        CloseHandle(thread);
        *error = "cannot clear null-context object reference scan status";
        return false;
    }
    CloseHandle(thread);
    *handled = true;
    return true;
}

bool WaitForExitProcessBreakpoint(HANDLE process,
                                  std::uint32_t exit_target,
                                  bool bounded_api_trace,
                                  bool allocation_trace,
                                  bool slot_writer_trace,
                                  bool null_context_object_source_trace,
                                  bool null_context_field_access_trace,
                                  bool null_context_field_writer_trace,
                                  bool null_context_field_reference_execution_trace,
                                  bool null_context_object_state_trace,
                                  bool null_context_object_reference_scan,
                                  bool null_context_entry_trace,
                                  bool scan_fault_references,
                                  const std::vector<std::uint32_t>& field_reference_scan_constants,
                                  std::uintptr_t field_write_watch_address,
                                  const std::vector<GuestCodeWindowRequest>& code_windows,
                                  bool trace,
                                  std::uint32_t lptdi_post_ioctl_trace_steps,
                                  std::uint32_t lptdi_post_ioctl_trace_code,
                                  std::uint32_t diagnostic_idle_timeout_ms,
                                  std::uintptr_t image_base,
                                  std::uintptr_t object_source_boundary,
                                  const LegacyIoTrapPolicy& io_policy,
                                  const re2dj::exe::PeImageInfo* image_info,
                                  GuestReturnWatchMap* guest_return_watches,
                                  ApiWatchMap* api_watches,
                                  re2dj::input::LegacyIoPortBus* io_port_bus,
                                  std::string* error)
{
    std::map<DWORD, std::uintptr_t> pending_api_steps;
    PendingAllocationReturnMap pending_allocation_returns;
    std::map<DWORD, std::uintptr_t> pending_allocation_return_steps;
    AllocationReturnBreakpointMap allocation_return_breakpoints;
    std::map<DWORD, std::uintptr_t> pending_guest_return_steps;
    std::map<DWORD, PendingDeviceIoControl> pending_device_io_controls;
    std::map<DWORD, PostDeviceIoControlTrace> post_device_io_control_traces;
    SlotWriterTraceState slot_writer_state;
    AllocationTraceState allocation_trace_state;
    std::set<std::uint32_t> allocation_return_values;
    NullContextObjectSourceTraceState null_context_object_source_state;
    NullContextFieldWriterTraceState null_context_field_writer_state;
    NullContextFieldWriterTraceState null_context_field_access_state;
    NullContextFieldReferenceExecutionTraceState
        null_context_field_reference_execution_state;
    NullContextObjectStateTraceState null_context_object_state_state;
    NullContextObjectReferenceScanState null_context_object_reference_state;
    NullContextEntryTraceState null_context_entry_state;
    if (null_context_field_access_trace || null_context_field_writer_trace)
    {
        const std::uintptr_t field_address =
            image_base + kEz2dj4thNullContextFieldRva;
        std::uint32_t field_value = 0;
        SIZE_T field_copied = 0;
        const bool field_readable =
            ReadProcessMemory(process,
                              reinterpret_cast<const void*>(field_address),
                              &field_value,
                              sizeof(field_value),
                              &field_copied) != FALSE &&
            field_copied == sizeof(field_value);
        null_context_field_writer_state.last_field_value = field_value;
        null_context_field_writer_state.last_field_readable = field_readable;
        null_context_field_access_state.last_field_value = field_value;
        null_context_field_access_state.last_field_readable = field_readable;
    }
    std::set<std::uintptr_t> dynamic_module_bases;
    bool original_initializer_recorded = false;
    bool field_references_scanned = false;
    FieldWriteWatchState field_write_watch_state;
    bool code_windows_captured = false;
    bool unload_tail_collecting = false;
    DWORD unload_tail_thread = 0;
    std::vector<InstructionSample> unload_history;
    constexpr std::size_t kUnloadHistoryCapacity = 48;
    unload_history.reserve(kUnloadHistoryCapacity);
    constexpr std::uint32_t kUnloadStepCap = 200000;
    std::uint32_t unload_steps = 0;
    DWORD last_activity_thread = 0;
    // One-shot-at-a-time software breakpoints planted on detected syscall
    // stubs' return addresses so execution can be re-traced after WOW64
    // gates kill single-step reporting. Each consumed breakpoint re-arms the
    // detector so later stubs (a second gate crossing, for example) are also
    // caught, bounded by a fire budget.
    constexpr unsigned kMaxResumeBreakpointFires = 8;
    unsigned resume_bp_fires = 0;
    bool resume_bp_armed = false;
    std::uint32_t resume_bp_address = 0;
    std::uint8_t resume_bp_original = 0;
    std::map<std::pair<std::uintptr_t, std::uintptr_t>, std::string> symbol_cache;
    // Names one sampled or stacked address through its image allocation base
    // using the nearest export; private pages stay unnamed.
    auto format_remote_symbol = [&](std::uintptr_t address, std::string* out) {
        MEMORY_BASIC_INFORMATION region = {};
        if (VirtualQueryEx(process,
                           reinterpret_cast<const void*>(address),
                           &region,
                           sizeof(region)) != sizeof(region) ||
            region.Type != MEM_IMAGE ||
            region.AllocationBase == nullptr)
        {
            return false;
        }
        const std::uintptr_t module_base =
            reinterpret_cast<std::uintptr_t>(region.AllocationBase);
        const auto cached = symbol_cache.find({module_base, address});
        if (cached != symbol_cache.end())
        {
            *out = cached->second;
            return true;
        }
        re2dj::tools::windows_x86_launcher_probe::RemoteNearestExport nearest = {};
        std::string nearest_error;
        if (!re2dj::tools::windows_x86_launcher_probe::FindRemotePe32NearestExport(
                process,
                module_base,
                address,
                &nearest,
                &nearest_error))
        {
            return false;
        }
        char text[288] = {};
        std::snprintf(text,
                      sizeof(text),
                      "%s!%s+0x%x",
                      nearest.module,
                      nearest.function,
                      static_cast<unsigned>(nearest.offset));
        SanitizeJsonText(text);
        symbol_cache[{module_base, address}] = text;
        *out = text;
        return true;
    };
    auto annotate_sample_symbol = [&](InstructionSample& sample) {
        std::string text;
        if (format_remote_symbol(sample.address, &text))
        {
            sample.symbol = text;
        }
    };
    constexpr std::uint64_t kBoundedTraceEventCap = 1024;
    const std::uint64_t requested_event_cap = bounded_api_trace || allocation_trace || slot_writer_trace ||
                                                      null_context_object_source_trace ||
                                                      null_context_field_access_trace ||
                                                      null_context_field_writer_trace ||
                                                      null_context_field_reference_execution_trace ||
                                                      null_context_object_state_trace ||
         null_context_object_reference_scan ||
         null_context_entry_trace ||
                              null_context_object_reference_scan ||
                              null_context_entry_trace ||
                                                      null_context_object_reference_scan ||
                                                      null_context_entry_trace
                                                  ? kBoundedTraceEventCap
                                                  : (io_port_bus == nullptr ? 128ull : 8192ull) +
                                                        static_cast<std::uint64_t>(lptdi_post_ioctl_trace_steps) * 16ull;
    const std::uint32_t normal_event_cap = static_cast<std::uint32_t>(
        (std::min)(requested_event_cap,
                   static_cast<std::uint64_t>((std::numeric_limits<std::uint32_t>::max)())));
    std::uint32_t event_count = 0;
    for (; event_count < (unload_tail_collecting ? kUnloadStepCap : normal_event_cap);
         ++event_count)
    {
        DEBUG_EVENT event = {};
        if (WaitForDebugEvent(&event, diagnostic_idle_timeout_ms) == FALSE)
        {
            const DWORD wait_error = GetLastError();
            if ((allocation_trace || slot_writer_trace || null_context_object_source_trace ||
                 null_context_field_access_trace ||
                 null_context_field_writer_trace ||
                 null_context_field_reference_execution_trace ||
                 null_context_object_state_trace ||
                 null_context_object_reference_scan ||
                 null_context_entry_trace) &&
                wait_error == ERROR_SEM_TIMEOUT)
            {
                if (allocation_trace)
                {
                    RecordDiagnostic("{\"event\":\"null_context_allocation_trace_boundary\",\"reason\":\"idle_timeout\",\"events\":%u,\"armed\":%u,\"hits\":%u,\"recorded\":%u,\"pending\":%u,\"capped\":%s}",
                                     event_count,
                                     allocation_trace_state.armed_count,
                                     allocation_trace_state.hit_count,
                                     allocation_trace_state.recorded_count,
                                     static_cast<unsigned>(PendingAllocationReturnCount(
                                         pending_allocation_returns)),
                                     allocation_trace_state.capped ? "true" : "false");
                }
                if (slot_writer_trace)
                {
                    const std::uintptr_t slot_address =
                        image_base + kEz2dj4thPointerSlotRva;
                    std::uint32_t slot_value = 0;
                    SIZE_T copied = 0;
                    const bool slot_readable =
                        ReadProcessMemory(process,
                                          reinterpret_cast<const void*>(slot_address),
                                          &slot_value,
                                          sizeof(slot_value),
                                          &copied) != FALSE &&
                        copied == sizeof(slot_value);
                    RecordDiagnostic("{\"event\":\"slot_writer_trace_boundary\",\"reason\":\"idle_timeout\",\"events\":%u,\"hits\":%u,\"recorded\":%u,\"capped\":%s,\"slot\":\"0x%08x\",\"slot_readable\":%s,\"slot_current\":\"0x%08x\"}",
                                     event_count,
                                     slot_writer_state.hit_count,
                                     slot_writer_state.recorded_count,
                                     slot_writer_state.capped ? "true" : "false",
                                     static_cast<unsigned>(slot_address),
                                     slot_readable ? "true" : "false",
                                     slot_value);
                }
                if (null_context_object_source_trace)
                {
                    RecordDiagnostic(
                        "{\"event\":\"null_context_object_source_trace_boundary\",\"reason\":\"idle_timeout\",\"events\":%u,\"boundary_hits\":%u,\"boundary_recorded\":%u,\"boundary_capped\":%s,\"hits\":%u,\"recorded\":%u,\"target_matches\":%u,\"capped\":%s,\"boundary\":\"0x%08x\",\"dynamic_slots\":%u}",
                        event_count,
                        null_context_object_source_state.boundary_hit_count,
                        null_context_object_source_state.boundary_recorded_count,
                        null_context_object_source_state.boundary_capped ? "true" : "false",
                        null_context_object_source_state.hit_count,
                        null_context_object_source_state.recorded_count,
                        null_context_object_source_state.target_match_count,
                        null_context_object_source_state.capped ? "true" : "false",
                        static_cast<unsigned>(object_source_boundary),
                        static_cast<unsigned>(null_context_object_source_state.dynamic_stack_slots.size()));
                }
                if (null_context_field_access_trace)
                {
                    const std::uintptr_t field_address =
                        image_base + kEz2dj4thNullContextFieldRva;
                    std::uint32_t field_value = 0;
                    SIZE_T copied = 0;
                    const bool field_readable =
                        ReadProcessMemory(process,
                                          reinterpret_cast<const void*>(field_address),
                                          &field_value,
                                          sizeof(field_value),
                                          &copied) != FALSE &&
                        copied == sizeof(field_value);
                    RecordDiagnostic("{\"event\":\"null_context_field_access_trace_boundary\",\"reason\":\"idle_timeout\",\"events\":%u,\"hits\":%u,\"recorded\":%u,\"capped\":%s,\"field\":\"0x%08x\",\"field_readable\":%s,\"field_current\":\"0x%08x\"}",
                                     event_count,
                                     null_context_field_access_state.hit_count,
                                     null_context_field_access_state.recorded_count,
                                     null_context_field_access_state.capped ? "true" : "false",
                                     static_cast<unsigned>(field_address),
                                     field_readable ? "true" : "false",
                                     field_value);
                }
                if (null_context_field_writer_trace)
                {
                    const std::uintptr_t field_address =
                        image_base + kEz2dj4thNullContextFieldRva;
                    std::uint32_t field_value = 0;
                    SIZE_T copied = 0;
                    const bool field_readable =
                        ReadProcessMemory(process,
                                          reinterpret_cast<const void*>(field_address),
                                          &field_value,
                                          sizeof(field_value),
                                          &copied) != FALSE &&
                        copied == sizeof(field_value);
                    RecordDiagnostic("{\"event\":\"null_context_field_writer_trace_boundary\",\"reason\":\"idle_timeout\",\"events\":%u,\"hits\":%u,\"recorded\":%u,\"capped\":%s,\"field\":\"0x%08x\",\"field_readable\":%s,\"field_current\":\"0x%08x\"}",
                                     event_count,
                                     null_context_field_writer_state.hit_count,
                                     null_context_field_writer_state.recorded_count,
                                     null_context_field_writer_state.capped ? "true" : "false",
                                     static_cast<unsigned>(field_address),
                                     field_readable ? "true" : "false",
                                     field_value);
                }
                if (null_context_field_reference_execution_trace)
                {
                    RecordDiagnostic(
                        "{\"event\":\"null_context_field_reference_execution_trace_boundary\",\"reason\":\"idle_timeout\",\"events\":%u,\"hits\":%u,\"recorded\":%u,\"target_matches\":%u,\"pending\":%u,\"capped\":%s}",
                        event_count,
                        null_context_field_reference_execution_state.hit_count,
                        null_context_field_reference_execution_state.recorded_count,
                        null_context_field_reference_execution_state.target_match_count,
                        static_cast<unsigned>(null_context_field_reference_execution_state
                                                  .pending_single_step_threads.size()),
                        null_context_field_reference_execution_state.capped ? "true" : "false");
                }
                if (null_context_object_state_trace)
                {
                    RecordDiagnostic(
                        "{\"event\":\"null_context_object_state_trace_boundary\",\"reason\":\"idle_timeout\",\"events\":%u,\"hits\":%u,\"recorded\":%u,\"frames\":%u,\"window_readable\":%s,\"disarmed\":%s}",
                        event_count,
                        null_context_object_state_state.hit_count,
                        null_context_object_state_state.recorded_count,
                        null_context_object_state_state.frame_count,
                        null_context_object_state_state.window_readable ? "true" : "false",
                        null_context_object_state_state.disarmed ? "true" : "false");
                }
                if (null_context_object_reference_scan)
                {
                    RecordDiagnostic(
                        "{\"event\":\"null_context_object_reference_scan_boundary\",\"reason\":\"idle_timeout\",\"events\":%u,\"hits\":%u,\"scanned\":%s,\"text_readable\":%s,\"matches\":%u,\"capped\":%s}",
                        event_count,
                        null_context_object_reference_state.hit_count,
                        null_context_object_reference_state.scanned ? "true" : "false",
                        null_context_object_reference_state.text_readable ? "true" : "false",
                        null_context_object_reference_state.match_count,
                        null_context_object_reference_state.capped ? "true" : "false");
                }
                if (null_context_entry_trace)
                {
                    RecordDiagnostic(
                        "{\"event\":\"null_context_entry_trace_boundary\",\"reason\":\"idle_timeout\",\"events\":%u,\"hits\":%u,\"recorded\":%u,\"singleton_receivers\":%u,\"pending\":%u,\"capped\":%s}",
                        event_count,
                        null_context_entry_state.hit_count,
                        null_context_entry_state.recorded_count,
                        null_context_entry_state.singleton_receiver_count,
                        static_cast<unsigned>(null_context_entry_state.pending_single_step_threads.size()),
                        null_context_entry_state.capped ? "true" : "false");
                }
                return true;
            }
            *error = "cannot wait for ExitProcess breakpoint";
            return false;
        }
        const bool suppress_io_event =
            !trace && io_port_bus != nullptr &&
            event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT &&
            event.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_PRIV_INSTRUCTION;
        if (!suppress_io_event)
        {
            TraceDebugEvent(event);
        }
        std::string debug_message;
        RecordAnsiOutputDebugString(process, event, &debug_message);
        if (event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT &&
            event.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_PRIV_INSTRUCTION)
        {
            RecordPrivilegedInstructionContext(
                process,
                event.dwThreadId,
                event.u.Exception.ExceptionRecord,
                event.u.Exception.dwFirstChance != 0);
        }
        if (field_write_watch_address != 0 &&
            event.dwDebugEventCode == CREATE_THREAD_DEBUG_EVENT)
        {
            // Debug registers are per thread, so a thread armed late writes the
            // field unobserved.
            if (!SetFieldWriteWatch(event.u.CreateThread.hThread,
                                    field_write_watch_address,
                                    error))
            {
                return false;
            }
            RecordDiagnostic(
                "{\"event\":\"field_write_watch_thread_armed\",\"thread\":%u,\"address\":\"0x%08x\"}",
                static_cast<unsigned>(event.dwThreadId),
                static_cast<unsigned>(field_write_watch_address));
        }
        if ((allocation_trace || slot_writer_trace || null_context_object_source_trace ||
             null_context_field_access_trace ||
             null_context_field_writer_trace ||
             null_context_field_reference_execution_trace ||
             null_context_object_state_trace ||
             null_context_object_reference_scan ||
             null_context_entry_trace) &&
            event.dwDebugEventCode == CREATE_THREAD_DEBUG_EVENT)
        {
            bool armed = true;
            if (null_context_field_reference_execution_trace)
            {
                armed = SetNullContextFieldReferenceExecutionBreakpoints(
                    event.u.CreateThread.hThread,
                    image_base,
                    error);
            }
            if (slot_writer_trace)
            {
                armed = SetSlotWriterBreakpoints(event.u.CreateThread.hThread,
                                                 image_base,
                                                 error);
            }
            if (armed && null_context_object_source_trace)
            {
                armed = SetNullContextObjectSourceBoundaryBreakpoint(
                    event.u.CreateThread.hThread,
                    object_source_boundary,
                    error);
            }
            if (armed && null_context_object_state_trace &&
                !null_context_object_state_state.disarmed)
            {
                armed = SetNullContextObjectSourceBoundaryBreakpoint(
                    event.u.CreateThread.hThread,
                    image_base + kEz2dj4thNullContextObjectSourceBoundaryRva,
                    error);
            }
            if (armed && null_context_object_reference_scan &&
                !null_context_object_reference_state.disarmed)
            {
                armed = SetNullContextObjectSourceBoundaryBreakpoint(
                    event.u.CreateThread.hThread,
                    image_base + kEz2dj4thNullContextObjectSourceBoundaryRva,
                    error);
            }
            if (armed && null_context_entry_trace)
            {
                armed = SetNullContextEntryBreakpoints(event.u.CreateThread.hThread,
                                                       image_base,
                                                       error);
            }
            if (armed && (null_context_field_access_trace ||
                          null_context_field_writer_trace))
            {
                const DWORD breakpoint_control =
                    null_context_field_access_trace
                        ? kNullContextFieldAccessBreakpointControl
                        : kNullContextFieldWriteBreakpointControl;
                armed = SetNullContextFieldAccessBreakpoint(
                    event.u.CreateThread.hThread,
                    image_base,
                    breakpoint_control,
                    error);
            }
            CloseHandle(event.u.CreateThread.hThread);
            if (!armed)
            {
                return false;
            }
            if (slot_writer_trace)
            {
                RecordDiagnostic("{\"event\":\"slot_writer_thread_armed\",\"thread\":%u}",
                                 static_cast<unsigned>(event.dwThreadId));
            }
            if (null_context_object_source_trace)
            {
                RecordDiagnostic(
                    "{\"event\":\"null_context_object_source_thread_armed\",\"thread\":%u,\"boundary\":\"0x%08x\"}",
                    static_cast<unsigned>(event.dwThreadId),
                    static_cast<unsigned>(object_source_boundary));
            }
            if (null_context_field_access_trace)
            {
                RecordDiagnostic("{\"event\":\"null_context_field_access_thread_armed\",\"thread\":%u}",
                                 static_cast<unsigned>(event.dwThreadId));
            }
            if (null_context_field_writer_trace)
            {
                RecordDiagnostic("{\"event\":\"null_context_field_writer_thread_armed\",\"thread\":%u}",
                                 static_cast<unsigned>(event.dwThreadId));
            }
            if (null_context_field_reference_execution_trace)
            {
                RecordDiagnostic(
                    "{\"event\":\"null_context_field_reference_execution_thread_armed\",\"thread\":%u}",
                    static_cast<unsigned>(event.dwThreadId));
            }
            if (null_context_object_state_trace &&
                !null_context_object_state_state.disarmed)
            {
                RecordDiagnostic(
                    "{\"event\":\"null_context_object_state_thread_armed\",\"thread\":%u}",
                    static_cast<unsigned>(event.dwThreadId));
            }
            if (null_context_entry_trace)
            {
                RecordDiagnostic(
                    "{\"event\":\"null_context_entry_thread_armed\",\"thread\":%u}",
                    static_cast<unsigned>(event.dwThreadId));
            }
        }
        if (event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT)
        {
            const IoPortTrapResult io_result = HandleLegacyIoPortTrap(
                process,
                event.dwThreadId,
                event.u.Exception.ExceptionRecord,
                image_base,
                io_policy,
                io_port_bus,
                trace,
                error);
            if (io_result == IoPortTrapResult::kError)
            {
                return false;
            }
            if (io_result == IoPortTrapResult::kHandled)
            {
                if (ContinueDebugEvent(event.dwProcessId,
                                       event.dwThreadId,
                                       DBG_CONTINUE) == FALSE)
                {
                    *error = "cannot continue handled legacy I/O port trap";
                    return false;
                }
                continue;
            }
        }
        if (null_context_field_reference_execution_trace &&
            event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT &&
            event.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_SINGLE_STEP)
        {
            bool handled = false;
            if (!HandleNullContextFieldReferenceExecutionBreakpoint(
                    process,
                    event.dwThreadId,
                    image_base,
                    &null_context_field_reference_execution_state,
                    &handled,
                    error))
            {
                return false;
            }
            if (handled)
            {
                if (ContinueDebugEvent(event.dwProcessId,
                                       event.dwThreadId,
                                       DBG_CONTINUE) == FALSE)
                {
                    *error =
                        "cannot continue null-context field reference execution trace";
                    return false;
                }
                continue;
            }
        }
        if (null_context_object_reference_scan &&
            event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT &&
            event.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_SINGLE_STEP)
        {
            bool handled = false;
            if (!HandleNullContextObjectReferenceScanBreakpoint(
                    process,
                    event.dwThreadId,
                    image_base,
                    image_info != nullptr
                        ? image_base +
                              static_cast<std::uintptr_t>(image_info->size_of_image)
                        : image_base,
                    &null_context_object_reference_state,
                    &handled,
                    error))
            {
                return false;
            }
            if (handled)
            {
                if (ContinueDebugEvent(event.dwProcessId,
                                       event.dwThreadId,
                                       DBG_CONTINUE) == FALSE)
                {
                    *error = "cannot continue null-context object reference scan hit";
                    return false;
                }
                continue;
            }
        }
        if (null_context_entry_trace &&
            event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT &&
            event.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_SINGLE_STEP)
        {
            bool handled = false;
            if (!HandleNullContextEntryBreakpoint(process,
                                                  event.dwThreadId,
                                                  image_base,
                                                  &null_context_entry_state,
                                                  &handled,
                                                  error))
            {
                return false;
            }
            if (handled)
            {
                if (ContinueDebugEvent(event.dwProcessId,
                                       event.dwThreadId,
                                       DBG_CONTINUE) == FALSE)
                {
                    *error = "cannot continue null-context entry breakpoint hit";
                    return false;
                }
                continue;
            }
        }
        if (null_context_object_state_trace &&
            event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT &&
            event.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_SINGLE_STEP)
        {
            bool handled = false;
            if (!HandleNullContextObjectStateBreakpoint(
                    process,
                    event.dwThreadId,
                    image_base,
                    image_info != nullptr
                        ? image_base +
                              static_cast<std::uintptr_t>(image_info->size_of_image)
                        : image_base,
                    &null_context_object_state_state,
                    &handled,
                    error))
            {
                return false;
            }
            if (handled)
            {
                if (ContinueDebugEvent(event.dwProcessId,
                                       event.dwThreadId,
                                       DBG_CONTINUE) == FALSE)
                {
                    *error = "cannot continue null-context object state breakpoint hit";
                    return false;
                }
                continue;
            }
        }
        if (slot_writer_trace &&
            event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT &&
            event.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_SINGLE_STEP)
        {
            bool handled = false;
            if (!HandleSlotWriterBreakpoint(process,
                                            event.dwThreadId,
                                            image_base,
                                            &slot_writer_state,
                                            &handled,
                                            error))
            {
                return false;
            }
            if (handled)
            {
                if (ContinueDebugEvent(event.dwProcessId,
                                       event.dwThreadId,
                                       DBG_CONTINUE) == FALSE)
                {
                    *error = "cannot continue slot-writer breakpoint hit";
                    return false;
                }
                continue;
            }
        }
        if (null_context_object_source_trace &&
            event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT &&
            event.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_SINGLE_STEP)
        {
            bool handled = false;
            if (!HandleNullContextObjectSourceBoundaryBreakpoint(
                    process,
                    event.dwThreadId,
                    image_base,
                    object_source_boundary,
                    &null_context_object_source_state,
                    &handled,
                    error))
            {
                return false;
            }
            if (handled)
            {
                if (ContinueDebugEvent(event.dwProcessId,
                                       event.dwThreadId,
                                       DBG_CONTINUE) == FALSE)
                {
                    *error = "cannot continue null-context object source breakpoint hit";
                    return false;
                }
                continue;
            }
        }
        if (null_context_object_source_trace &&
            event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT &&
            event.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_SINGLE_STEP)
        {
            const auto dynamic_slot =
                null_context_object_source_state.dynamic_stack_slots.find(
                    event.dwThreadId);
            if (dynamic_slot != null_context_object_source_state.dynamic_stack_slots.end())
            {
                bool handled = false;
                if (!HandleNullContextObjectSourceBreakpoint(
                        process,
                        event.dwThreadId,
                        image_base,
                        dynamic_slot->second,
                        &null_context_object_source_state,
                        &handled,
                        error))
                {
                    return false;
                }
                if (handled)
                {
                    if (ContinueDebugEvent(event.dwProcessId,
                                           event.dwThreadId,
                                           DBG_CONTINUE) == FALSE)
                    {
                        *error = "cannot continue null-context object source breakpoint hit";
                        return false;
                    }
                    continue;
                }
            }
        }
        if (null_context_field_writer_trace &&
            event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT &&
            event.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_SINGLE_STEP)
        {
            bool handled = false;
            if (!HandleNullContextFieldAccessBreakpoint(
                    process,
                    event.dwThreadId,
                    image_base,
                    &null_context_field_writer_state,
                    "null_context_field_writer_hit",
                    allocation_trace ? &allocation_return_values : nullptr,
                    &handled,
                    error))
            {
                return false;
            }
            if (handled)
            {
                if (ContinueDebugEvent(event.dwProcessId,
                                       event.dwThreadId,
                                       DBG_CONTINUE) == FALSE)
                {
                    *error = "cannot continue null-context field breakpoint hit";
                    return false;
                }
                continue;
            }
        }
        if (null_context_field_access_trace &&
            event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT &&
            event.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_SINGLE_STEP)
        {
            bool handled = false;
            if (!HandleNullContextFieldAccessBreakpoint(
                    process,
                    event.dwThreadId,
                    image_base,
                    &null_context_field_access_state,
                    "null_context_field_access_hit",
                    allocation_trace ? &allocation_return_values : nullptr,
                    &handled,
                    error))
            {
                return false;
            }
            if (handled)
            {
                if (ContinueDebugEvent(event.dwProcessId,
                                       event.dwThreadId,
                                       DBG_CONTINUE) == FALSE)
                {
                    *error = "cannot continue null-context field access hit";
                    return false;
                }
                continue;
            }
        }
        if (event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT && api_watches != nullptr)
        {
            last_activity_thread = event.dwThreadId;
            const DWORD exception_code = event.u.Exception.ExceptionRecord.ExceptionCode;
            const std::uintptr_t exception_address =
                reinterpret_cast<std::uintptr_t>(
                    event.u.Exception.ExceptionRecord.ExceptionAddress);
            if (exception_code == EXCEPTION_SINGLE_STEP)
            {
                const auto pending_allocation_step =
                    pending_allocation_return_steps.find(event.dwThreadId);
                if (allocation_trace &&
                    pending_allocation_step != pending_allocation_return_steps.end())
                {
                    bool rearmed = false;
                    const auto return_breakpoint = allocation_return_breakpoints.find(
                        pending_allocation_step->second);
                    HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT,
                                               FALSE,
                                               event.dwThreadId);
                    CONTEXT context = {};
                    context.ContextFlags = CONTEXT_CONTROL;
                    if (thread != nullptr && GetThreadContext(thread, &context) != FALSE &&
                        return_breakpoint != allocation_return_breakpoints.end())
                    {
                        const std::uint8_t breakpoint = 0xcc;
                        SIZE_T written = 0;
                        rearmed = WriteProcessMemory(
                                      process,
                                      reinterpret_cast<void*>(pending_allocation_step->second),
                                      &breakpoint,
                                      sizeof(breakpoint),
                                      &written) != FALSE &&
                                  written == sizeof(breakpoint) &&
                                  FlushInstructionCache(
                                      process,
                                      reinterpret_cast<const void*>(pending_allocation_step->second),
                                      sizeof(breakpoint)) != FALSE;
                        context.EFlags &= ~0x100u;
                        rearmed = rearmed && SetThreadContext(thread, &context) != FALSE;
                    }
                    if (thread != nullptr)
                    {
                        CloseHandle(thread);
                    }
                    pending_allocation_return_steps.erase(pending_allocation_step);
                    if (!rearmed ||
                        ContinueDebugEvent(event.dwProcessId,
                                           event.dwThreadId,
                                           DBG_CONTINUE) == FALSE)
                    {
                        *error = "cannot rearm allocator return breakpoint";
                        return false;
                    }
                    continue;
                }
                const auto pending_guest = pending_guest_return_steps.find(event.dwThreadId);
                if (pending_guest != pending_guest_return_steps.end())
                {
                    bool rearmed = false;
                    const auto watch = guest_return_watches->find(pending_guest->second);
                    if (watch != guest_return_watches->end())
                    {
                        const std::uint8_t breakpoint = 0xcc;
                        SIZE_T written = 0;
                        rearmed = WriteProcessMemory(
                                      process,
                                      reinterpret_cast<void*>(pending_guest->second),
                                      &breakpoint,
                                      sizeof(breakpoint),
                                      &written) != FALSE &&
                                  written == sizeof(breakpoint) &&
                                  FlushInstructionCache(
                                      process,
                                      reinterpret_cast<const void*>(pending_guest->second),
                                      sizeof(breakpoint)) != FALSE;
                    }
                    pending_guest_return_steps.erase(pending_guest);
                    if (!rearmed ||
                        ContinueDebugEvent(event.dwProcessId,
                                           event.dwThreadId,
                                           DBG_CONTINUE) == FALSE)
                    {
                        *error = "cannot rearm guest return breakpoint";
                        return false;
                    }
                    continue;
                }
                const auto pending = pending_api_steps.find(event.dwThreadId);
                if (pending != pending_api_steps.end())
                {
                    bool rearmed = false;
                    const auto watch = api_watches->find(pending->second);
                    if (watch != api_watches->end())
                    {
                        const std::uint8_t breakpoint = 0xcc;
                        SIZE_T written = 0;
                        rearmed = WriteProcessMemory(
                                      process,
                                      reinterpret_cast<void*>(pending->second),
                                      &breakpoint,
                                      sizeof(breakpoint),
                                      &written) != FALSE &&
                                  written == sizeof(breakpoint) &&
                                  FlushInstructionCache(
                                      process,
                                      reinterpret_cast<const void*>(pending->second),
                                      sizeof(breakpoint)) != FALSE;
                    }
                    pending_api_steps.erase(pending);
                    if (!rearmed)
                    {
                        *error = "cannot rearm watched API breakpoint";
                        return false;
                    }
                    const auto post_trace =
                        post_device_io_control_traces.find(event.dwThreadId);
                    if (post_trace != post_device_io_control_traces.end() &&
                        !post_trace->second.waiting_for_resume)
                    {
                        RecordDiagnostic("{\"event\":\"lptdi_post_ioctl_trace_end\",\"thread\":%u,\"code\":\"0x%08x\",\"samples\":%u,\"reason\":\"api_watch\"}",
                                         static_cast<unsigned>(event.dwThreadId),
                                         post_trace->second.code,
                                         post_trace->second.sequence);
                        post_device_io_control_traces.erase(post_trace);
                    }
                    if (ContinueDebugEvent(event.dwProcessId,
                                           event.dwThreadId,
                                           DBG_CONTINUE) == FALSE)
                    {
                        *error = "cannot continue watched API rearm step";
                        return false;
                    }
                    continue;
                }
                const auto post_trace =
                    post_device_io_control_traces.find(event.dwThreadId);
                if (post_trace != post_device_io_control_traces.end() &&
                    !post_trace->second.waiting_for_resume)
                {
                    HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT,
                                               FALSE,
                                               event.dwThreadId);
                    CONTEXT context = {};
                    context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
                    if (thread == nullptr || GetThreadContext(thread, &context) == FALSE)
                    {
                        if (thread != nullptr)
                        {
                            CloseHandle(thread);
                        }
                        *error = "cannot capture LPTDI post-IOCTL trace context";
                        return false;
                    }
                    MEMORY_BASIC_INFORMATION region = {};
                    const bool same_allocation =
                        VirtualQueryEx(process,
                                       reinterpret_cast<const void*>(context.Eip),
                                       &region,
                                       sizeof(region)) == sizeof(region) &&
                        reinterpret_cast<std::uintptr_t>(region.AllocationBase) ==
                            post_trace->second.allocation_base;
                    if (!same_allocation)
                    {
                        std::uint32_t resume_address = 0;
                        const PostDeviceIoControlTrace& state = post_trace->second;
                        if (state.last_byte_count >= 5 && state.last_bytes[0] == 0xe8)
                        {
                            resume_address = state.last_address + 5;
                        }
                        else if (state.last_byte_count >= 6 && state.last_bytes[0] == 0xff &&
                                 state.last_bytes[1] == 0x15)
                        {
                            resume_address = state.last_address + 6;
                        }
                        else if (state.last_byte_count >= 2 && state.last_bytes[0] == 0xff &&
                                 (state.last_bytes[1] & 0xf8) == 0xd0)
                        {
                            resume_address = state.last_address + 2;
                        }
                        if (resume_address != 0 &&
                            SetSoftwareEntryBreakpoint(process,
                                                       resume_address,
                                                       &post_trace->second.resume_original_byte,
                                                       error))
                        {
                            context.EFlags &= ~0x100u;
                            if (SetThreadContext(thread, &context) == FALSE)
                            {
                                CloseHandle(thread);
                                *error = "cannot suspend LPTDI post-IOCTL single step";
                                return false;
                            }
                            if (post_trace->second.resume_original_byte == 0xcc)
                            {
                                if (resume_bp_armed && resume_address == resume_bp_address)
                                {
                                    post_trace->second.resume_original_byte =
                                        resume_bp_original;
                                    post_trace->second.waiting_for_resume = true;
                                    post_trace->second.resume_address = resume_address;
                                    resume_bp_armed = false;
                                    ++resume_bp_fires;
                                    RecordDiagnostic("{\"event\":\"lptdi_post_ioctl_trace_suspend\",\"thread\":%u,\"code\":\"0x%08x\",\"samples\":%u,\"callee\":\"0x%08x\",\"resume\":\"0x%08x\",\"shared_syscall_breakpoint\":true}",
                                                     static_cast<unsigned>(event.dwThreadId),
                                                     post_trace->second.code,
                                                     post_trace->second.sequence,
                                                     static_cast<unsigned>(context.Eip),
                                                     resume_address);
                                }
                                else
                                {
                                    RecordDiagnostic("{\"event\":\"lptdi_post_ioctl_trace_end\",\"thread\":%u,\"code\":\"0x%08x\",\"samples\":%u,\"reason\":\"resume_breakpoint_collision\",\"address\":\"0x%08x\"}",
                                                     static_cast<unsigned>(event.dwThreadId),
                                                     post_trace->second.code,
                                                     post_trace->second.sequence,
                                                     resume_address);
                                    post_device_io_control_traces.erase(post_trace);
                                }
                            }
                            else
                            {
                                post_trace->second.waiting_for_resume = true;
                                post_trace->second.resume_address = resume_address;
                                RecordDiagnostic("{\"event\":\"lptdi_post_ioctl_trace_suspend\",\"thread\":%u,\"code\":\"0x%08x\",\"samples\":%u,\"callee\":\"0x%08x\",\"resume\":\"0x%08x\"}",
                                                 static_cast<unsigned>(event.dwThreadId),
                                                 post_trace->second.code,
                                                 post_trace->second.sequence,
                                                 static_cast<unsigned>(context.Eip),
                                                 resume_address);
                            }
                        }
                        else
                        {
                            context.EFlags &= ~0x100u;
                            if (SetThreadContext(thread, &context) == FALSE)
                            {
                                CloseHandle(thread);
                                *error = "cannot stop LPTDI post-IOCTL single step";
                                return false;
                            }
                            RecordDiagnostic("{\"event\":\"lptdi_post_ioctl_trace_end\",\"thread\":%u,\"code\":\"0x%08x\",\"samples\":%u,\"reason\":\"allocation_exit\",\"address\":\"0x%08x\"}",
                                             static_cast<unsigned>(event.dwThreadId),
                                             post_trace->second.code,
                                             post_trace->second.sequence,
                                             static_cast<unsigned>(context.Eip));
                            error->clear();
                            post_device_io_control_traces.erase(post_trace);
                        }
                    }
                    else
                    {
                        RecordPostDeviceIoControlSample(process,
                                                        event.dwThreadId,
                                                        post_trace->second,
                                                        context);
                        post_trace->second.last_address = context.Eip;
                        post_trace->second.last_byte_count = 0;
                        ReadProcessMemory(process,
                                          reinterpret_cast<const void*>(context.Eip),
                                          post_trace->second.last_bytes.data(),
                                          post_trace->second.last_bytes.size(),
                                          &post_trace->second.last_byte_count);
                        ++post_trace->second.sequence;
                        --post_trace->second.remaining;
                        if (post_trace->second.remaining == 0)
                        {
                            RecordDiagnostic("{\"event\":\"lptdi_post_ioctl_trace_end\",\"thread\":%u,\"code\":\"0x%08x\",\"samples\":%u,\"reason\":\"step_limit\"}",
                                             static_cast<unsigned>(event.dwThreadId),
                                             post_trace->second.code,
                                             post_trace->second.sequence);
                            post_device_io_control_traces.erase(post_trace);
                        }
                        else
                        {
                            context.EFlags |= 0x100;
                            if (SetThreadContext(thread, &context) == FALSE)
                            {
                                CloseHandle(thread);
                                *error = "cannot rearm LPTDI post-IOCTL single step";
                                return false;
                            }
                        }
                    }
                    CloseHandle(thread);
                    if (ContinueDebugEvent(event.dwProcessId,
                                           event.dwThreadId,
                                           DBG_CONTINUE) == FALSE)
                    {
                        *error = "cannot continue LPTDI post-IOCTL single step";
                        return false;
                    }
                    continue;
                }
                if (unload_tail_collecting && event.dwThreadId == unload_tail_thread)
                {
                    InstructionSample sample;
                    sample.address = static_cast<std::uint32_t>(exception_address);
                    ReadProcessMemory(process,
                                      reinterpret_cast<const void*>(sample.address),
                                      sample.bytes.data(),
                                      sample.bytes.size(),
                                      &sample.byte_count);
                    // One thread round trip captures the register trail and
                    // re-arms the trap flag for the next step.
                    HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT,
                                               FALSE,
                                               event.dwThreadId);
                    CONTEXT context = {};
                    context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
                    const bool have_context =
                        thread != nullptr && GetThreadContext(thread, &context) != FALSE;
                    if (have_context)
                    {
                        sample.has_regs = true;
                        sample.regs[0] = context.Eax;
                        sample.regs[1] = context.Ebx;
                        sample.regs[2] = context.Ecx;
                        sample.regs[3] = context.Edx;
                        sample.regs[4] = context.Esi;
                        sample.regs[5] = context.Edi;
                        sample.regs[6] = context.Ebp;
                        sample.regs[7] = context.Esp;
                        context.EFlags |= 0x100;
                        if (SetThreadContext(thread, &context) == FALSE)
                        {
                            *error = "cannot rearm unload-tail single step";
                            CloseHandle(thread);
                            return false;
                        }
                    }
                    if (thread != nullptr)
                    {
                        CloseHandle(thread);
                    }
                    annotate_sample_symbol(sample);
                    // Detect the standard syscall stanza tail - mov edx,
                    // imm32 followed by call edx - and plant a one-shot
                    // breakpoint on its return address so the 32-bit resume
                    // after the WOW64 gate can be caught and re-traced.
                    const InstructionSample* previous =
                        unload_history.empty() ? nullptr : &unload_history.back();
                    if (!resume_bp_armed && resume_bp_fires < kMaxResumeBreakpointFires &&
                        previous != nullptr &&
                        sample.byte_count >= 2 && sample.bytes[0] == 0xff &&
                        sample.bytes[1] == 0xd2 &&
                        static_cast<std::uintptr_t>(previous->address) + 5 ==
                            sample.address &&
                        previous->byte_count >= 1 && previous->bytes[0] == 0xba)
                    {
                        std::uint8_t original = 0;
                        if (SetSoftwareEntryBreakpoint(process,
                                                       sample.address + 2,
                                                       &original,
                                                       error))
                        {
                            resume_bp_armed = true;
                            resume_bp_address = sample.address + 2;
                            resume_bp_original = original;
                            RecordDiagnostic("{\"event\":\"resume_breakpoint_arm\",\"stub\":\"0x%08x\",\"return\":\"0x%08x\"}",
                                             sample.address,
                                             resume_bp_address);
                        }
                        else
                        {
                            RecordDiagnostic("{\"event\":\"resume_breakpoint_arm\",\"status\":\"failed\",\"reason\":\"%s\"}",
                                             error->c_str());
                            error->clear();
                        }
                    }
                    if (unload_history.size() == kUnloadHistoryCapacity)
                    {
                        unload_history.erase(unload_history.begin());
                    }
                    unload_history.push_back(std::move(sample));
                    ++unload_steps;
                    if (unload_steps == kUnloadStepCap)
                    {
                        RecordInstructionHistory(unload_history, unload_steps);
                        *error = "unload-tail single-step limit reached before the fault";
                        return false;
                    }
                    if (!have_context ||
                        ContinueDebugEvent(event.dwProcessId,
                                           event.dwThreadId,
                                           DBG_CONTINUE) == FALSE)
                    {
                        if (error->empty())
                        {
                            *error = "cannot continue unload-tail single step";
                        }
                        return false;
                    }
                    continue;
                }
            }
            else if (exception_code == EXCEPTION_BREAKPOINT)
            {
                auto allocation_thread =
                    pending_allocation_returns.find(event.dwThreadId);
                auto allocation_return =
                    allocation_thread == pending_allocation_returns.end()
                        ? std::vector<PendingAllocationReturn>::iterator{}
                        : allocation_thread->second.end();
                if (allocation_thread != pending_allocation_returns.end())
                {
                    for (auto candidate = allocation_thread->second.begin();
                         candidate != allocation_thread->second.end();
                         ++candidate)
                    {
                        if (candidate->return_address == exception_address)
                        {
                            allocation_return = candidate;
                            break;
                        }
                    }
                }
                if (allocation_trace && allocation_thread != pending_allocation_returns.end() &&
                    allocation_return != allocation_thread->second.end())
                {
                    const PendingAllocationReturn pending = *allocation_return;
                    HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT,
                                               FALSE,
                                               event.dwThreadId);
                    CONTEXT context = {};
                    context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
                    bool captured = thread != nullptr &&
                                    GetThreadContext(thread, &context) != FALSE;
                    bool restored_return = false;
                    bool rearmed_api = false;
                    bool shared_return_breakpoint = false;
                    if (captured)
                    {
                        const auto return_breakpoint = allocation_return_breakpoints.find(
                            pending.return_address);
                        if (return_breakpoint == allocation_return_breakpoints.end() ||
                            return_breakpoint->second.pending_count == 0)
                        {
                            *error = "allocator return breakpoint state is missing";
                        }
                        else
                        {
                            shared_return_breakpoint =
                                return_breakpoint->second.pending_count > 1;
                            restored_return = RestoreSoftwareEntryBreakpoint(
                                process,
                                static_cast<std::uint32_t>(pending.return_address),
                                return_breakpoint->second.original_byte,
                                error);
                            if (restored_return)
                            {
                                RecordAllocationReturn(process,
                                                       event.dwThreadId,
                                                       pending,
                                                       context,
                                                       &allocation_trace_state);
                                if (context.Eax != 0)
                                {
                                    allocation_return_values.insert(context.Eax);
                                }
                                --return_breakpoint->second.pending_count;
                                if (shared_return_breakpoint)
                                {
                                    pending_allocation_return_steps[event.dwThreadId] =
                                        pending.return_address;
                                }
                                else
                                {
                                    allocation_return_breakpoints.erase(return_breakpoint);
                                }
                                std::uint8_t ignored_original_byte = 0;
                                rearmed_api = SetSoftwareEntryBreakpoint(
                                    process,
                                    static_cast<std::uint32_t>(pending.api_address),
                                    &ignored_original_byte,
                                    error);
                                context.Eip = static_cast<DWORD>(exception_address);
                                if (shared_return_breakpoint)
                                {
                                    context.EFlags |= 0x100u;
                                }
                                else
                                {
                                    context.EFlags &= ~0x100u;
                                }
                                rearmed_api = rearmed_api &&
                                              SetThreadContext(thread, &context) != FALSE;
                            }
                        }
                    }
                    if (thread != nullptr)
                    {
                        CloseHandle(thread);
                    }
                    allocation_thread->second.erase(allocation_return);
                    if (allocation_thread->second.empty())
                    {
                        pending_allocation_returns.erase(allocation_thread);
                    }
                    if (!captured || !restored_return || !rearmed_api)
                    {
                        if (error->empty())
                        {
                            *error = "cannot complete allocator return breakpoint";
                        }
                        return false;
                    }
                    if (ContinueDebugEvent(event.dwProcessId,
                                           event.dwThreadId,
                                           DBG_CONTINUE) == FALSE)
                    {
                        *error = "cannot continue allocator return breakpoint";
                        return false;
                    }
                    continue;
                }
                const auto guest_return = guest_return_watches->find(exception_address);
                if (guest_return != guest_return_watches->end())
                {
                    bool swallowed = false;
                    HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT,
                                               FALSE,
                                               event.dwThreadId);
                    CONTEXT context = {};
                    context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
                    bool captured = thread != nullptr &&
                                    GetThreadContext(thread, &context) != FALSE;
                    if (captured && guest_return->second.kind ==
                                        GuestReturnWatchPoint::Kind::kD3dInit)
                    {
                        constexpr std::uint32_t kObjectRvas[] = {0x01ab7cc0,
                                                                  0x01ab7cc4,
                                                                  0x01ab7ce0,
                                                                  0x01ab7d00,
                                                                  0x01ab7d04,
                                                                  0x01ab7d08,
                                                                  0x01ab7d24,
                                                                  0x01ab7d48};
                        std::uint32_t objects[std::size(kObjectRvas)] = {};
                        for (std::size_t index = 0;
                             captured && index < std::size(kObjectRvas);
                             ++index)
                        {
                            captured = ReadRemoteU32(
                                process, image_base + kObjectRvas[index], &objects[index]);
                        }
                        if (captured)
                        {
                            RecordDiagnostic("{\"event\":\"d3d_init_return\",\"thread\":%u,\"stage\":\"%s\",\"address\":\"0x%08x\",\"result\":\"0x%08x\",\"success\":%s,\"objects\":{\"d3d_device3\":\"0x%08x\",\"device_aux\":\"0x%08x\",\"d3d3\":\"0x%08x\",\"direct_draw4\":\"0x%08x\",\"primary_surface\":\"0x%08x\",\"surface_aux\":\"0x%08x\"},\"markers\":{\"zbuffer_caps\":\"0x%08x\",\"find_device_passed\":\"0x%08x\"}}",
                                             static_cast<unsigned>(event.dwThreadId),
                                             guest_return->second.name,
                                             static_cast<unsigned>(exception_address),
                                             static_cast<unsigned>(context.Eax),
                                             context.Eax == 0 ? "true" : "false",
                                             objects[0],
                                             objects[1],
                                             objects[2],
                                             objects[3],
                                             objects[4],
                                             objects[5],
                                             objects[6],
                                             objects[7]);
                        }
                    }
                    else if (captured)
                    {
                        std::uint32_t sound_slot = 0;
                        std::uint32_t sound_buffer = 0;
                        std::uint32_t parsed_bytes = 0;
                        std::uint32_t retry_index = 0;
                        std::uint32_t filename_pointer = 0;
                        char filename[128] = {};
                        const bool have_slot = context.Ebp >= 4 &&
                                               ReadRemoteU32(process,
                                                             context.Ebp - 4,
                                                             &sound_slot);
                        const bool have_buffer = have_slot && sound_slot != 0 &&
                                                 ReadRemoteU32(process,
                                                               sound_slot + 0x3c,
                                                               &sound_buffer);
                        const bool have_parsed_bytes = have_slot && sound_slot != 0 &&
                                                       ReadRemoteU32(process,
                                                                     sound_slot + 0x20,
                                                                     &parsed_bytes);
                        const bool have_retry = context.Ebp >= 0x230 &&
                                                ReadRemoteU32(process,
                                                              context.Ebp - 0x230,
                                                              &retry_index);
                        const bool have_filename =
                            ReadRemoteU32(process,
                                          context.Ebp + 8,
                                          &filename_pointer) &&
                            ReadRemoteAnsiString(process, filename_pointer, filename);
                        const bool success = guest_return->second.nonzero_is_success
                                                 ? context.Eax != 0
                                                 : context.Eax == 0;
                        RecordDiagnostic("{\"event\":\"ksnd_load_return\",\"thread\":%u,\"file\":\"%s\",\"file_readable\":%s,\"stage\":\"%s\",\"address\":\"0x%08x\",\"result\":\"0x%08x\",\"success\":%s,\"sound_slot\":\"0x%08x\",\"sound_slot_readable\":%s,\"sound_buffer\":\"0x%08x\",\"sound_buffer_readable\":%s,\"parsed_bytes\":%u,\"parsed_bytes_readable\":%s,\"retry_index\":%u,\"retry_index_readable\":%s}",
                                         static_cast<unsigned>(event.dwThreadId),
                                         have_filename ? filename : "",
                                         have_filename ? "true" : "false",
                                         guest_return->second.name,
                                         static_cast<unsigned>(exception_address),
                                         static_cast<unsigned>(context.Eax),
                                         success ? "true" : "false",
                                         sound_slot,
                                         have_slot ? "true" : "false",
                                         sound_buffer,
                                         have_buffer ? "true" : "false",
                                         parsed_bytes,
                                         have_parsed_bytes ? "true" : "false",
                                         retry_index,
                                         have_retry ? "true" : "false");
                    }
                    if (!captured)
                    {
                        *error = "cannot capture guest return context";
                    }
                    else
                    {
                        SIZE_T written = 0;
                        context.Eip = static_cast<DWORD>(exception_address);
                        swallowed = WriteProcessMemory(
                                        process,
                                        reinterpret_cast<void*>(exception_address),
                                        &guest_return->second.original_byte,
                                        sizeof(guest_return->second.original_byte),
                                        &written) != FALSE &&
                                    written == sizeof(guest_return->second.original_byte) &&
                                    FlushInstructionCache(
                                        process,
                                        reinterpret_cast<const void*>(exception_address),
                                        sizeof(guest_return->second.original_byte)) != FALSE &&
                                    SetThreadContext(thread, &context) != FALSE;
                        if (swallowed && guest_return->second.kind ==
                                             GuestReturnWatchPoint::Kind::kKsndLoad)
                        {
                            context.EFlags |= 0x100;
                            swallowed = SetThreadContext(thread, &context) != FALSE;
                            if (swallowed)
                            {
                                pending_guest_return_steps[event.dwThreadId] =
                                    exception_address;
                            }
                        }
                        else if (swallowed)
                        {
                            guest_return_watches->erase(guest_return);
                        }
                        else
                        {
                            *error = "cannot swallow guest return breakpoint";
                        }
                    }
                    if (thread != nullptr)
                    {
                        CloseHandle(thread);
                    }
                    if (!swallowed ||
                        ContinueDebugEvent(event.dwProcessId,
                                           event.dwThreadId,
                                           DBG_CONTINUE) == FALSE)
                    {
                        return false;
                    }
                    continue;
                }
                const auto post_resume =
                    post_device_io_control_traces.find(event.dwThreadId);
                if (post_resume != post_device_io_control_traces.end() &&
                    post_resume->second.waiting_for_resume &&
                    exception_address == post_resume->second.resume_address)
                {
                    bool swallowed = false;
                    HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT,
                                               FALSE,
                                               event.dwThreadId);
                    CONTEXT context = {};
                    context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
                    if (thread == nullptr || GetThreadContext(thread, &context) == FALSE)
                    {
                        *error = "cannot capture LPTDI post-IOCTL resume context";
                    }
                    else
                    {
                        context.Eip = post_resume->second.resume_address;
                        RecordPostDeviceIoControlSample(
                            process,
                            event.dwThreadId,
                            post_resume->second,
                            context,
                            &post_resume->second.resume_original_byte);
                        post_resume->second.last_address = context.Eip;
                        post_resume->second.last_byte_count = 0;
                        ReadProcessMemory(process,
                                          reinterpret_cast<const void*>(context.Eip),
                                          post_resume->second.last_bytes.data(),
                                          post_resume->second.last_bytes.size(),
                                          &post_resume->second.last_byte_count);
                        if (post_resume->second.last_byte_count != 0)
                        {
                            post_resume->second.last_bytes[0] =
                                post_resume->second.resume_original_byte;
                        }
                        ++post_resume->second.sequence;
                        --post_resume->second.remaining;
                        const std::uint32_t code = post_resume->second.code;
                        const std::uint32_t samples = post_resume->second.sequence;
                        const bool finished = post_resume->second.remaining == 0;
                        if (!finished)
                        {
                            post_resume->second.waiting_for_resume = false;
                            context.EFlags |= 0x100;
                        }
                        SIZE_T written = 0;
                        swallowed =
                            WriteProcessMemory(
                                process,
                                reinterpret_cast<void*>(post_resume->second.resume_address),
                                &post_resume->second.resume_original_byte,
                                sizeof(post_resume->second.resume_original_byte),
                                &written) != FALSE &&
                            written == sizeof(post_resume->second.resume_original_byte) &&
                            FlushInstructionCache(
                                process,
                                reinterpret_cast<const void*>(post_resume->second.resume_address),
                                sizeof(post_resume->second.resume_original_byte)) != FALSE &&
                            SetThreadContext(thread, &context) != FALSE;
                        if (swallowed && finished)
                        {
                            RecordDiagnostic("{\"event\":\"lptdi_post_ioctl_trace_end\",\"thread\":%u,\"code\":\"0x%08x\",\"samples\":%u,\"reason\":\"step_limit\"}",
                                             static_cast<unsigned>(event.dwThreadId),
                                             code,
                                             samples);
                            post_device_io_control_traces.erase(post_resume);
                        }
                        else if (!swallowed)
                        {
                            *error = "cannot swallow LPTDI post-IOCTL resume breakpoint";
                        }
                    }
                    if (thread != nullptr)
                    {
                        CloseHandle(thread);
                    }
                    if (!swallowed ||
                        ContinueDebugEvent(event.dwProcessId,
                                           event.dwThreadId,
                                           DBG_CONTINUE) == FALSE)
                    {
                        return false;
                    }
                    continue;
                }
                const auto device_return = pending_device_io_controls.find(event.dwThreadId);
                if (device_return != pending_device_io_controls.end() &&
                    exception_address == device_return->second.return_address)
                {
                    bool swallowed = false;
                    HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT,
                                               FALSE,
                                               event.dwThreadId);
                    CONTEXT context = {};
                    context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
                    if (thread == nullptr || GetThreadContext(thread, &context) == FALSE)
                    {
                        *error = "cannot capture DeviceIoControl return context";
                    }
                    else
                    {
                        const PendingDeviceIoControl pending = device_return->second;
                        const RemoteBufferSnapshot input_after =
                            ReadRemoteBufferSnapshot(process, pending.args[2], pending.args[3]);
                        const RemoteBufferSnapshot output_after =
                            ReadRemoteBufferSnapshot(process, pending.args[4], pending.args[5]);
                        std::uint32_t bytes_returned_after = 0;
                        const bool have_bytes_returned_after =
                            ReadRemoteU32(process, pending.args[6], &bytes_returned_after);
                        const bool input_unchanged =
                            pending.input_before.readable == input_after.readable &&
                            pending.input_before.bytes == input_after.bytes;
                        const bool output_unchanged =
                            pending.output_before.readable == output_after.readable &&
                            pending.output_before.bytes == output_after.bytes;
                        RecordDiagnostic("{\"event\":\"device_io_control_return\",\"thread\":%u,\"caller\":\"0x%08x\",\"code\":\"0x%08x\",\"eax\":\"0x%08x\",\"success\":%s,\"input_readable\":%s,\"input_after\":\"%s\",\"input_unchanged\":%s,\"output_readable\":%s,\"output_after\":\"%s\",\"output_unchanged\":%s,\"bytes_returned_after_valid\":%s,\"bytes_returned_after\":%u}",
                                         static_cast<unsigned>(event.dwThreadId),
                                         pending.return_address,
                                         pending.args[1],
                                         static_cast<unsigned>(context.Eax),
                                         context.Eax != 0 ? "true" : "false",
                                         input_after.readable ? "true" : "false",
                                         input_after.bytes.c_str(),
                                         input_unchanged ? "true" : "false",
                                         output_after.readable ? "true" : "false",
                                         output_after.bytes.c_str(),
                                         output_unchanged ? "true" : "false",
                                         have_bytes_returned_after ? "true" : "false",
                                         bytes_returned_after);

                        SIZE_T written = 0;
                        context.Eip = pending.return_address;
                        if (lptdi_post_ioctl_trace_steps != 0 &&
                            IsSyntheticDeviceHandle(pending.args[0]) &&
                            (lptdi_post_ioctl_trace_code == 0 ||
                             pending.args[1] == lptdi_post_ioctl_trace_code))
                        {
                            MEMORY_BASIC_INFORMATION region = {};
                            if (VirtualQueryEx(process,
                                               reinterpret_cast<const void*>(pending.return_address),
                                               &region,
                                               sizeof(region)) != sizeof(region))
                            {
                                *error = "cannot identify LPTDI post-IOCTL caller allocation";
                            }
                            else
                            {
                                PostDeviceIoControlTrace post_trace;
                                post_trace.code = pending.args[1];
                                post_trace.output_address = pending.args[4];
                                post_trace.output_size = pending.args[5];
                                post_trace.allocation_base =
                                    reinterpret_cast<std::uintptr_t>(region.AllocationBase);
                                post_trace.remaining = lptdi_post_ioctl_trace_steps;
                                RecordPostDeviceIoControlSample(process,
                                                                event.dwThreadId,
                                                                post_trace,
                                                                context,
                                                                &pending.original_byte);
                                post_trace.last_address = context.Eip;
                                post_trace.last_byte_count = 0;
                                ReadProcessMemory(process,
                                                  reinterpret_cast<const void*>(context.Eip),
                                                  post_trace.last_bytes.data(),
                                                  post_trace.last_bytes.size(),
                                                  &post_trace.last_byte_count);
                                if (post_trace.last_byte_count != 0)
                                {
                                    post_trace.last_bytes[0] = pending.original_byte;
                                }
                                ++post_trace.sequence;
                                --post_trace.remaining;
                                if (post_trace.remaining != 0)
                                {
                                    context.EFlags |= 0x100;
                                    post_device_io_control_traces[event.dwThreadId] = post_trace;
                                }
                                else
                                {
                                    RecordDiagnostic("{\"event\":\"lptdi_post_ioctl_trace_end\",\"thread\":%u,\"code\":\"0x%08x\",\"samples\":1,\"reason\":\"step_limit\"}",
                                                     static_cast<unsigned>(event.dwThreadId),
                                                     post_trace.code);
                                }
                            }
                        }
                        swallowed =
                            error->empty() && WriteProcessMemory(
                                process,
                                reinterpret_cast<void*>(pending.return_address),
                                &pending.original_byte,
                                sizeof(pending.original_byte),
                                &written) != FALSE &&
                            written == sizeof(pending.original_byte) &&
                            FlushInstructionCache(
                                process,
                                reinterpret_cast<const void*>(pending.return_address),
                                sizeof(pending.original_byte)) != FALSE &&
                            SetThreadContext(thread, &context) != FALSE;
                        if (swallowed)
                        {
                            pending_device_io_controls.erase(device_return);
                        }
                        else
                        {
                            *error = "cannot swallow DeviceIoControl return breakpoint";
                        }
                    }
                    if (thread != nullptr)
                    {
                        CloseHandle(thread);
                    }
                    if (!swallowed ||
                        ContinueDebugEvent(event.dwProcessId,
                                           event.dwThreadId,
                                           DBG_CONTINUE) == FALSE)
                    {
                        return false;
                    }
                    continue;
                }
                if (resume_bp_armed &&
                    exception_address == static_cast<std::uintptr_t>(resume_bp_address))
                {
                    // The thread came back through the gate to the stub's
                    // return address. Swallow the breakpoint, restore the
                    // byte, rewind EIP, and re-arm TF so sampling resumes.
                    bool swallowed = false;
                    HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT,
                                               FALSE,
                                               event.dwThreadId);
                    CONTEXT context = {};
                    context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
                    constexpr std::size_t kResumeStackWords = 64;
                    std::uint32_t stack_words[kResumeStackWords] = {};
                    SIZE_T copied = 0;
                    const bool captured =
                        thread != nullptr && GetThreadContext(thread, &context) != FALSE &&
                        ReadProcessMemory(process,
                                          reinterpret_cast<const void*>(context.Esp),
                                          stack_words,
                                          sizeof(stack_words),
                                          &copied) != FALSE &&
                        copied >= sizeof(std::uint32_t);
                    if (!captured)
                    {
                        *error = "cannot capture syscall resume context";
                    }
                    else
                    {
                        RecordDiagnostic("{\"event\":\"syscall_resume_hit\",\"address\":\"0x%08x\",\"eax\":\"0x%08x\",\"ebx\":\"0x%08x\",\"ecx\":\"0x%08x\",\"edx\":\"0x%08x\",\"esi\":\"0x%08x\",\"edi\":\"0x%08x\",\"ebp\":\"0x%08x\",\"esp\":\"0x%08x\"}",
                                         resume_bp_address,
                                         static_cast<unsigned>(context.Eax),
                                         static_cast<unsigned>(context.Ebx),
                                         static_cast<unsigned>(context.Ecx),
                                         static_cast<unsigned>(context.Edx),
                                         static_cast<unsigned>(context.Esi),
                                         static_cast<unsigned>(context.Edi),
                                         static_cast<unsigned>(context.Ebp),
                                         static_cast<unsigned>(context.Esp));
                        const std::size_t word_count = copied / sizeof(std::uint32_t);
                        std::string words;
                        words.reserve(word_count * 14);
                        for (std::size_t index = 0; index < word_count; ++index)
                        {
                            char word[16] = {};
                            std::snprintf(word, sizeof(word), "\"0x%08x\"", stack_words[index]);
                            if (index != 0)
                            {
                                words += ',';
                            }
                            words += word;
                        }
                        RecordDiagnostic("{\"event\":\"resume_stack\",\"esp\":\"0x%08x\",\"words\":[%s]}",
                                         static_cast<unsigned>(context.Esp),
                                         words.c_str());
                        // Attribute image-range words so the owner of the
                        // teardown frames becomes visible in the log.
                        constexpr unsigned kMaxResumeAnnotations = 24;
                        unsigned annotated = 0;
                        for (std::size_t index = 0;
                             index < word_count && annotated < kMaxResumeAnnotations;
                             ++index)
                        {
                            if (stack_words[index] == 0)
                            {
                                continue;
                            }
                            std::string symbol;
                            if (!format_remote_symbol(stack_words[index], &symbol))
                            {
                                continue;
                            }
                            ++annotated;
                            RecordDiagnostic("{\"event\":\"resume_stack_symbol\",\"index\":%u,\"value\":\"0x%08x\",\"symbol\":\"%s\"}",
                                             static_cast<unsigned>(index),
                                             stack_words[index],
                                             symbol.c_str());
                        }
                        SIZE_T written = 0;
                        context.Eip = resume_bp_address;
                        context.EFlags |= 0x100;
                        swallowed =
                            WriteProcessMemory(
                                process,
                                reinterpret_cast<void*>(resume_bp_address),
                                &resume_bp_original,
                                sizeof(resume_bp_original),
                                &written) != FALSE &&
                            written == sizeof(resume_bp_original) &&
                            FlushInstructionCache(
                                process,
                                reinterpret_cast<const void*>(resume_bp_address),
                                sizeof(resume_bp_original)) != FALSE &&
                            SetThreadContext(thread, &context) != FALSE;
                        if (swallowed)
                        {
                            ++resume_bp_fires;
                            resume_bp_armed = false;
                        }
                        else
                        {
                            *error = "cannot swallow syscall resume breakpoint";
                        }
                    }
                    if (thread != nullptr)
                    {
                        CloseHandle(thread);
                    }
                    if (!swallowed ||
                        ContinueDebugEvent(event.dwProcessId,
                                           event.dwThreadId,
                                           DBG_CONTINUE) == FALSE)
                    {
                        return false;
                    }
                    continue;
                }
                const auto watch = api_watches->find(exception_address);
                if (watch != api_watches->end())
                {
                    bool swallowed = false;
                    HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT,
                                               FALSE,
                                               event.dwThreadId);
                    CONTEXT context = {};
                    context.ContextFlags = CONTEXT_CONTROL;
                    std::uint32_t stack[13] = {};
                    SIZE_T copied = 0;
                    const SIZE_T stack_size =
                        (watch->second.argument_count + 1) * sizeof(std::uint32_t);
                    const bool captured =
                        thread != nullptr && GetThreadContext(thread, &context) != FALSE &&
                        ReadProcessMemory(process,
                                          reinterpret_cast<const void*>(context.Esp),
                                          stack,
                                          stack_size,
                                          &copied) != FALSE &&
                        copied == stack_size;
                    if (!captured)
                    {
                        *error = "cannot capture watched API call context";
                    }
                    else
                    {
                        RecordApiCall(event.dwThreadId,
                                      watch->second,
                                      exception_address,
                                      stack[0],
                                      process,
                                      stack + 1);
                        constexpr std::uintptr_t kOriginalEntryGetVersionCallerRva = 0x0003a66c;
                        if (!original_initializer_recorded &&
                            watch->second.name == "GetVersion" &&
                            stack[0] == image_base + kOriginalEntryGetVersionCallerRva)
                        {
                            original_initializer_recorded =
                                RecordOriginalInitializerWindow(process, image_base);
                            if (!original_initializer_recorded)
                            {
                                RecordDiagnostic("{\"event\":\"original_initializer_window_error\"}");
                            }
                        }
                        if (watch->second.name == "DeviceIoControl" && image_info != nullptr &&
                            stack[0] >= image_base &&
                            stack[0] < image_base + image_info->size_of_image)
                        {
                            const auto prior_trace =
                                post_device_io_control_traces.find(event.dwThreadId);
                            if (prior_trace != post_device_io_control_traces.end() &&
                                prior_trace->second.waiting_for_resume &&
                                prior_trace->second.resume_address == stack[0])
                            {
                                SIZE_T restored_size = 0;
                                const bool restored =
                                    WriteProcessMemory(
                                        process,
                                        reinterpret_cast<void*>(stack[0]),
                                        &prior_trace->second.resume_original_byte,
                                        sizeof(prior_trace->second.resume_original_byte),
                                        &restored_size) != FALSE &&
                                    restored_size ==
                                        sizeof(prior_trace->second.resume_original_byte) &&
                                    FlushInstructionCache(
                                        process,
                                        reinterpret_cast<const void*>(stack[0]),
                                        sizeof(prior_trace->second.resume_original_byte)) != FALSE;
                                if (!restored)
                                {
                                    *error = "cannot retire LPTDI trace before next IOCTL";
                                }
                                else
                                {
                                    RecordDiagnostic("{\"event\":\"lptdi_post_ioctl_trace_end\",\"thread\":%u,\"code\":\"0x%08x\",\"samples\":%u,\"reason\":\"next_ioctl\"}",
                                                     static_cast<unsigned>(event.dwThreadId),
                                                     prior_trace->second.code,
                                                     prior_trace->second.sequence);
                                    post_device_io_control_traces.erase(prior_trace);
                                }
                            }
                            if (pending_device_io_controls.count(event.dwThreadId) != 0)
                            {
                                *error = "overlapping DeviceIoControl return trace on one thread";
                            }
                            else
                            {
                                PendingDeviceIoControl pending = RecordDeviceIoControlEntry(
                                    event.dwThreadId, stack[0], process, stack + 1);
                                if (SetSoftwareEntryBreakpoint(process,
                                                               pending.return_address,
                                                               &pending.original_byte,
                                                               error))
                                {
                                    pending_device_io_controls.emplace(event.dwThreadId,
                                                                       std::move(pending));
                                }
                            }
                            if (!error->empty())
                            {
                                if (thread != nullptr)
                                {
                                    CloseHandle(thread);
                                }
                                return false;
                            }
                        }
                        if (allocation_trace && IsAllocationApi(watch->second))
                        {
                            PendingAllocationReturn pending;
                            pending.api_address = exception_address;
                            pending.return_address = stack[0];
                            pending.watch = watch->second;
                            std::copy_n(stack + 1,
                                        pending.args.size(),
                                        pending.args.begin());
                            if (pending.return_address == 0 ||
                                     pending.return_address == exit_target ||
                                     api_watches->count(pending.return_address) != 0 ||
                                     guest_return_watches->count(pending.return_address) != 0)
                            {
                                *error = "allocator return breakpoint address collides with an existing watch";
                            }
                            else
                            {
                                auto return_breakpoint = allocation_return_breakpoints.find(
                                    pending.return_address);
                                bool new_return_breakpoint =
                                    return_breakpoint == allocation_return_breakpoints.end();
                                bool return_breakpoint_ready = !new_return_breakpoint;
                                if (new_return_breakpoint)
                                {
                                    return_breakpoint_ready = SetSoftwareEntryBreakpoint(
                                        process,
                                        static_cast<std::uint32_t>(pending.return_address),
                                        &pending.return_original_byte,
                                        error);
                                    if (return_breakpoint_ready &&
                                        pending.return_original_byte == 0xcc)
                                    {
                                        RestoreSoftwareEntryBreakpoint(
                                            process,
                                            static_cast<std::uint32_t>(pending.return_address),
                                            pending.return_original_byte,
                                            error);
                                        *error = "allocator return breakpoint landed on an existing software breakpoint";
                                        return_breakpoint_ready = false;
                                    }
                                    if (return_breakpoint_ready)
                                    {
                                        AllocationReturnBreakpoint state;
                                        state.original_byte = pending.return_original_byte;
                                        return_breakpoint =
                                            allocation_return_breakpoints.emplace(
                                                pending.return_address,
                                                state)
                                                .first;
                                    }
                                }
                                else
                                {
                                    pending.return_original_byte =
                                        return_breakpoint->second.original_byte;
                                }
                                if (return_breakpoint_ready &&
                                    RestoreSoftwareEntryBreakpoint(
                                        process,
                                        static_cast<std::uint32_t>(exception_address),
                                        watch->second.original_byte,
                                        error))
                                {
                                    context.Eip = static_cast<DWORD>(exception_address);
                                    context.EFlags &= ~0x100u;
                                    if (SetThreadContext(thread, &context) != FALSE)
                                    {
                                        ++return_breakpoint->second.pending_count;
                                        pending_allocation_returns[event.dwThreadId].push_back(
                                            std::move(pending));
                                        ++allocation_trace_state.armed_count;
                                        RecordDiagnostic(
                                            "{\"event\":\"null_context_allocation_arm\",\"thread\":%u,\"api\":\"%s\",\"address\":\"0x%08x\",\"return\":\"0x%08x\",\"arg0\":\"0x%08x\",\"arg1\":\"0x%08x\",\"arg2\":\"0x%08x\",\"arg3\":\"0x%08x\"}",
                                            static_cast<unsigned>(event.dwThreadId),
                                            watch->second.name.c_str(),
                                            static_cast<unsigned>(exception_address),
                                            static_cast<unsigned>(stack[0]),
                                            static_cast<unsigned>(stack[1]),
                                            static_cast<unsigned>(stack[2]),
                                            static_cast<unsigned>(stack[3]),
                                            static_cast<unsigned>(stack[4]));
                                        swallowed = true;
                                    }
                                    else
                                    {
                                        *error = "cannot set allocator entry context";
                                    }
                                }
                                if (!swallowed && new_return_breakpoint &&
                                    return_breakpoint_ready)
                                {
                                    RestoreSoftwareEntryBreakpoint(
                                        process,
                                        static_cast<std::uint32_t>(pending.return_address),
                                        return_breakpoint->second.original_byte,
                                        error);
                                    allocation_return_breakpoints.erase(return_breakpoint);
                                }
                            }
                        }
                        else
                        {
                            // Restore the original byte, rewind EIP onto the API
                            // entry, and trap-flag one instruction before
                            // rearming on the following single step.
                            SIZE_T written = 0;
                            context.Eip = static_cast<DWORD>(exception_address);
                            context.EFlags |= 0x100;
                            const bool restored =
                                WriteProcessMemory(
                                    process,
                                    reinterpret_cast<void*>(exception_address),
                                    &watch->second.original_byte,
                                    sizeof(watch->second.original_byte),
                                    &written) != FALSE &&
                                written == sizeof(watch->second.original_byte) &&
                                FlushInstructionCache(
                                    process,
                                    reinterpret_cast<const void*>(exception_address),
                                    sizeof(watch->second.original_byte)) != FALSE &&
                                SetThreadContext(thread, &context) != FALSE;
                            if (restored)
                            {
                                pending_api_steps[event.dwThreadId] = exception_address;
                                swallowed = true;
                            }
                            else
                            {
                                *error = "cannot swallow watched API breakpoint hit";
                            }
                        }
                        if (!swallowed && error->empty())
                        {
                            *error = "cannot swallow watched API breakpoint hit";
                        }
                    }
                    if (thread != nullptr)
                    {
                        CloseHandle(thread);
                    }
                    if (!swallowed ||
                        ContinueDebugEvent(event.dwProcessId,
                                           event.dwThreadId,
                                           DBG_CONTINUE) == FALSE)
                    {
                        return false;
                    }
                    continue;
                }
            }
        }
        if (field_write_watch_address != 0 &&
            event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT &&
            event.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_SINGLE_STEP)
        {
            bool handled = false;
            if (!HandleFieldWriteWatchHit(process,
                                          event.dwThreadId,
                                          field_write_watch_address,
                                          image_base,
                                          image_info,
                                          &field_write_watch_state,
                                          &handled,
                                          error))
            {
                return false;
            }
            if (handled)
            {
                if (ContinueDebugEvent(event.dwProcessId,
                                       event.dwThreadId,
                                       DBG_CONTINUE) == FALSE)
                {
                    *error = "cannot continue a field write watch hit";
                    return false;
                }
                continue;
            }
        }
        if (event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT)
        {
            if (event.u.Exception.dwFirstChance != 0 &&
                event.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
            {
                RecordAccessViolationContext(process,
                                             event.dwThreadId,
                                             event.u.Exception.ExceptionRecord,
                                             image_base,
                                             image_info);
                // The packed image is decrypted by the time it faults, so this
                // is where a scan of the guest's own code can run. Once per
                // run: the answer does not change between faults.
                if (!field_reference_scan_constants.empty() && !field_references_scanned)
                {
                    field_references_scanned = true;
                    for (const std::uint32_t constant : field_reference_scan_constants)
                    {
                        ScanGuestFieldReferences(process, image_base, image_info, constant);
                    }
                }
                if (!code_windows.empty() && !code_windows_captured)
                {
                    code_windows_captured = true;
                    for (const GuestCodeWindowRequest& request : code_windows)
                    {
                        CaptureGuestCodeWindow(process, image_base, image_info, request);
                    }
                }
            }
            if (scan_fault_references &&
                event.u.Exception.dwFirstChance != 0 &&
                event.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION)
            {
                ScanFaultReferences(
                    process,
                    static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(
                        event.u.Exception.ExceptionRecord.ExceptionAddress)));
            }
            if (event.u.Exception.dwFirstChance != 0 &&
                event.u.Exception.ExceptionRecord.ExceptionCode ==
                    EXCEPTION_ILLEGAL_INSTRUCTION)
            {
                RecordIllegalInstructionContext(process,
                                                event.dwThreadId,
                                                event.u.Exception.ExceptionRecord,
                                                image_base,
                                                image_info);
                if (unload_tail_collecting)
                {
                    RecordInstructionHistory(unload_history, unload_steps);
                    char message[160] = {};
                    std::snprintf(message,
                                  sizeof(message),
                                  "api trace reached illegal instruction at 0x%08x after %u unload-tail single steps",
                                  static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(
                                      event.u.Exception.ExceptionRecord.ExceptionAddress)),
                                  unload_steps);
                    *error = message;
                    return false;
                }
            }
            MEMORY_BASIC_INFORMATION memory = {};
            if (VirtualQueryEx(process,
                               event.u.Exception.ExceptionRecord.ExceptionAddress,
                               &memory,
                               sizeof(memory)) == sizeof(memory))
            {
                RecordDiagnostic("{\"exception_region\":\"0x%08x\",\"allocation\":\"0x%08x\",\"protect\":\"0x%08x\",\"type\":\"0x%08x\"}",
                                 static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(memory.BaseAddress)),
                                 static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(memory.AllocationBase)),
                                 static_cast<unsigned>(memory.Protect),
                                 static_cast<unsigned>(memory.Type));
            }
                HANDLE thread = OpenThread(THREAD_GET_CONTEXT, FALSE, event.dwThreadId);
                CONTEXT context = {};
                context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
                std::uint32_t stack[16] = {};
                SIZE_T stack_copied = 0;
                if (thread != nullptr && GetThreadContext(thread, &context) != FALSE &&
                    ReadProcessMemory(process,
                                      reinterpret_cast<const void*>(context.Esp),
                                      stack,
                                      sizeof(stack),
                                      &stack_copied) != FALSE &&
                    stack_copied >= sizeof(std::uint32_t))
                {
                    const std::size_t words = stack_copied / sizeof(std::uint32_t);
                    std::string words_json;
                    for (std::size_t i = 0; i < words; ++i)
                    {
                        if (i > 0) words_json += ",";
                        char buf[16] = {};
                        std::snprintf(buf, sizeof(buf), "\"0x%08x\"", stack[i]);
                        words_json += buf;
                    }
                    std::uint32_t frame[16] = {};
                    SIZE_T frame_copied = 0;
                    std::string frame_json;
                    if (ReadProcessMemory(process,
                                          reinterpret_cast<const void*>(context.Ebp),
                                          frame,
                                          sizeof(frame),
                                          &frame_copied) != FALSE &&
                        frame_copied >= sizeof(std::uint32_t))
                    {
                        const std::size_t fwords = frame_copied / sizeof(std::uint32_t);
                        for (std::size_t i = 0; i < fwords; ++i)
                        {
                            if (i > 0) frame_json += ",";
                            char buf[16] = {};
                            std::snprintf(buf, sizeof(buf), "\"0x%08x\"", frame[i]);
                            frame_json += buf;
                        }
                    }
                    RecordDiagnostic("{\"exception_esp\":\"0x%08x\",\"exception_ebp\":\"0x%08x\",\"exception_stack\":[%s],\"exception_frame\":[%s]}",
                                     static_cast<unsigned>(context.Esp),
                                     static_cast<unsigned>(context.Ebp),
                                     words_json.c_str(),
                                     frame_json.c_str());
                }
                if (thread != nullptr)
                {
                    CloseHandle(thread);
                }
                std::uint8_t bytes[16] = {};
                SIZE_T copied = 0;
                if (ReadProcessMemory(process,
                                      event.u.Exception.ExceptionRecord.ExceptionAddress,
                                      bytes,
                                      sizeof(bytes),
                                      &copied) != FALSE &&
                    copied == sizeof(bytes))
                {
                    char text[sizeof(bytes) * 2 + 1] = {};
                    for (std::size_t index = 0; index < sizeof(bytes); ++index)
                    {
                        std::snprintf(text + index * 2, 3, "%02x", bytes[index]);
                    }
                    RecordDiagnostic("{\"exception_bytes\":\"%s\"}", text);
                }
            }
        if (event.dwDebugEventCode == UNLOAD_DLL_DEBUG_EVENT && api_watches != nullptr &&
            !api_watches->empty() && !unload_tail_collecting && last_activity_thread != 0)
        {
            const std::uintptr_t unloaded_base = reinterpret_cast<std::uintptr_t>(
                event.u.UnloadDll.lpBaseOfDll);
            if (dynamic_module_bases.count(unloaded_base) != 0)
            {
                // Dynamically loaded modules only unload through guest-driven
                // FreeLibrary calls, so the unload tail is the crash-adjacent
                // stretch worth single-stepping.
                unload_tail_collecting = true;
                unload_tail_thread = last_activity_thread;
                RecordDiagnostic("{\"event\":\"unload_tail_arm\",\"base\":\"0x%08x\",\"thread\":%u}",
                                 static_cast<unsigned>(unloaded_base),
                                 static_cast<unsigned>(unload_tail_thread));
                if (!RearmSingleStep(unload_tail_thread, error))
                {
                    return false;
                }
            }
        }
        if (event.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT)
        {
            if (event.u.LoadDll.lpBaseOfDll != nullptr)
            {
                dynamic_module_bases.insert(reinterpret_cast<std::uintptr_t>(
                    event.u.LoadDll.lpBaseOfDll));
            }
            if (event.u.LoadDll.hFile != nullptr)
            {
                CloseHandle(event.u.LoadDll.hFile);
            }
        }
        if (event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT &&
            event.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_BREAKPOINT &&
            reinterpret_cast<std::uintptr_t>(event.u.Exception.ExceptionRecord.ExceptionAddress) ==
                exit_target)
        {
            HANDLE thread = OpenThread(THREAD_GET_CONTEXT, FALSE, event.dwThreadId);
            CONTEXT context = {};
            context.ContextFlags = CONTEXT_CONTROL;
            std::uint32_t return_address = 0;
            std::uint32_t wrapper_frame[4] = {};
            SIZE_T copied = 0;
            const bool captured = thread != nullptr && GetThreadContext(thread, &context) != FALSE &&
                                  ReadProcessMemory(process,
                                                    reinterpret_cast<const void*>(context.Esp),
                                                    &return_address,
                                                    sizeof(return_address),
                                                    &copied) != FALSE &&
                                  copied == sizeof(return_address);
            constexpr std::uint32_t kControlledExitReturnRva = 0x00024061;
            const bool controlled_exit = captured &&
                                         return_address ==
                                             image_base + kControlledExitReturnRva;
            SIZE_T frame_copied = 0;
            const bool frame_readable = controlled_exit &&
                                        ReadProcessMemory(
                                            process,
                                            reinterpret_cast<const void*>(context.Ebp),
                                            wrapper_frame,
                                            sizeof(wrapper_frame),
                                            &frame_copied) != FALSE &&
                                        frame_copied == sizeof(wrapper_frame);
            char exit_message[128] = {};
            const bool message_readable = frame_readable &&
                                          ReadRemoteAnsiString(
                                              process, wrapper_frame[2], exit_message);
            char exit_detail[128] = {};
            const bool detail_readable = frame_readable &&
                                         ReadRemoteAnsiString(
                                             process, wrapper_frame[3], exit_detail);
            const bool caller_in_image = frame_readable && image_info != nullptr &&
                                         wrapper_frame[1] >= image_base &&
                                         wrapper_frame[1] <
                                             image_base + image_info->size_of_image;
            if (thread != nullptr)
            {
                CloseHandle(thread);
            }
            if (!captured)
            {
                *error = "cannot capture ExitProcess caller return address";
                return false;
            }
            RecordDiagnostic("{\"exit_process_return\":\"0x%08x\"}", return_address);
            if (controlled_exit)
            {
                RecordDiagnostic("{\"event\":\"controlled_exit_attribution\",\"wrapper_frame\":\"0x%08x\",\"frame_readable\":%s,\"saved_ebp\":\"0x%08x\",\"caller\":\"0x%08x\",\"caller_in_image\":%s,\"message_pointer\":\"0x%08x\",\"message_readable\":%s,\"message\":\"%s\",\"detail_pointer\":\"0x%08x\",\"detail_readable\":%s,\"detail\":\"%s\"}",
                                 static_cast<unsigned>(context.Ebp),
                                 frame_readable ? "true" : "false",
                                 wrapper_frame[0],
                                 wrapper_frame[1],
                                 caller_in_image ? "true" : "false",
                                 wrapper_frame[2],
                                 message_readable ? "true" : "false",
                                 message_readable ? exit_message : "",
                                 wrapper_frame[3],
                                 detail_readable ? "true" : "false",
                                 detail_readable ? exit_detail : "");
                if (frame_readable)
                {
                    RecordKsndSearchPathState(process, image_base, wrapper_frame[1]);
                }
            }
            return true;
        }
        if (event.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT)
        {
            if (bounded_api_trace || allocation_trace || slot_writer_trace ||
                null_context_object_source_trace ||
                null_context_field_access_trace || null_context_field_writer_trace ||
                null_context_field_reference_execution_trace ||
                null_context_object_state_trace ||
                null_context_object_reference_scan ||
                null_context_entry_trace)
            {
                if (bounded_api_trace)
                {
                    RecordDiagnostic("{\"event\":\"api_trace_boundary\",\"reason\":\"child_exit\",\"events\":%u,\"code\":\"0x%08x\"}",
                                     event_count,
                                     static_cast<unsigned>(event.u.ExitProcess.dwExitCode));
                }
                if (allocation_trace)
                {
                    RecordDiagnostic("{\"event\":\"null_context_allocation_trace_boundary\",\"reason\":\"child_exit\",\"events\":%u,\"armed\":%u,\"hits\":%u,\"recorded\":%u,\"pending\":%u,\"capped\":%s,\"code\":\"0x%08x\"}",
                                     event_count,
                                     allocation_trace_state.armed_count,
                                     allocation_trace_state.hit_count,
                                     allocation_trace_state.recorded_count,
                                     static_cast<unsigned>(PendingAllocationReturnCount(
                                         pending_allocation_returns)),
                                     allocation_trace_state.capped ? "true" : "false",
                                     static_cast<unsigned>(event.u.ExitProcess.dwExitCode));
                }
                if (slot_writer_trace)
                {
                    RecordDiagnostic("{\"event\":\"slot_writer_trace_boundary\",\"reason\":\"child_exit\",\"events\":%u,\"hits\":%u,\"recorded\":%u,\"capped\":%s,\"code\":\"0x%08x\"}",
                                     event_count,
                                     slot_writer_state.hit_count,
                                     slot_writer_state.recorded_count,
                                     slot_writer_state.capped ? "true" : "false",
                                     static_cast<unsigned>(event.u.ExitProcess.dwExitCode));
                }
                if (null_context_object_source_trace)
                {
                    RecordDiagnostic(
                        "{\"event\":\"null_context_object_source_trace_boundary\",\"reason\":\"child_exit\",\"events\":%u,\"boundary_hits\":%u,\"boundary_recorded\":%u,\"boundary_capped\":%s,\"hits\":%u,\"recorded\":%u,\"target_matches\":%u,\"capped\":%s,\"boundary\":\"0x%08x\",\"dynamic_slots\":%u,\"code\":\"0x%08x\"}",
                        event_count,
                        null_context_object_source_state.boundary_hit_count,
                        null_context_object_source_state.boundary_recorded_count,
                        null_context_object_source_state.boundary_capped ? "true" : "false",
                        null_context_object_source_state.hit_count,
                        null_context_object_source_state.recorded_count,
                        null_context_object_source_state.target_match_count,
                        null_context_object_source_state.capped ? "true" : "false",
                        static_cast<unsigned>(object_source_boundary),
                        static_cast<unsigned>(null_context_object_source_state.dynamic_stack_slots.size()),
                        static_cast<unsigned>(event.u.ExitProcess.dwExitCode));
                }
                if (null_context_field_writer_trace)
                {
                    RecordDiagnostic("{\"event\":\"null_context_field_writer_trace_boundary\",\"reason\":\"child_exit\",\"events\":%u,\"hits\":%u,\"recorded\":%u,\"capped\":%s,\"code\":\"0x%08x\"}",
                                     event_count,
                                     null_context_field_writer_state.hit_count,
                                     null_context_field_writer_state.recorded_count,
                                     null_context_field_writer_state.capped ? "true" : "false",
                                     static_cast<unsigned>(event.u.ExitProcess.dwExitCode));
                }
                if (null_context_field_access_trace)
                {
                    RecordDiagnostic("{\"event\":\"null_context_field_access_trace_boundary\",\"reason\":\"child_exit\",\"events\":%u,\"hits\":%u,\"recorded\":%u,\"capped\":%s,\"code\":\"0x%08x\"}",
                                     event_count,
                                     null_context_field_access_state.hit_count,
                                     null_context_field_access_state.recorded_count,
                                     null_context_field_access_state.capped ? "true" : "false",
                                     static_cast<unsigned>(event.u.ExitProcess.dwExitCode));
                }
                if (null_context_field_reference_execution_trace)
                {
                    RecordDiagnostic(
                        "{\"event\":\"null_context_field_reference_execution_trace_boundary\",\"reason\":\"child_exit\",\"events\":%u,\"hits\":%u,\"recorded\":%u,\"target_matches\":%u,\"pending\":%u,\"capped\":%s,\"code\":\"0x%08x\"}",
                        event_count,
                        null_context_field_reference_execution_state.hit_count,
                        null_context_field_reference_execution_state.recorded_count,
                        null_context_field_reference_execution_state.target_match_count,
                        static_cast<unsigned>(null_context_field_reference_execution_state
                                                  .pending_single_step_threads.size()),
                        null_context_field_reference_execution_state.capped ? "true" : "false",
                        static_cast<unsigned>(event.u.ExitProcess.dwExitCode));
                }
                if (null_context_object_state_trace)
                {
                    RecordDiagnostic(
                        "{\"event\":\"null_context_object_state_trace_boundary\",\"reason\":\"child_exit\",\"events\":%u,\"hits\":%u,\"recorded\":%u,\"frames\":%u,\"window_readable\":%s,\"disarmed\":%s,\"code\":\"0x%08x\"}",
                        event_count,
                        null_context_object_state_state.hit_count,
                        null_context_object_state_state.recorded_count,
                        null_context_object_state_state.frame_count,
                        null_context_object_state_state.window_readable ? "true" : "false",
                        null_context_object_state_state.disarmed ? "true" : "false",
                        static_cast<unsigned>(event.u.ExitProcess.dwExitCode));
                }
                if (null_context_object_reference_scan)
                {
                    RecordDiagnostic(
                        "{\"event\":\"null_context_object_reference_scan_boundary\",\"reason\":\"child_exit\",\"events\":%u,\"hits\":%u,\"scanned\":%s,\"text_readable\":%s,\"matches\":%u,\"capped\":%s,\"code\":\"0x%08x\"}",
                        event_count,
                        null_context_object_reference_state.hit_count,
                        null_context_object_reference_state.scanned ? "true" : "false",
                        null_context_object_reference_state.text_readable ? "true" : "false",
                        null_context_object_reference_state.match_count,
                        null_context_object_reference_state.capped ? "true" : "false",
                        static_cast<unsigned>(event.u.ExitProcess.dwExitCode));
                }
                if (null_context_entry_trace)
                {
                    RecordDiagnostic(
                        "{\"event\":\"null_context_entry_trace_boundary\",\"reason\":\"child_exit\",\"events\":%u,\"hits\":%u,\"recorded\":%u,\"singleton_receivers\":%u,\"pending\":%u,\"capped\":%s,\"code\":\"0x%08x\"}",
                        event_count,
                        null_context_entry_state.hit_count,
                        null_context_entry_state.recorded_count,
                        null_context_entry_state.singleton_receiver_count,
                        static_cast<unsigned>(null_context_entry_state.pending_single_step_threads.size()),
                        null_context_entry_state.capped ? "true" : "false",
                        static_cast<unsigned>(event.u.ExitProcess.dwExitCode));
                }
                return true;
            }
            char message[160] = {};
            std::snprintf(message,
                          sizeof(message),
                          "original process exited with code 0x%08x before ExitProcess breakpoint",
                          static_cast<unsigned>(event.u.ExitProcess.dwExitCode));
            *error = message;
            return false;
        }
        const DWORD continue_status = event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT
                                          ? DBG_EXCEPTION_NOT_HANDLED
                                          : DBG_CONTINUE;
        if (ContinueDebugEvent(event.dwProcessId, event.dwThreadId, continue_status) == FALSE)
        {
            *error = "cannot continue child debug event while waiting for ExitProcess";
            return false;
        }
    }
    if (bounded_api_trace || allocation_trace || slot_writer_trace ||
        null_context_object_source_trace ||
        null_context_field_access_trace || null_context_field_writer_trace ||
        null_context_field_reference_execution_trace ||
        null_context_object_state_trace ||
        null_context_object_reference_scan ||
        null_context_entry_trace)
    {
        if (bounded_api_trace)
        {
            RecordDiagnostic("{\"event\":\"api_trace_boundary\",\"reason\":\"event_cap\",\"events\":%u}",
                             event_count);
        }
        if (slot_writer_trace)
        {
            RecordDiagnostic("{\"event\":\"slot_writer_trace_boundary\",\"reason\":\"event_cap\",\"events\":%u,\"hits\":%u,\"recorded\":%u,\"capped\":%s}",
                             event_count,
                             slot_writer_state.hit_count,
                             slot_writer_state.recorded_count,
                             slot_writer_state.capped ? "true" : "false");
        }
        if (null_context_object_source_trace)
        {
            RecordDiagnostic(
                "{\"event\":\"null_context_object_source_trace_boundary\",\"reason\":\"event_cap\",\"events\":%u,\"boundary_hits\":%u,\"boundary_recorded\":%u,\"boundary_capped\":%s,\"hits\":%u,\"recorded\":%u,\"target_matches\":%u,\"capped\":%s,\"boundary\":\"0x%08x\",\"dynamic_slots\":%u}",
                event_count,
                null_context_object_source_state.boundary_hit_count,
                null_context_object_source_state.boundary_recorded_count,
                null_context_object_source_state.boundary_capped ? "true" : "false",
                null_context_object_source_state.hit_count,
                null_context_object_source_state.recorded_count,
                null_context_object_source_state.target_match_count,
                null_context_object_source_state.capped ? "true" : "false",
                static_cast<unsigned>(object_source_boundary),
                static_cast<unsigned>(null_context_object_source_state.dynamic_stack_slots.size()));
        }
        if (allocation_trace)
        {
            RecordDiagnostic("{\"event\":\"null_context_allocation_trace_boundary\",\"reason\":\"event_cap\",\"events\":%u,\"armed\":%u,\"hits\":%u,\"recorded\":%u,\"pending\":%u,\"capped\":%s}",
                             event_count,
                             allocation_trace_state.armed_count,
                             allocation_trace_state.hit_count,
                             allocation_trace_state.recorded_count,
                             static_cast<unsigned>(PendingAllocationReturnCount(
                                 pending_allocation_returns)),
                             allocation_trace_state.capped ? "true" : "false");
        }
        if (null_context_field_writer_trace)
        {
            RecordDiagnostic("{\"event\":\"null_context_field_writer_trace_boundary\",\"reason\":\"event_cap\",\"events\":%u,\"hits\":%u,\"recorded\":%u,\"capped\":%s}",
                             event_count,
                             null_context_field_writer_state.hit_count,
                             null_context_field_writer_state.recorded_count,
                             null_context_field_writer_state.capped ? "true" : "false");
        }
        if (null_context_field_access_trace)
        {
            RecordDiagnostic("{\"event\":\"null_context_field_access_trace_boundary\",\"reason\":\"event_cap\",\"events\":%u,\"hits\":%u,\"recorded\":%u,\"capped\":%s}",
                             event_count,
                             null_context_field_access_state.hit_count,
                             null_context_field_access_state.recorded_count,
                             null_context_field_access_state.capped ? "true" : "false");
        }
        if (null_context_field_reference_execution_trace)
        {
            RecordDiagnostic(
                "{\"event\":\"null_context_field_reference_execution_trace_boundary\",\"reason\":\"event_cap\",\"events\":%u,\"hits\":%u,\"recorded\":%u,\"target_matches\":%u,\"pending\":%u,\"capped\":%s}",
                event_count,
                null_context_field_reference_execution_state.hit_count,
                null_context_field_reference_execution_state.recorded_count,
                null_context_field_reference_execution_state.target_match_count,
                static_cast<unsigned>(null_context_field_reference_execution_state
                                          .pending_single_step_threads.size()),
                null_context_field_reference_execution_state.capped ? "true" : "false");
        }
        if (null_context_object_state_trace)
        {
            RecordDiagnostic(
                "{\"event\":\"null_context_object_state_trace_boundary\",\"reason\":\"event_cap\",\"events\":%u,\"hits\":%u,\"recorded\":%u,\"frames\":%u,\"window_readable\":%s,\"disarmed\":%s}",
                event_count,
                null_context_object_state_state.hit_count,
                null_context_object_state_state.recorded_count,
                null_context_object_state_state.frame_count,
                null_context_object_state_state.window_readable ? "true" : "false",
                null_context_object_state_state.disarmed ? "true" : "false");
        }
        if (null_context_object_reference_scan)
        {
            RecordDiagnostic(
                "{\"event\":\"null_context_object_reference_scan_boundary\",\"reason\":\"event_cap\",\"events\":%u,\"hits\":%u,\"scanned\":%s,\"text_readable\":%s,\"matches\":%u,\"capped\":%s}",
                event_count,
                null_context_object_reference_state.hit_count,
                null_context_object_reference_state.scanned ? "true" : "false",
                null_context_object_reference_state.text_readable ? "true" : "false",
                null_context_object_reference_state.match_count,
                null_context_object_reference_state.capped ? "true" : "false");
        }
        if (null_context_entry_trace)
        {
            RecordDiagnostic(
                "{\"event\":\"null_context_entry_trace_boundary\",\"reason\":\"event_cap\",\"events\":%u,\"hits\":%u,\"recorded\":%u,\"singleton_receivers\":%u,\"pending\":%u,\"capped\":%s}",
                event_count,
                null_context_entry_state.hit_count,
                null_context_entry_state.recorded_count,
                null_context_entry_state.singleton_receiver_count,
                static_cast<unsigned>(null_context_entry_state.pending_single_step_threads.size()),
                null_context_entry_state.capped ? "true" : "false");
        }
        return true;
    }
    *error = "ExitProcess breakpoint was not observed";
    return false;
}

bool FindOptionalIatSlotByName(const re2dj::exe::PeImageInfo& info,
                               const std::uint8_t* file,
                               std::size_t file_size,
                               const std::string& module,
                               const std::string& function,
                               std::uint32_t* slot_rva,
                               bool* present,
                               std::string* error)
{
    if (slot_rva == nullptr || present == nullptr || error == nullptr)
    {
        return false;
    }
    error->clear();
    if (re2dj::tools::windows_original_process_probe::FindIatSlotByName(
            info, file, file_size, module, function, slot_rva, error))
    {
        *present = true;
        return true;
    }
    if (*error == "requested import is not present")
    {
        error->clear();
        *present = false;
        return true;
    }
    return false;
}

bool FindOptionalIatSlotsByName(const re2dj::exe::PeImageInfo& info,
                                const std::uint8_t* file,
                                std::size_t file_size,
                                const std::string& module,
                                const std::string& function,
                                std::vector<std::uint32_t>* slot_rvas,
                                bool* present,
                                std::string* error)
{
    if (slot_rvas == nullptr || present == nullptr || error == nullptr)
    {
        return false;
    }
    error->clear();
    if (re2dj::tools::windows_original_process_probe::FindIatSlotsByName(
            info, file, file_size, module, function, slot_rvas, error))
    {
        *present = true;
        return true;
    }
    if (*error == "requested import is not present")
    {
        error->clear();
        slot_rvas->clear();
        *present = false;
        return true;
    }
    return false;
}

}  // namespace

int re2dj::platform::windows::RunOriginalProcessLauncherCommand(int argc, char** argv)
{
    std::filesystem::path hdd_path;
    std::filesystem::path chd_path;
    std::string target_id = "ez2dj1stse";
    std::string target_executable_path;
    bool trace = false;
    bool software_breakpoint = false;
    bool instruction_trace = false;
    std::uint32_t instruction_trace_max_steps = 0;
    bool inject_runtime = false;
    bool probe_handoff = false;
    bool hle_command_line = false;
    bool hle_windows_directory = false;
    bool hle_vfs = false;
    bool force_dynamic_vfs_resolver = false;
    bool hle_display_mode = false;
    bool hle_d3d3 = false;
    bool fullscreen = false;
    bool hle_directsound = false;
    float audio_gain_db = 0.0f;
    bool audio_gain_set = false;
    std::uint32_t demo_volume = 3;
    bool demo_volume_set = false;
    bool audio_volume_trace = false;
    bool hle_io_ports = false;
    std::filesystem::path io_config_path;
    bool run_detached = false;
    bool hle_message_box = false;
    bool d3d_init_trace = false;
    bool ksnd_load_trace = false;
    bool device_mock_lptdi = false;
    bool device_mock_lptdi_ioctl_success = false;
    bool device_mock_lptdi_ioctl_full_success = false;
    bool device_mock_wts_console_session = false;
    std::string device_mock_hardlock_450_response_hex;
    std::string device_mock_hardlock_44c_tail_hex;
    bool hardlock_device = false;
    std::string hardlock_transform_map_path;
    std::string device_mock_lptdi_path_prefix;
    std::filesystem::path device_mock_lptdi_response_profile_path;
    std::string device_mock_lptdi_target_state_hex;
    bool probe_exit_process = false;
    bool break_exit_process = false;
    bool scan_fault_references = false;
    // Constants to look for in the guest's decrypted .text at the first fault.
    std::vector<std::uint32_t> field_reference_scan_constants;
    // Guest address whose writes are watched, or zero for no watch.
    std::uintptr_t field_write_watch_address = 0;
    // Guest addresses whose surrounding bytes are captured at the first fault.
    std::vector<GuestCodeWindowRequest> code_windows;
    bool slot_writer_trace = false;
    bool null_context_object_source_trace = false;
    bool null_context_field_writer_early_trace = false;
    bool null_context_field_writer_trace = false;
    bool null_context_field_access_trace = false;
    bool null_context_field_reference_execution_trace = false;
    bool null_context_object_state_trace = false;
    bool null_context_object_reference_scan = false;
    bool null_context_entry_trace = false;
    bool null_context_allocation_trace = false;
    bool api_trace = false;
    bool system_api_trace = false;
    std::uint32_t lptdi_post_ioctl_trace_steps = 0;
    std::uint32_t lptdi_post_ioctl_trace_code = 0;
    std::uint32_t diagnostic_idle_timeout_ms = kDefaultDiagnosticIdleTimeoutMs;
    std::filesystem::path runtime_path;
    for (int index = 1; index < argc; ++index)
    {
        const std::string option = argv[index];
        if (option == "--hdd" && index + 1 < argc)
        {
            hdd_path = argv[++index];
        }
        else if (option == "--chd" && index + 1 < argc)
        {
            chd_path = argv[++index];
            hle_vfs = true;
            inject_runtime = true;
            software_breakpoint = true;
        }
        else if (option == "--target" && index + 1 < argc)
        {
            target_id = argv[++index];
        }
        else if (option == "--target-executable" && index + 1 < argc)
        {
            target_executable_path = argv[++index];
        }
        else if (option == "--trace")
        {
            trace = true;
        }
        else if (option == "--software-breakpoint")
        {
            software_breakpoint = true;
        }
        else if (option == "--instruction-trace" && index + 1 < argc)
        {
            try
            {
                instruction_trace_max_steps = static_cast<std::uint32_t>(
                    std::stoul(argv[++index]));
            }
            catch (const std::exception&)
            {
                PrintUsage();
                return 1;
            }
            if (instruction_trace_max_steps == 0)
            {
                PrintUsage();
                return 1;
            }
            instruction_trace = true;
            software_breakpoint = true;
        }
        else if (option == "--inject-runtime")
        {
            inject_runtime = true;
            if (index + 1 < argc && std::string(argv[index + 1]).rfind("--", 0) != 0)
            {
                runtime_path = argv[++index];
            }
        }
        else if (option == "--probe-handoff")
        {
            probe_handoff = true;
            inject_runtime = true;
            software_breakpoint = true;
        }
        else if (option == "--hle-command-line")
        {
            hle_command_line = true;
            inject_runtime = true;
            software_breakpoint = true;
        }
        else if (option == "--hle-windows-directory")
        {
            hle_windows_directory = true;
            inject_runtime = true;
            software_breakpoint = true;
        }
        else if (option == "--hle-vfs")
        {
            hle_vfs = true;
            inject_runtime = true;
            software_breakpoint = true;
        }
        // Diagnostic override for a target whose profile does not enable the
        // dynamic resolver. Without it a guest that resolves CreateFileA
        // through GetProcAddress reaches the real Win32 entry, so the device
        // mock never sees the protection's device open.
        else if (option == "--hle-dynamic-vfs")
        {
            force_dynamic_vfs_resolver = true;
            hle_vfs = true;
            inject_runtime = true;
            software_breakpoint = true;
        }
        else if (option == "--hle-display-mode")
        {
            // The display boundary is installed on every injected run because
            // the host mode is never changed. This option now only asks the
            // launcher to inject the runtime and to wait for the boundary's
            // first record, which is what the older diagnostic runs used it for.
            hle_display_mode = true;
            inject_runtime = true;
            software_breakpoint = true;
        }
        else if (option == "--hle-d3d3")
        {
            hle_d3d3 = true;
            inject_runtime = true;
            software_breakpoint = true;
        }
        else if (option == "--fullscreen")
        {
            fullscreen = true;
        }
        else if (option == "--hle-directsound")
        {
            hle_directsound = true;
            inject_runtime = true;
            software_breakpoint = true;
            break_exit_process = true;
        }
        else if (option == "--audio-gain-db" && index + 1 < argc)
        {
            try
            {
                std::size_t parsed = 0;
                const std::string value = argv[++index];
                audio_gain_db = std::stof(value, &parsed);
                if (parsed != value.size() || !std::isfinite(audio_gain_db) ||
                    audio_gain_db < -24.0f || audio_gain_db > 18.0f)
                {
                    throw std::out_of_range("audio gain");
                }
            }
            catch (const std::exception&)
            {
                PrintUsage();
                return 1;
            }
            audio_gain_set = true;
        }
        else if (option == "--audio-volume-trace")
        {
            audio_volume_trace = true;
            inject_runtime = true;
            software_breakpoint = true;
        }
        else if (option == "--demo-volume" && index + 1 < argc)
        {
            try
            {
                std::size_t parsed = 0;
                const std::string value = argv[++index];
                const unsigned long parsed_value = std::stoul(value, &parsed);
                if (parsed != value.size() || parsed_value > 3)
                {
                    throw std::out_of_range("demo volume");
                }
                demo_volume = static_cast<std::uint32_t>(parsed_value);
            }
            catch (const std::exception&)
            {
                PrintUsage();
                return 1;
            }
            demo_volume_set = true;
            inject_runtime = true;
            software_breakpoint = true;
        }
        else if (option == "--hle-io-ports")
        {
            hle_io_ports = true;
            break_exit_process = true;
            software_breakpoint = true;
        }
        else if (option == "--io-config" && index + 1 < argc)
        {
            io_config_path = argv[++index];
            hle_io_ports = true;
            inject_runtime = true;
            break_exit_process = true;
            software_breakpoint = true;
        }
        else if (option == "--hle-message-box")
        {
            hle_message_box = true;
            inject_runtime = true;
            software_breakpoint = true;
        }
        else if (option == "--run-detached")
        {
            run_detached = true;
        }
        else if (option == "--d3d-init-trace")
        {
            d3d_init_trace = true;
            break_exit_process = true;
            software_breakpoint = true;
        }
        else if (option == "--ksnd-load-trace")
        {
            ksnd_load_trace = true;
            break_exit_process = true;
            software_breakpoint = true;
        }
        else if (option == "--device-mock-lptdi")
        {
            device_mock_lptdi = true;
            hle_vfs = true;
            inject_runtime = true;
            software_breakpoint = true;
        }
        else if (option == "--device-mock-lptdi-path-prefix" && index + 1 < argc)
        {
            device_mock_lptdi_path_prefix = argv[++index];
            device_mock_lptdi = true;
            hle_vfs = true;
            inject_runtime = true;
            software_breakpoint = true;
        }
        else if (option == "--device-mock-lptdi-ioctl-success")
        {
            device_mock_lptdi_ioctl_success = true;
            device_mock_lptdi = true;
            hle_vfs = true;
            inject_runtime = true;
            software_breakpoint = true;
        }
        else if (option == "--device-mock-lptdi-ioctl-full-success")
        {
            device_mock_lptdi_ioctl_full_success = true;
            device_mock_lptdi = true;
            hle_vfs = true;
            inject_runtime = true;
            software_breakpoint = true;
        }
        else if (option == "--device-mock-wts-console-session")
        {
            device_mock_wts_console_session = true;
            device_mock_lptdi = true;
            hle_vfs = true;
            inject_runtime = true;
            software_breakpoint = true;
        }
        else if (option == "--device-mock-hardlock-450-response" &&
                 index + 1 < argc)
        {
            device_mock_hardlock_450_response_hex = argv[++index];
            device_mock_lptdi = true;
            hle_vfs = true;
            inject_runtime = true;
            software_breakpoint = true;
        }
        else if (option == "--device-mock-hardlock-44c-tail" &&
                 index + 1 < argc)
        {
            device_mock_hardlock_44c_tail_hex = argv[++index];
            device_mock_lptdi = true;
            hle_vfs = true;
            inject_runtime = true;
            software_breakpoint = true;
        }
        else if (option == "--hardlock-transform-map" && index + 1 < argc)
        {
            hardlock_transform_map_path = argv[++index];
            hardlock_device = true;
            device_mock_lptdi = true;
            hle_vfs = true;
            inject_runtime = true;
            software_breakpoint = true;
        }
        else if (option == "--hardlock-device")
        {
            hardlock_device = true;
            device_mock_lptdi = true;
            hle_vfs = true;
            inject_runtime = true;
            software_breakpoint = true;
        }
        else if (option == "--device-mock-lptdi-response-profile" && index + 1 < argc)
        {
            device_mock_lptdi_response_profile_path = argv[++index];
            device_mock_lptdi = true;
            hle_vfs = true;
            inject_runtime = true;
            software_breakpoint = true;
        }
        else if (option == "--device-mock-lptdi-target-state" && index + 1 < argc)
        {
            device_mock_lptdi_target_state_hex = argv[++index];
            device_mock_lptdi = true;
            hle_vfs = true;
            inject_runtime = true;
            software_breakpoint = true;
        }
        else if (option == "--diagnostic-idle-timeout" && index + 1 < argc)
        {
            try
            {
                diagnostic_idle_timeout_ms = static_cast<std::uint32_t>(
                    std::stoul(argv[++index]));
            }
            catch (const std::exception&)
            {
                PrintUsage();
                return 1;
            }
            if (diagnostic_idle_timeout_ms < kMinimumDiagnosticIdleTimeoutMs ||
                diagnostic_idle_timeout_ms > kMaximumDiagnosticIdleTimeoutMs)
            {
                PrintUsage();
                return 1;
            }
        }
        else if (option == "--lptdi-post-ioctl-trace" && index + 1 < argc)
        {
            try
            {
                lptdi_post_ioctl_trace_steps = static_cast<std::uint32_t>(
                    std::stoul(argv[++index]));
            }
            catch (const std::exception&)
            {
                PrintUsage();
                return 1;
            }
            if (lptdi_post_ioctl_trace_steps == 0 ||
                lptdi_post_ioctl_trace_steps > 4096)
            {
                PrintUsage();
                return 1;
            }
            api_trace = true;
            break_exit_process = true;
            software_breakpoint = true;
        }
        else if (option == "--lptdi-post-ioctl-code" && index + 1 < argc)
        {
            try
            {
                std::size_t parsed = 0;
                const std::string value = argv[++index];
                const unsigned long parsed_value = std::stoul(value, &parsed, 0);
                if (parsed != value.size() ||
                    parsed_value > (std::numeric_limits<std::uint32_t>::max)())
                {
                    throw std::out_of_range("post IOCTL trace code");
                }
                lptdi_post_ioctl_trace_code = static_cast<std::uint32_t>(parsed_value);
            }
            catch (const std::exception&)
            {
                PrintUsage();
                return 1;
            }
        }
        else if (option == "--probe-exit-process")
        {
            probe_exit_process = true;
            inject_runtime = true;
            software_breakpoint = true;
        }
        else if (option == "--break-exit-process")
        {
            break_exit_process = true;
            software_breakpoint = true;
        }
        else if (option == "--scan-fault-references")
        {
            scan_fault_references = true;
            break_exit_process = true;
            software_breakpoint = true;
        }
        else if (option == "--code-window" && index + 1 < argc)
        {
            try
            {
                const std::string value = argv[++index];
                const std::size_t separator = value.find(':');
                const std::string address_text = value.substr(0, separator);
                std::size_t parsed = 0;
                const unsigned long address_value =
                    std::stoul(address_text, &parsed, 16);
                if (parsed != address_text.size() || address_value == 0 ||
                    address_value > (std::numeric_limits<std::uint32_t>::max)())
                {
                    throw std::out_of_range("code window address");
                }
                GuestCodeWindowRequest request;
                request.address = static_cast<std::uintptr_t>(address_value);
                if (separator != std::string::npos)
                {
                    const std::string length_text = value.substr(separator + 1);
                    std::size_t length_parsed = 0;
                    const unsigned long length_value =
                        std::stoul(length_text, &length_parsed, 16);
                    if (length_parsed != length_text.size() || length_value == 0 ||
                        length_value > kGuestCodeWindowMaxLength)
                    {
                        throw std::out_of_range("code window length");
                    }
                    request.length = static_cast<std::uint32_t>(length_value);
                }
                code_windows.push_back(request);
                // The capture runs from the debugger's fault handler.
                break_exit_process = true;
                software_breakpoint = true;
            }
            catch (const std::exception&)
            {
                PrintUsage();
                return 1;
            }
        }
        else if (option == "--field-write-watch" && index + 1 < argc)
        {
            try
            {
                std::size_t parsed = 0;
                const std::string value = argv[++index];
                const unsigned long parsed_value = std::stoul(value, &parsed, 16);
                if (parsed != value.size() || parsed_value == 0 ||
                    parsed_value > (std::numeric_limits<std::uint32_t>::max)())
                {
                    throw std::out_of_range("field write watch address");
                }
                // x86 four-byte data breakpoints require a four-byte aligned
                // address. Accepting an unaligned one would watch a range the
                // caller did not ask for and report writes that never touched
                // the field.
                if ((parsed_value & 0x3u) != 0)
                {
                    throw std::out_of_range("field write watch alignment");
                }
                field_write_watch_address = static_cast<std::uintptr_t>(parsed_value);
                break_exit_process = true;
                software_breakpoint = true;
            }
            catch (const std::exception&)
            {
                PrintUsage();
                return 1;
            }
        }
        else if (option == "--field-reference-scan" && index + 1 < argc)
        {
            // Repeatable: one run can look for a field displacement and the
            // same field's absolute address at once, which is the pair a
            // structure field is addressed by.
            try
            {
                std::size_t parsed = 0;
                const std::string value = argv[++index];
                const unsigned long parsed_value = std::stoul(value, &parsed, 16);
                if (parsed != value.size() ||
                    parsed_value > (std::numeric_limits<std::uint32_t>::max)())
                {
                    throw std::out_of_range("field reference constant");
                }
                field_reference_scan_constants.push_back(
                    static_cast<std::uint32_t>(parsed_value));
                // The scan runs from the debugger's fault handler, so the run
                // has to be traced for it to happen at all.
                break_exit_process = true;
                software_breakpoint = true;
            }
            catch (const std::exception&)
            {
                PrintUsage();
                return 1;
            }
        }
        else if (option == "--slot-writer-trace")
        {
            slot_writer_trace = true;
            break_exit_process = true;
            software_breakpoint = true;
        }
        else if (option == "--null-context-object-source-trace")
        {
            null_context_object_source_trace = true;
            break_exit_process = true;
            software_breakpoint = true;
        }
        else if (option == "--null-context-field-writer-trace")
        {
            null_context_field_writer_trace = true;
            break_exit_process = true;
            software_breakpoint = true;
        }
        else if (option == "--null-context-field-writer-early-trace")
        {
            null_context_field_writer_early_trace = true;
            break_exit_process = true;
            software_breakpoint = true;
        }
        else if (option == "--null-context-field-access-trace")
        {
            null_context_field_access_trace = true;
            break_exit_process = true;
            software_breakpoint = true;
        }
        else if (option == "--null-context-field-reference-execution-trace")
        {
            null_context_field_reference_execution_trace = true;
            break_exit_process = true;
            software_breakpoint = true;
        }
        else if (option == "--null-context-object-state-trace")
        {
            null_context_object_state_trace = true;
            break_exit_process = true;
            software_breakpoint = true;
        }
        else if (option == "--null-context-object-reference-scan")
        {
            null_context_object_reference_scan = true;
            break_exit_process = true;
            software_breakpoint = true;
        }
        else if (option == "--null-context-entry-trace")
        {
            null_context_entry_trace = true;
            break_exit_process = true;
            software_breakpoint = true;
        }
        else if (option == "--null-context-allocation-trace")
        {
            null_context_allocation_trace = true;
            api_trace = true;
            system_api_trace = true;
            break_exit_process = true;
            software_breakpoint = true;
        }
        else if (option == "--api-trace")
        {
            api_trace = true;
            system_api_trace = true;
            break_exit_process = true;
            software_breakpoint = true;
        }
        else
        {
            PrintUsage();
            return 1;
        }
    }
    if (hdd_path.empty())
    {
        PrintUsage();
        return 1;
    }
    if (!chd_path.empty() && !std::filesystem::is_regular_file(chd_path))
    {
        std::fprintf(stderr, "{\"error\":\"CHD path does not exist\"}\n");
        return 1;
    }
    if (!chd_path.empty())
    {
        // The injected runtime opens this path from inside the child, whose
        // working directory is the guest's, not the launcher's. A relative
        // path given on the command line would silently fail to mount there.
        std::error_code chd_absolute_error;
        const std::filesystem::path absolute_chd_path =
            std::filesystem::absolute(chd_path, chd_absolute_error);
        if (chd_absolute_error)
        {
            std::fprintf(stderr, "{\"error\":\"CHD path cannot be made absolute\"}\n");
            return 1;
        }
        chd_path = absolute_chd_path;
    }
    if (!target_executable_path.empty() &&
        std::filesystem::path(target_executable_path).is_absolute())
    {
        std::fprintf(stderr, "{\"error\":\"target executable path must be relative\"}\n");
        return 1;
    }
    const unsigned device_ioctl_policy_count =
        (device_mock_lptdi_ioctl_success ? 1u : 0u) +
        (device_mock_lptdi_ioctl_full_success ? 1u : 0u) +
        (!device_mock_lptdi_response_profile_path.empty() ? 1u : 0u) +
        (!device_mock_lptdi_target_state_hex.empty() ? 1u : 0u);
    if (device_ioctl_policy_count > 1)
    {
        PrintUsage();
        return 1;
    }
    if (lptdi_post_ioctl_trace_steps != 0 &&
        device_ioctl_policy_count == 0)
    {
        PrintUsage();
        return 1;
    }
    if (lptdi_post_ioctl_trace_code != 0 && lptdi_post_ioctl_trace_steps == 0)
    {
        PrintUsage();
        return 1;
    }
    if (null_context_field_writer_early_trace && !null_context_field_access_trace)
    {
        null_context_field_writer_trace = true;
    }
    if (null_context_field_writer_trace && null_context_field_access_trace)
    {
        PrintUsage();
        return 1;
    }
    if (null_context_object_source_trace && slot_writer_trace)
    {
        std::fprintf(stderr,
                     "{\"error\":\"null-context object source trace conflicts with slot-writer trace\"}\n");
        return 1;
    }
    if (null_context_field_reference_execution_trace &&
        (slot_writer_trace || null_context_object_source_trace ||
         null_context_field_writer_early_trace || null_context_field_writer_trace ||
         null_context_field_access_trace || null_context_object_state_trace ||
         null_context_object_reference_scan || null_context_entry_trace))
    {
        std::fprintf(
            stderr,
            "{\"error\":\"null-context field reference execution trace conflicts with another hardware trace\"}\n");
        return 1;
    }
    if ((null_context_object_state_trace || null_context_object_reference_scan ||
         null_context_entry_trace) &&
        (slot_writer_trace || null_context_object_source_trace ||
         null_context_field_writer_early_trace || null_context_field_writer_trace ||
         null_context_field_access_trace ||
         null_context_field_reference_execution_trace ||
         (null_context_object_state_trace && null_context_object_reference_scan)))
    {
        std::fprintf(
            stderr,
            "{\"error\":\"null-context object state trace conflicts with another hardware trace\"}\n");
        return 1;
    }
    if ((audio_gain_set || audio_volume_trace) && !hle_directsound)
    {
        PrintUsage();
        return 1;
    }
    if (fullscreen && !hle_d3d3)
    {
        PrintUsage();
        return 1;
    }
    if (!io_config_path.empty() && !run_detached)
    {
        PrintUsage();
        return 1;
    }
    if (instruction_trace && (probe_handoff || hle_command_line || hle_windows_directory ||
                              hle_vfs || hle_display_mode || hle_d3d3 || hle_directsound || hle_io_ports || d3d_init_trace || ksnd_load_trace || probe_exit_process ||
                              break_exit_process || null_context_object_source_trace ||
                              null_context_field_writer_early_trace ||
                              null_context_field_reference_execution_trace ||
                              null_context_object_state_trace ||
                              null_context_allocation_trace))
    {
        PrintUsage();
        return 1;
    }
    // Debug register 1 carries the field write watch, and these two diagnostics
    // claim it as well. Running them together would leave both reporting
    // whichever armed last, so the combination is refused rather than trusted.
    if (field_write_watch_address != 0 &&
        (slot_writer_trace || null_context_field_reference_execution_trace))
    {
        PrintUsage();
        return 1;
    }
    if (run_detached &&
        (trace || instruction_trace || d3d_init_trace || ksnd_load_trace ||
         api_trace || probe_exit_process || scan_fault_references || slot_writer_trace ||
         !field_reference_scan_constants.empty() || field_write_watch_address != 0 ||
         !code_windows.empty() ||
         null_context_field_writer_trace ||
         null_context_field_access_trace ||
         null_context_object_source_trace ||
         null_context_field_writer_early_trace ||
         null_context_field_reference_execution_trace ||
         null_context_object_state_trace ||
         null_context_allocation_trace ||
         lptdi_post_ioctl_trace_steps != 0))
    {
        PrintUsage();
        return 1;
    }
    if (run_detached)
    {
        break_exit_process = false;
    }

    g_trace = trace;

    std::string error;
    re2dj::device::LptdiTargetState device_target_state = {};
    re2dj::hle::hardlock::HardlockHandshakeResponse hardlock_450_response = {};
    std::uint16_t hardlock_44c_tail_word = 0;
    std::vector<re2dj::hle::hardlock::HardlockTransformResponseEntry> hardlock_transform_map;
    // Deferred until the target is known, because a profile default may fill
    // these in from cfg after the command line has been read.
    const auto parse_hardlock_material = [&]() -> bool {
        if (!device_mock_hardlock_44c_tail_hex.empty() &&
            !re2dj::hle::hardlock::ParseHardlockApiTailWordHex(
                device_mock_hardlock_44c_tail_hex,
                &hardlock_44c_tail_word,
                &error))
        {
            std::fprintf(stderr, "{\"error\":\"%s\"}\n", error.c_str());
            return false;
        }
        if (!device_mock_hardlock_450_response_hex.empty() &&
            !re2dj::hle::hardlock::ParseHardlockHandshakeResponse(
                device_mock_hardlock_450_response_hex,
                &hardlock_450_response,
                &error))
        {
            std::fprintf(stderr, "{\"error\":\"%s\"}\n", error.c_str());
            return false;
        }
        if (!hardlock_transform_map_path.empty())
        {
            std::ifstream map_stream(hardlock_transform_map_path, std::ios::binary);
            if (!map_stream)
            {
                std::fprintf(stderr,
                             "{\"error\":\"cannot open Hardlock transform map\"}\n");
                return false;
            }
            const std::string map_text((std::istreambuf_iterator<char>(map_stream)),
                                       std::istreambuf_iterator<char>());
            if (!re2dj::hle::hardlock::ParseHardlockTransformResponseTable(
                    map_text, &hardlock_transform_map, &error))
            {
                std::fprintf(stderr, "{\"error\":\"%s\"}\n", error.c_str());
                return false;
            }
        }
        return true;
    };
    if (!device_mock_lptdi_target_state_hex.empty() &&
        !re2dj::device::ParseLptdiTargetState(
            device_mock_lptdi_target_state_hex, &device_target_state, &error))
    {
        std::fprintf(stderr, "{\"error\":\"%s\"}\\n", error.c_str());
        return 1;
    }
    re2dj::device::LptdiResponseProfile device_response_profile;
    if (!device_mock_lptdi_response_profile_path.empty() &&
        !re2dj::device::ReadLptdiResponseProfile(
            device_mock_lptdi_response_profile_path,
            &device_response_profile,
            &error))
    {
        std::fprintf(stderr, "{\"error\":\"%s\"}\\n", error.c_str());
        return 1;
    }
    if (inject_runtime && runtime_path.empty() && !FindBundledRuntime(&runtime_path, &error))
    {
        std::fprintf(stderr, "{\"error\":\"%s\"}\\n", error.c_str());
        return 1;
    }
    if (inject_runtime && !std::filesystem::is_regular_file(runtime_path))
    {
        std::fprintf(stderr, "{\"error\":\"injected runtime does not exist\"}\\n");
        return 1;
    }
    if (!io_config_path.empty())
    {
        std::error_code path_error;
        io_config_path = std::filesystem::absolute(io_config_path, path_error);
        if (path_error || !std::filesystem::is_regular_file(io_config_path))
        {
            std::fprintf(stderr, "{\"error\":\"I/O configuration does not exist\"}\n");
            return 1;
        }
    }

    re2dj::hdd::HddRoot root;
    if (!re2dj::hdd::HddRoot::Open(hdd_path, &root, &error))
    {
        std::fprintf(stderr, "{\"error\":\"%s\"}\\n", error.c_str());
        return 2;
    }
    const re2dj::hdd::HddScanResult scan = re2dj::hdd::ScanHdd(root);
    const std::vector<re2dj::target::TargetProfile> profiles =
        re2dj::target::BuildTargetProfiles(root, scan);
    re2dj::target::TargetProfile explicit_target;
    const re2dj::target::TargetProfile* target = nullptr;
    if (!target_executable_path.empty())
    {
        const re2dj::target::BuiltInTargetProfile* built_in =
            re2dj::target::FindBuiltInTargetProfileById(target_id);
        if (built_in == nullptr)
        {
            std::fprintf(stderr, "{\"error\":\"explicit executable requires a built-in target profile\"}\n");
            return 2;
        }
        explicit_target = built_in->profile;
        explicit_target.executable_relative_path = target_executable_path;
        explicit_target.working_directory_relative_path =
            std::filesystem::path(target_executable_path).parent_path().generic_string();
        explicit_target.detected = false;
        target = &explicit_target;
    }
    else
    {
        target = re2dj::target::FindTargetProfileById(profiles, target_id);
    }
    std::filesystem::path executable;
    re2dj::exe::PeImageInfo info;
    if (target == nullptr ||
        !root.ResolveFile(target->executable_relative_path, &executable) ||
        !re2dj::exe::ReadPeImageInfo(executable, &info, &error) ||
        !re2dj::exe::IsGuestExecutable(info) ||
        info.image_base > (std::numeric_limits<std::uint32_t>::max)())
    {
        std::fprintf(stderr, "{\"error\":\"cannot resolve valid bring-up target\"}\\n");
        return 2;
    }
    if (d3d_init_trace && target->id != "ez2dj1stse")
    {
        std::fprintf(stderr, "{\"error\":\"Direct3D initialization trace requires ez2dj1stse target\"}\\n");
        return 2;
    }
    if (ksnd_load_trace && target->id != "ez2dj1stse")
    {
        std::fprintf(stderr, "{\"error\":\"KSND load trace requires ez2dj1stse target\"}\\n");
        return 2;
    }
    if (slot_writer_trace && target->id != "ez2dj4th")
    {
        std::fprintf(stderr, "{\"error\":\"slot-writer trace requires ez2dj4th target\"}\n");
        return 2;
    }
    if (null_context_object_source_trace && target->id != "ez2dj4th")
    {
        std::fprintf(stderr, "{\"error\":\"null-context object source trace requires ez2dj4th target\"}\n");
        return 2;
    }
    if (null_context_field_writer_early_trace && target->id != "ez2dj4th")
    {
        std::fprintf(stderr, "{\"error\":\"early null-context field writer trace requires ez2dj4th target\"}\n");
        return 2;
    }
    if (null_context_field_writer_trace && target->id != "ez2dj4th")
    {
        std::fprintf(stderr, "{\"error\":\"null-context field writer trace requires ez2dj4th target\"}\n");
        return 2;
    }
    if (null_context_field_access_trace && target->id != "ez2dj4th")
    {
        std::fprintf(stderr, "{\"error\":\"null-context field access trace requires ez2dj4th target\"}\n");
        return 2;
    }
    if (null_context_entry_trace && target->id != "ez2dj4th")
    {
        std::fprintf(stderr,
                     "{\"error\":\"null-context entry trace supports ez2dj4th only\"}\n");
        return 1;
    }
    if (null_context_object_reference_scan && target->id != "ez2dj4th")
    {
        std::fprintf(stderr,
                     "{\"error\":\"null-context object reference scan supports ez2dj4th only\"}\n");
        return 1;
    }
    if (null_context_object_state_trace && target->id != "ez2dj4th")
    {
        std::fprintf(stderr,
                     "{\"error\":\"null-context object state trace supports ez2dj4th only\"}\n");
        return 1;
    }
    if (null_context_field_reference_execution_trace && target->id != "ez2dj4th")
    {
        std::fprintf(
            stderr,
            "{\"error\":\"null-context field reference execution trace requires ez2dj4th target\"}\n");
        return 2;
    }
    if (null_context_allocation_trace && target->id != "ez2dj4th")
    {
        std::fprintf(stderr, "{\"error\":\"null-context allocation trace requires ez2dj4th target\"}\n");
        return 2;
    }
    if (hle_d3d3 && !target->run_defaults.hle_d3d3)
    {
        std::fprintf(stderr, "{\"error\":\"Direct3D3 HLE is not configured for this target\"}\\n");
        return 2;
    }
    if (hle_directsound && !target->run_defaults.hle_directsound)
    {
        std::fprintf(stderr, "{\"error\":\"DirectSound HLE is not configured for this target\"}\\n");
        return 2;
    }
    if (hle_io_ports && !target->run_defaults.lptdi.legacy_io_ports)
    {
        std::fprintf(stderr, "{\"error\":\"legacy I/O port HLE is not configured for this target\"}\\n");
        return 2;
    }
    const LegacyIoTrapPolicy io_policy = {
        static_cast<std::uintptr_t>(target->run_defaults.lptdi.legacy_io_in_byte_rva),
        static_cast<std::uintptr_t>(target->run_defaults.lptdi.legacy_io_out_byte_rva)};
    if (device_mock_lptdi && !target->run_defaults.lptdi.device_mock_enabled)
    {
        std::fprintf(stderr, "{\"error\":\"LPTDI device mock is not configured for this target\"}\\n");
        return 2;
    }
    bool hardlock_cfg_replay = false;
    bool hardlock_cfg_tail = false;
    bool hardlock_cfg_map = false;
    // Profile default: take Hardlock material from the conventional cfg paths
    // when the user has put it there. Nothing in this repository produces those
    // values, an explicit option outranks the file, and absence is not an error
    // because the run is still a valid observation without them.
    if (target->run_defaults.lptdi.hardlock_cfg_material_default)
    {
        re2dj::config::HardlockSecretMaterial cfg_material;
        bool cfg_section_found = false;
        if (!re2dj::config::LoadHardlockProfileMaterial(
                re2dj::config::DefaultHardlockSecretConfigPath(),
                target->id,
                &cfg_material,
                &cfg_section_found,
                &error))
        {
            std::fprintf(stderr, "{\"error\":\"%s\"}\\n", error.c_str());
            return 2;
        }
        if (hardlock_transform_map_path.empty())
        {
            const std::filesystem::path default_map =
                re2dj::config::DefaultHardlockTransformMapPath(target->id);
            std::error_code map_code;
            if (!default_map.empty() && std::filesystem::exists(default_map, map_code) &&
                !map_code)
            {
                hardlock_transform_map_path = default_map.string();
                // The map is only consumed by the stub, so selecting one here
                // has to enable the same boundary the explicit option does.
                hardlock_device = true;
                hardlock_cfg_map = true;
            }
        }
        // The device replay values are applied only alongside a map, because
        // the three materials only make sense together: with the replay but no
        // map the protection reaches its own modal dialog and waits there,
        // which is worse than stopping at the earlier boundary.
        if (cfg_section_found && !hardlock_transform_map_path.empty())
        {
            if (device_mock_hardlock_450_response_hex.empty() &&
                !cfg_material.handshake_response_hex.empty())
            {
                device_mock_hardlock_450_response_hex = cfg_material.handshake_response_hex;
                hardlock_cfg_replay = true;
            }
            if (device_mock_hardlock_44c_tail_hex.empty() &&
                !cfg_material.descriptor_tail_hex.empty())
            {
                device_mock_hardlock_44c_tail_hex = cfg_material.descriptor_tail_hex;
                hardlock_cfg_tail = true;
            }
        }
    }
    if (!parse_hardlock_material())
    {
        return 2;
    }
    const std::string profile_device_mock_path_prefix =
        device_mock_lptdi_path_prefix.empty()
            ? target->run_defaults.lptdi.device_mock_path_prefix
            : device_mock_lptdi_path_prefix;
    if (device_mock_lptdi && profile_device_mock_path_prefix.empty())
    {
        std::fprintf(stderr, "{\"error\":\"LPTDI device mock has no path prefix\"}\\n");
        return 2;
    }
    std::filesystem::path vfs_source_root = root.root();
    if (hle_vfs && !target->working_directory_relative_path.empty() &&
        !root.ResolveDirectory(target->working_directory_relative_path, &vfs_source_root))
    {
        std::fprintf(stderr,
                     "{\"error\":\"cannot resolve target working directory for VFS mount\"}\\n");
        return 2;
    }
    std::vector<std::uint8_t> file;
    if (!ReadFile(executable, &file, &error))
    {
        std::fprintf(stderr, "{\"error\":\"%s\"}\\n", error.c_str());
        return 2;
    }
    DiagnosticLog diagnostic_log;
    if (!diagnostic_log.Open(target->id, &error))
    {
        std::fprintf(stderr, "{\"error\":\"%s\"}\n", error.c_str());
        return 2;
    }
    g_diagnostic_log = &diagnostic_log;
    RecordDiagnostic("{\"event\":\"launch\",\"target\":\"%s\",\"executable\":\"%s\",\"chd\":\"%s\",\"trace\":%s,\"software_breakpoint\":%s,\"instruction_trace_steps\":%u,\"api_trace\":%s,\"slot_writer_trace\":%s,\"null_context_object_source_trace\":%s,\"null_context_field_writer_early_trace\":%s,\"null_context_field_writer_trace\":%s,\"null_context_field_access_trace\":%s,\"null_context_field_reference_execution_trace\":%s,\"null_context_object_state_trace\":%s,\"null_context_allocation_trace\":%s,\"hle_display_mode\":%s,\"hle_d3d3\":%s,\"fullscreen\":%s,\"hle_directsound\":%s,\"hle_io_ports\":%s,\"hle_message_box\":%s,\"run_detached\":%s,\"d3d_init_trace\":%s,\"ksnd_load_trace\":%s,\"device_mock_lptdi\":%s,\"device_mock_lptdi_ioctl_success\":%s,\"device_mock_lptdi_ioctl_full_success\":%s,\"device_mock_wts_console_session\":%s,\"device_response_profile_entries\":%u,\"device_target_state\":%s,\"lptdi_post_ioctl_trace_steps\":%u,\"lptdi_post_ioctl_trace_code\":\"0x%08x\",\"diagnostic_idle_timeout_ms\":%u}",
                     target->id.c_str(),
                     executable.generic_string().c_str(),
                     chd_path.generic_string().c_str(),
                     trace ? "true" : "false",
                     software_breakpoint ? "true" : "false",
                     instruction_trace_max_steps,
                     api_trace ? "true" : "false",
                     slot_writer_trace ? "true" : "false",
                     null_context_object_source_trace ? "true" : "false",
                     null_context_field_writer_early_trace ? "true" : "false",
                     null_context_field_writer_trace ? "true" : "false",
                     null_context_field_access_trace ? "true" : "false",
                     null_context_field_reference_execution_trace ? "true" : "false",
                     null_context_object_state_trace ? "true" : "false",
                     null_context_allocation_trace ? "true" : "false",
                     hle_display_mode ? "true" : "false",
                     hle_d3d3 ? "true" : "false",
                     fullscreen ? "true" : "false",
                     hle_directsound ? "true" : "false",
                     hle_io_ports ? "true" : "false",
                     hle_message_box ? "true" : "false",
                     run_detached ? "true" : "false",
                     d3d_init_trace ? "true" : "false",
                     ksnd_load_trace ? "true" : "false",
                     device_mock_lptdi ? "true" : "false",
                     device_mock_lptdi_ioctl_success ? "true" : "false",
                     device_mock_lptdi_ioctl_full_success ? "true" : "false",
                     device_mock_wts_console_session ? "true" : "false",
                     static_cast<unsigned>(device_response_profile.entries.size()),
                     device_mock_lptdi_target_state_hex.empty() ? "false" : "true",
                     lptdi_post_ioctl_trace_steps,
                     lptdi_post_ioctl_trace_code,
                     diagnostic_idle_timeout_ms);
    if (hardlock_cfg_replay || hardlock_cfg_tail || hardlock_cfg_map)
    {
        // Records which material a profile default applied, never its values.
        RecordDiagnostic("{\"event\":\"hardlock_cfg_material\",\"response450\":%s,\"tail44c\":%s,\"map\":%s}",
                         hardlock_cfg_replay ? "true" : "false",
                         hardlock_cfg_tail ? "true" : "false",
                         hardlock_cfg_map ? "true" : "false");
    }
    std::string hle_value = executable.filename().string();
    if (hle_windows_directory)
    {
        std::vector<wchar_t> module_path(32768, L'\0');
        const DWORD module_length = GetModuleFileNameW(
            nullptr, module_path.data(), static_cast<DWORD>(module_path.size()));
        if (module_length == 0 || module_length >= module_path.size())
        {
            PrintDiagnosticError("cannot determine re2dj executable path");
            return 2;
        }
        hle_value = (std::filesystem::path(module_path.data()).parent_path() / L"windows").string();
    }

    std::vector<wchar_t> command(executable.native().begin(), executable.native().end());
    command.push_back(L'\0');
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION child = {};
    if (CreateProcessW(executable.c_str(),
                       command.data(),
                       nullptr,
                       nullptr,
                       FALSE,
                       CREATE_NO_WINDOW | DEBUG_ONLY_THIS_PROCESS,
                       nullptr,
                       executable.parent_path().c_str(),
                       &startup,
                       &child) == FALSE)
    {
        PrintDiagnosticError("cannot create original process");
        return 3;
    }

    DWORD breakpoint_process_id = 0;
    DWORD breakpoint_thread_id = 0;
    std::uintptr_t main_image_base = 0;
    std::uintptr_t kernel32_base = 0;
    std::uintptr_t kernelbase_base = 0;
    std::uintptr_t user32_base = 0;
    const std::uint32_t expected_base = static_cast<std::uint32_t>(info.image_base);
    const std::uint32_t entry = expected_base + info.entry_point_rva;
    std::uint8_t original_entry_byte = 0;
    const bool reached_initial = WaitForInitialBreakpoint(child.hProcess,
                                                          &breakpoint_process_id,
                                                          &breakpoint_thread_id,
                                                          &main_image_base,
                                                          &kernel32_base,
                                                          &kernelbase_base,
                                                          &user32_base,
                                                          null_context_field_writer_early_trace,
                                                          &error);
    RecordDiagnostic("{\"event\":\"system_modules\",\"image_base\":\"0x%08x\",\"kernel32\":\"0x%08x\",\"kernelbase\":\"0x%08x\",\"user32\":\"0x%08x\"}",
                     static_cast<unsigned>(main_image_base),
                     static_cast<unsigned>(kernel32_base),
                     static_cast<unsigned>(kernelbase_base),
                     static_cast<unsigned>(user32_base));
    const bool breakpoint_set = reached_initial && main_image_base == expected_base &&
                                (software_breakpoint
                                     ? SetSoftwareEntryBreakpoint(child.hProcess,
                                                                  entry,
                                                                  &original_entry_byte,
                                                                  &error)
                                     : SetEntryBreakpoint(child.hThread, entry, &error));
    const bool reached = breakpoint_set &&
                         ContinueDebugEvent(breakpoint_process_id,
                                            breakpoint_thread_id,
                                            DBG_CONTINUE) != FALSE &&
                         WaitForEntryBreakpoint(entry,
                                                child.hProcess,
                                                &breakpoint_process_id,
                                                &breakpoint_thread_id,
                                                software_breakpoint,
                                                trace,
                                                &kernel32_base,
                                                &kernelbase_base,
                                                &user32_base,
                                                &error);
    const bool entry_restored = !software_breakpoint || !reached ||
                                RestoreSoftwareEntryBreakpoint(child.hProcess,
                                                               entry,
                                                               original_entry_byte,
                                                               &error);
    std::uint32_t runtime_base = 0;
    const bool runtime_loaded = !inject_runtime ||
                                (reached && entry_restored &&
                                 re2dj::platform::windows::LoadInjectedRuntime(child.hProcess,
                                                                                child.hThread,
                                                                                breakpoint_process_id,
                                                                                breakpoint_thread_id,
                                                                                runtime_path,
                                                                                &runtime_base,
                                                                                &error));
    const bool handoff_requested = probe_handoff || hle_command_line || hle_windows_directory;
    std::uint32_t original_target = 0;
    std::uint32_t hook_slot_rva = 0;
    std::uint32_t handoff_thunk_rva = 0;
    std::uint32_t handoff_target_rva = 0;
    const char* const thunk_export = hle_windows_directory ? "Re2djHleGetWindowsDirectoryA"
                                    : hle_command_line ? "Re2djHleGetCommandLineA"
                                                       : "Re2djProbeGetCommandLineA";
    const char* const data_export = hle_windows_directory ? "g_re2dj_hle_windows_directory"
                                   : hle_command_line ? "g_re2dj_hle_command_line"
                                                      : "g_re2dj_probe_original_target";
    const char* const handoff_message = hle_windows_directory ? "re2dj:hle:GetWindowsDirectoryA"
                                       : hle_command_line ? "re2dj:hle:GetCommandLineA"
                                                          : "re2dj:handoff:GetCommandLineA";
    const bool handoff_prepared = !handoff_requested ||
                                  (runtime_loaded &&
                                   re2dj::platform::windows::FindPe32ExportRva(
                                       runtime_path,
                                       thunk_export,
                                       &handoff_thunk_rva,
                                       &error) &&
                                   re2dj::platform::windows::FindPe32ExportRva(
                                       runtime_path,
                                       data_export,
                                       &handoff_target_rva,
                                       &error) &&
                                   re2dj::tools::windows_original_process_probe::FindIatSlotByName(
                                       info,
                                       file.data(),
                                       file.size(),
                                       "KERNEL32.dll",
                                       hle_windows_directory ? "GetWindowsDirectoryA" : "GetCommandLineA",
                                       &hook_slot_rva,
                                       &error) &&
                                   (!probe_handoff ||
                                    (ReadProcessMemory(child.hProcess,
                                                       reinterpret_cast<const void*>(main_image_base + hook_slot_rva),
                                                       &original_target,
                                                       sizeof(original_target),
                                                       nullptr) != FALSE &&
                                     WriteRemoteU32(child.hProcess,
                                                    runtime_base + handoff_target_rva,
                                                    original_target,
                                                    &error))) &&
                                   (!(hle_command_line || hle_windows_directory) ||
                                    WriteProcessMemory(child.hProcess,
                                                       reinterpret_cast<void*>(runtime_base + handoff_target_rva),
                                                       hle_value.c_str(),
                                                       hle_value.size() + 1,
                                                       nullptr) != FALSE) &&
                                   WriteRemoteU32(child.hProcess,
                                                  main_image_base + hook_slot_rva,
                                                  runtime_base + handoff_thunk_rva,
                                                  &error));
    // The host display mode is never changed, so this boundary is installed on
    // every run that injects the runtime rather than behind a diagnostic
    // option. A guest that does not import the entry point simply has nothing
    // to redirect.
    bool display_prepared = true;
    if (runtime_loaded)
    {
        struct DisplayEntryPoint
        {
            const char* import_name;
            const char* export_name;
        };
        constexpr DisplayEntryPoint kDisplayEntryPoints[] = {
            {"ChangeDisplaySettingsExA", "_Re2djHleChangeDisplaySettingsExA@20"},
            {"ChangeDisplaySettingsA", "_Re2djHleChangeDisplaySettingsA@8"},
        };
        for (const DisplayEntryPoint& display_entry : kDisplayEntryPoints)
        {
            std::uint32_t display_thunk_rva = 0;
            std::uint32_t display_slot_rva = 0;
            std::string display_find_err;
            if (!re2dj::tools::windows_original_process_probe::FindIatSlotByName(
                    info,
                    file.data(),
                    file.size(),
                    "USER32.dll",
                    display_entry.import_name,
                    &display_slot_rva,
                    &display_find_err))
            {
                continue;
            }
            if (!re2dj::platform::windows::FindPe32ExportRva(
                    runtime_path, display_entry.export_name, &display_thunk_rva, &error) ||
                !WriteRemoteU32(child.hProcess,
                                main_image_base + display_slot_rva,
                                runtime_base + display_thunk_rva,
                                &error))
            {
                display_prepared = false;
                break;
            }
        }
    }
    bool d3d3_prepared = !hle_d3d3;
    if (hle_d3d3 && runtime_loaded)
    {
        std::uint32_t d3d3_thunk_rva = 0;
        std::uint32_t d3d3_slot_rva = 0;
        std::uint32_t d3d3_ex_thunk_rva = 0;
        std::uint32_t d3d3_ex_slot_rva = 0;
        std::uint32_t graphics_trace_path_rva = 0;
        std::uint32_t fullscreen_rva = 0;
        const DWORD fullscreen_value = fullscreen ? TRUE : FALSE;
        std::filesystem::path graphics_trace_path = diagnostic_log.path();
        graphics_trace_path.replace_extension(".ddraw.log");

        std::string find_create_err;
        const bool has_create =
            re2dj::tools::windows_original_process_probe::FindIatSlotByName(
                info,
                file.data(),
                file.size(),
                "DDRAW.dll",
                "DirectDrawCreate",
                &d3d3_slot_rva,
                &find_create_err);

        std::string find_create_ex_err;
        const bool has_create_ex =
            re2dj::tools::windows_original_process_probe::FindIatSlotByName(
                info,
                file.data(),
                file.size(),
                "DDRAW.dll",
                "DirectDrawCreateEx",
                &d3d3_ex_slot_rva,
                &find_create_ex_err);

        if (!has_create && !has_create_ex)
        {
            error = "cannot find DirectDrawCreate or DirectDrawCreateEx IAT slot in DDRAW.dll: " +
                    find_create_err + "; " + find_create_ex_err;
            d3d3_prepared = false;
        }
        else
        {
            d3d3_prepared = re2dj::platform::windows::FindPe32ExportRva(
                                runtime_path,
                                "g_re2dj_graphics_trace_path",
                                &graphics_trace_path_rva,
                                &error) &&
                            re2dj::platform::windows::FindPe32ExportRva(
                                runtime_path,
                                "g_re2dj_fullscreen",
                                &fullscreen_rva,
                                &error) &&
                            WriteRemoteAnsi(child.hProcess,
                                            runtime_base + graphics_trace_path_rva,
                                            graphics_trace_path.string(),
                                            &error) &&
                            WriteRemoteBytes(
                                child.hProcess,
                                runtime_base + fullscreen_rva,
                                reinterpret_cast<const std::uint8_t*>(&fullscreen_value),
                                sizeof(fullscreen_value),
                                &error);

            if (d3d3_prepared && has_create)
            {
                d3d3_prepared = re2dj::platform::windows::FindPe32ExportRva(
                                    runtime_path,
                                    "_Re2djHleDirectDrawCreate@12",
                                    &d3d3_thunk_rva,
                                    &error) &&
                                WriteRemoteU32(child.hProcess,
                                               main_image_base + d3d3_slot_rva,
                                               runtime_base + d3d3_thunk_rva,
                                               &error);
            }
            if (d3d3_prepared && has_create_ex)
            {
                // On ez2dj4th, DirectDrawCreateEx in the PE import directory
                // belongs to the packer's import table (.protect). Patching it
                // overwrites packer data and breaks unpacking. For ez2dj4th,
                // do not overwrite the packer slot.
                d3d3_prepared = re2dj::platform::windows::FindPe32ExportRva(
                                    runtime_path,
                                    "_Re2djHleDirectDrawCreateEx@16",
                                    &d3d3_ex_thunk_rva,
                                    &error);
            }
        }
        if (d3d3_prepared)
        {
            RecordDiagnostic("{\"event\":\"graphics_trace\",\"path\":\"%s\",\"has_create\":%s,\"has_create_ex\":%s}",
                             graphics_trace_path.generic_string().c_str(),
                             has_create ? "true" : "false",
                             has_create_ex ? "true" : "false");
        }
    }
    bool directsound_prepared = !hle_directsound;
    if (hle_directsound && runtime_loaded)
    {
        std::uint32_t directsound_thunk_rva = 0;
        std::uint32_t directsound_slot_rva = 0;
        std::uint32_t audio_master_gain_rva = 0;
        const float audio_master_gain = std::pow(10.0f, audio_gain_db / 20.0f);
        const bool audio_gain_prepared =
            !audio_gain_set ||
            (re2dj::platform::windows::FindPe32ExportRva(
                 runtime_path,
                 "g_re2dj_audio_master_gain",
                 &audio_master_gain_rva,
                 &error) &&
             WriteRemoteBytes(
                 child.hProcess,
                 runtime_base + audio_master_gain_rva,
                 reinterpret_cast<const std::uint8_t*>(&audio_master_gain),
                 sizeof(audio_master_gain),
                 &error));
        if (target->id == "ez2dj4th")
        {
            directsound_prepared = re2dj::platform::windows::FindPe32ExportRva(
                                       runtime_path,
                                       "_Re2djHleDirectSoundCreate@12",
                                       &directsound_thunk_rva,
                                       &error) &&
                                   audio_gain_prepared;
        }
        else
        {
            directsound_prepared = re2dj::platform::windows::FindPe32ExportRva(
                                       runtime_path,
                                       "_Re2djHleDirectSoundCreate@12",
                                       &directsound_thunk_rva,
                                       &error) &&
                                   audio_gain_prepared &&
                                   re2dj::tools::windows_original_process_probe::FindIatSlotByOrdinal(
                                       info,
                                       file.data(),
                                       file.size(),
                                       "DSOUND.dll",
                                       1,
                                       &directsound_slot_rva,
                                       &error) &&
                                   WriteRemoteU32(child.hProcess,
                                                  main_image_base + directsound_slot_rva,
                                                  runtime_base + directsound_thunk_rva,
                                                  &error);
        }
        if (directsound_prepared && audio_gain_set)
        {
            RecordDiagnostic("{\"event\":\"audio_master_gain\",\"db\":%.3f,\"linear\":%.6f}",
                             static_cast<double>(audio_gain_db),
                             static_cast<double>(audio_master_gain));
        }
    }
    bool demo_volume_prepared = !demo_volume_set;
    if (demo_volume_set && runtime_loaded)
    {
        std::uint32_t demo_volume_rva = 0;
        std::uint32_t profile_thunk_rva = 0;
        std::uint32_t profile_slot_rva = 0;
        demo_volume_prepared =
            re2dj::platform::windows::FindPe32ExportRva(
                runtime_path, "g_re2dj_demo_volume", &demo_volume_rva, &error) &&
            re2dj::platform::windows::FindPe32ExportRva(
                runtime_path, "_Re2djHleGetPrivateProfileIntA@16", &profile_thunk_rva, &error) &&
            re2dj::tools::windows_original_process_probe::FindIatSlotByName(
                info, file.data(), file.size(), "KERNEL32.dll", "GetPrivateProfileIntA",
                &profile_slot_rva, &error) &&
            WriteRemoteU32(child.hProcess,
                           runtime_base + demo_volume_rva,
                           demo_volume,
                           &error) &&
            WriteRemoteU32(child.hProcess,
                           main_image_base + profile_slot_rva,
                           runtime_base + profile_thunk_rva,
                           &error);
        if (demo_volume_prepared)
        {
            RecordDiagnostic("{\"event\":\"demo_volume\",\"profile\":%u}",
                             static_cast<unsigned>(demo_volume));
        }
    }
    bool audio_trace_prepared = !audio_volume_trace;
    if (audio_volume_trace && runtime_loaded)
    {
        const char* const winmm_imports[] = {
            "mixerGetNumDevs", "mixerOpen", "mixerClose", "mixerGetLineInfoA",
            "mixerGetLineControlsA", "mixerGetControlDetailsA", "mixerSetControlDetails"};
        const char* const winmm_exports[] = {
            "_Re2djTraceMixerGetNumDevs@0", "_Re2djTraceMixerOpen@20",
            "_Re2djTraceMixerClose@4", "_Re2djTraceMixerGetLineInfoA@12",
            "_Re2djTraceMixerGetLineControlsA@12", "_Re2djTraceMixerGetControlDetailsA@12",
            "_Re2djTraceMixerSetControlDetails@12"};
        std::uint32_t audio_trace_path_rva = 0;
        std::uint32_t audio_image_base_rva = 0;
        std::filesystem::path audio_trace_path = diagnostic_log.path();
        audio_trace_path.replace_extension(".audio.log");
        audio_trace_prepared = re2dj::platform::windows::FindPe32ExportRva(
                                   runtime_path, "g_re2dj_audio_trace_path",
                                   &audio_trace_path_rva, &error) &&
                               re2dj::platform::windows::FindPe32ExportRva(
                                   runtime_path, "g_re2dj_audio_image_base",
                                   &audio_image_base_rva, &error) &&
                               WriteRemoteAnsi(child.hProcess,
                                               runtime_base + audio_trace_path_rva,
                                               audio_trace_path.string(), &error) &&
                               WriteRemoteU32(child.hProcess,
                                              runtime_base + audio_image_base_rva,
                                              static_cast<std::uint32_t>(main_image_base), &error);
        for (std::size_t index = 0;
             audio_trace_prepared && index < std::size(winmm_imports); ++index)
        {
            std::uint32_t thunk_rva = 0;
            std::uint32_t slot_rva = 0;
            bool import_present = false;
            audio_trace_prepared = re2dj::platform::windows::FindPe32ExportRva(
                                       runtime_path, winmm_exports[index], &thunk_rva, &error) &&
                                   FindOptionalIatSlotByName(
                                       info, file.data(), file.size(), "WINMM.dll",
                                       winmm_imports[index], &slot_rva, &import_present, &error) &&
                                   (!import_present ||
                                    WriteRemoteU32(child.hProcess,
                                                   main_image_base + slot_rva,
                                                   runtime_base + thunk_rva, &error));
        }
        if (audio_trace_prepared)
        {
            RecordDiagnostic("{\"event\":\"audio_volume_trace\",\"path\":\"%s\"}",
                             audio_trace_path.generic_string().c_str());
        }
    }
    const char* const vfs_exports[] = {"_Re2djVfsCreateFileA@28",
                                       "_Re2djVfsReadFile@20",
                                       "_Re2djVfsWriteFile@20",
                                       "_Re2djVfsSetFilePointer@16",
                                       "_Re2djVfsGetFileSize@8",
                                       "_Re2djVfsCloseHandle@4",
                                       "_Re2djVfsGetFileType@4",
                                       "_Re2djVfsSetCurrentDirectoryA@4",
                                       "_Re2djVfsGetCurrentDirectoryA@8"};
    const char* const vfs_imports[] = {"CreateFileA",
                                       "ReadFile",
                                       "WriteFile",
                                       "SetFilePointer",
                                       "GetFileSize",
                                       "CloseHandle",
                                       "GetFileType",
                                       "SetCurrentDirectoryA",
                                       "GetCurrentDirectoryA"};
    bool vfs_prepared = !hle_vfs;
    // Tracked apart from vfs_prepared so a failed image-loader patch reports
    // itself instead of silently skipping the device patches that follow it.
    bool image_loader_prepared = !hle_vfs;
    std::uint32_t device_ioctl_wrapper_address = 0;
    if (hle_vfs && runtime_loaded)
    {
        std::uint32_t hdd_root_rva = 0;
        std::uint32_t overlay_root_rva = 0;
        std::uint32_t chd_path_rva = 0;
        std::uint32_t vfs_trace_path_rva = 0;
        std::uint32_t device_mock_rva = 0;
        std::uint32_t device_mock_path_prefix_rva = 0;
        std::uint32_t dynamic_vfs_resolver_rva = 0;
        std::uint32_t get_proc_address_thunk_rva = 0;
        std::vector<std::uint32_t> get_proc_address_slot_rvas;
        bool get_proc_address_import_present = false;
        std::uint32_t device_ioctl_mode_rva = 0;
        std::uint32_t wts_console_session_mock_rva = 0;
        const std::filesystem::path overlay = std::filesystem::current_path() / "overlays" / target->id;
        std::filesystem::path vfs_trace_path = diagnostic_log.path();
        vfs_trace_path.replace_extension(".vfs.log");
        vfs_prepared = re2dj::platform::windows::FindPe32ExportRva(
                           runtime_path, "g_re2dj_vfs_hdd_root", &hdd_root_rva, &error) &&
                       re2dj::platform::windows::FindPe32ExportRva(
                           runtime_path, "g_re2dj_vfs_overlay_root", &overlay_root_rva, &error) &&
                       re2dj::platform::windows::FindPe32ExportRva(
                           runtime_path, "g_re2dj_vfs_chd_path", &chd_path_rva, &error) &&
                       re2dj::platform::windows::FindPe32ExportRva(
                           runtime_path, "g_re2dj_vfs_trace_path", &vfs_trace_path_rva, &error) &&
                       WriteRemoteAnsi(child.hProcess,
                                       runtime_base + hdd_root_rva,
                                       vfs_source_root.string(),
                                       &error) &&
                       WriteRemoteAnsi(child.hProcess,
                                       runtime_base + overlay_root_rva,
                                       overlay.string(),
                                       &error) &&
                       WriteRemoteAnsi(child.hProcess,
                                       runtime_base + chd_path_rva,
                                       chd_path.string(),
                                       &error) &&
                       WriteRemoteAnsi(child.hProcess,
                                       runtime_base + vfs_trace_path_rva,
                                       vfs_trace_path.string(),
                                       &error);
        if (vfs_prepared)
        {
            RecordDiagnostic("{\"event\":\"vfs_mount\",\"dump_root\":\"%s\",\"working_directory\":\"%s\",\"source_root\":\"%s\",\"overlay_root\":\"%s\",\"chd\":\"%s\"}",
                             root.root().generic_string().c_str(),
                             target->working_directory_relative_path.c_str(),
                             vfs_source_root.generic_string().c_str(),
                             overlay.generic_string().c_str(),
                             chd_path.generic_string().c_str());
            RecordDiagnostic("{\"event\":\"vfs_trace\",\"path\":\"%s\"}",
                             vfs_trace_path.generic_string().c_str());
        }
        const bool dynamic_vfs_resolver =
            target->run_defaults.hle_dynamic_vfs || force_dynamic_vfs_resolver;
        if (vfs_prepared && (device_mock_lptdi || dynamic_vfs_resolver))
        {
            vfs_prepared = re2dj::platform::windows::FindPe32ExportRva(
                               runtime_path,
                               "_Re2djHleGetProcAddress@8",
                               &get_proc_address_thunk_rva,
                               &error) &&
                           FindOptionalIatSlotsByName(
                               info,
                               file.data(),
                               file.size(),
                                "KERNEL32.dll",
                                "GetProcAddress",
                                &get_proc_address_slot_rvas,
                               &get_proc_address_import_present,
                               &error);
            if (vfs_prepared && dynamic_vfs_resolver && !get_proc_address_import_present)
            {
                vfs_prepared = false;
                error = "4th dynamic VFS resolver requires a GetProcAddress import";
            }
            for (const std::uint32_t slot_rva : get_proc_address_slot_rvas)
            {
                if (!vfs_prepared ||
                    !WriteRemoteU32(child.hProcess,
                                    main_image_base + slot_rva,
                                    runtime_base + get_proc_address_thunk_rva,
                                    &error))
                {
                    vfs_prepared = false;
                    break;
                }
            }
            if (vfs_prepared)
            {
                if (device_mock_lptdi)
                {
                    RecordDiagnostic("{\"event\":\"device_mock_dynamic_resolver_slots\",\"count\":%u}",
                                     static_cast<unsigned>(get_proc_address_slot_rvas.size()));
                }
                if (dynamic_vfs_resolver)
                {
                    vfs_prepared = re2dj::platform::windows::FindPe32ExportRva(
                                       runtime_path,
                                       "g_re2dj_vfs_dynamic_resolver",
                                       &dynamic_vfs_resolver_rva,
                                       &error) &&
                                   WriteRemoteU32(child.hProcess,
                                                  runtime_base + dynamic_vfs_resolver_rva,
                                                  1,
                                                  &error);
                    if (vfs_prepared)
                    {
                        RecordDiagnostic(
                            "{\"event\":\"vfs_dynamic_resolver\",\"enabled\":true,\"slots\":%u}",
                            static_cast<unsigned>(get_proc_address_slot_rvas.size()));
                    }
                }
            }
            if (!vfs_prepared)
            {
                error = (device_mock_lptdi ? "device mock dynamic resolver setup: "
                                            : "dynamic VFS resolver setup: ") +
                        error;
            }
        }
        if (vfs_prepared && device_mock_wts_console_session)
        {
            vfs_prepared = re2dj::platform::windows::FindPe32ExportRva(
                               runtime_path,
                               "g_re2dj_wts_console_session_mock",
                               &wts_console_session_mock_rva,
                               &error) &&
                           WriteRemoteU32(child.hProcess,
                                          runtime_base + wts_console_session_mock_rva,
                                          1,
                                          &error);
        }
        if (vfs_prepared && device_mock_lptdi)
        {
            vfs_prepared = re2dj::platform::windows::FindPe32ExportRva(
                               runtime_path,
                               "g_re2dj_device_mock",
                               &device_mock_rva,
                               &error) &&
                           WriteRemoteU32(child.hProcess,
                                          runtime_base + device_mock_rva,
                                          1,
                                          &error);
        }
        if (vfs_prepared && device_mock_lptdi)
        {
            vfs_prepared = re2dj::platform::windows::FindPe32ExportRva(
                               runtime_path,
                               "g_re2dj_device_mock_path_prefix",
                               &device_mock_path_prefix_rva,
                               &error) &&
                           WriteRemoteAnsi(child.hProcess,
                                           runtime_base + device_mock_path_prefix_rva,
                                           profile_device_mock_path_prefix,
                                           &error);
        }
        if (vfs_prepared &&
            device_ioctl_policy_count != 0)
        {
            const std::uint32_t ioctl_mode =
                !device_mock_lptdi_target_state_hex.empty()
                    ? 4
                    : (!device_mock_lptdi_response_profile_path.empty()
                           ? 3
                           : (device_mock_lptdi_ioctl_full_success ? 2 : 1));
            vfs_prepared = re2dj::platform::windows::FindPe32ExportRva(
                               runtime_path,
                               "g_re2dj_device_ioctl_mode",
                               &device_ioctl_mode_rva,
                               &error) &&
                           WriteRemoteU32(child.hProcess,
                                          runtime_base + device_ioctl_mode_rva,
                                          ioctl_mode,
                                          &error);
        }
        if (vfs_prepared && !device_mock_lptdi_target_state_hex.empty())
        {
            std::uint32_t target_state_rva = 0;
            vfs_prepared = re2dj::platform::windows::FindPe32ExportRva(
                               runtime_path,
                               "g_re2dj_device_target_state",
                               &target_state_rva,
                               &error) &&
                           WriteRemoteBytes(child.hProcess,
                                            runtime_base + target_state_rva,
                                            device_target_state.data(),
                                            device_target_state.size(),
                                            &error);
            if (vfs_prepared)
            {
                RecordDiagnostic("{\"event\":\"lptdi_target_state\",\"bytes\":\"%s\"}",
                                 device_mock_lptdi_target_state_hex.c_str());
            }
        }
        if (vfs_prepared && !device_mock_hardlock_450_response_hex.empty())
        {
            std::uint32_t response_rva = 0;
            std::uint32_t enabled_rva = 0;
            vfs_prepared = re2dj::platform::windows::FindPe32ExportRva(
                               runtime_path,
                               "g_re2dj_hardlock_response_450",
                               &response_rva,
                               &error) &&
                           re2dj::platform::windows::FindPe32ExportRva(
                               runtime_path,
                               "g_re2dj_hardlock_response_450_enabled",
                               &enabled_rva,
                               &error) &&
                           WriteRemoteBytes(child.hProcess,
                                            runtime_base + response_rva,
                                            hardlock_450_response.data(),
                                            hardlock_450_response.size(),
                                            &error) &&
                           WriteRemoteU32(child.hProcess,
                                          runtime_base + enabled_rva,
                                          1,
                                          &error);
            if (vfs_prepared)
            {
                RecordDiagnostic(
                    "{\"event\":\"hardlock_450_response_replay\",\"size\":6}");
            }
        }
        if (vfs_prepared && !device_mock_hardlock_44c_tail_hex.empty())
        {
            std::uint32_t tail_rva = 0;
            std::uint32_t enabled_rva = 0;
            vfs_prepared = re2dj::platform::windows::FindPe32ExportRva(
                               runtime_path,
                               "g_re2dj_hardlock_44c_tail_word",
                               &tail_rva,
                               &error) &&
                           re2dj::platform::windows::FindPe32ExportRva(
                               runtime_path,
                               "g_re2dj_hardlock_44c_tail_enabled",
                               &enabled_rva,
                               &error) &&
                           WriteRemoteU32(child.hProcess,
                                          runtime_base + tail_rva,
                                          hardlock_44c_tail_word,
                                          &error) &&
                           WriteRemoteU32(child.hProcess,
                                          runtime_base + enabled_rva,
                                          1,
                                          &error);
            if (vfs_prepared)
            {
                RecordDiagnostic(
                    "{\"event\":\"hardlock_44c_tail_patch\",\"value\":%u}",
                    static_cast<unsigned>(hardlock_44c_tail_word));
            }
        }
        if (vfs_prepared && hardlock_device)
        {
            std::uint32_t enabled_rva = 0;
            vfs_prepared = re2dj::platform::windows::FindPe32ExportRva(
                               runtime_path,
                               "g_re2dj_hardlock_device_enabled",
                               &enabled_rva,
                               &error) &&
                           WriteRemoteU32(child.hProcess,
                                          runtime_base + enabled_rva,
                                          1,
                                          &error);
            if (vfs_prepared)
            {
                RecordDiagnostic(
                    "{\"event\":\"hardlock_device\",\"enabled\":true}");
            }
        }
        if (vfs_prepared && !hardlock_transform_map.empty())
        {
            constexpr std::size_t kEntryStride =
                re2dj::hle::hardlock::kHardlockTransformBlockSize * 2;
            std::vector<unsigned char> packed;
            packed.reserve(hardlock_transform_map.size() * kEntryStride);
            for (const re2dj::hle::hardlock::HardlockTransformResponseEntry& map_entry :
                 hardlock_transform_map)
            {
                packed.insert(packed.end(), map_entry.input.begin(), map_entry.input.end());
                packed.insert(
                    packed.end(), map_entry.output.begin(), map_entry.output.end());
            }
            std::uint32_t map_rva = 0;
            std::uint32_t count_rva = 0;
            vfs_prepared = re2dj::platform::windows::FindPe32ExportRva(
                               runtime_path,
                               "g_re2dj_hardlock_transform_responses",
                               &map_rva,
                               &error) &&
                           re2dj::platform::windows::FindPe32ExportRva(
                               runtime_path,
                               "g_re2dj_hardlock_transform_response_count",
                               &count_rva,
                               &error) &&
                           WriteRemoteBytes(child.hProcess,
                                            runtime_base + map_rva,
                                            packed.data(),
                                            packed.size(),
                                            &error) &&
                           WriteRemoteU32(
                               child.hProcess,
                               runtime_base + count_rva,
                               static_cast<std::uint32_t>(hardlock_transform_map.size()),
                               &error);
            if (vfs_prepared)
            {
                // Only the entry count is recorded; the blocks themselves stay
                // out of the diagnostic log.
                RecordDiagnostic(
                    "{\"event\":\"hardlock_transform_map\",\"entries\":%u}",
                    static_cast<unsigned>(hardlock_transform_map.size()));
            }
        }
        for (const re2dj::device::LptdiResponseEntry& profile_entry :
             device_response_profile.entries)
        {
            if (!vfs_prepared)
            {
                break;
            }
            const bool first_code =
                profile_entry.control_code == re2dj::device::kLptdiIoctlCode410;
            const char* const bytes_export = first_code
                                                 ? "g_re2dj_device_response_410"
                                                 : "g_re2dj_device_response_414";
            const char* const size_export = first_code
                                                ? "g_re2dj_device_response_410_size"
                                                : "g_re2dj_device_response_414_size";
            std::uint32_t bytes_rva = 0;
            std::uint32_t size_rva = 0;
            vfs_prepared = re2dj::platform::windows::FindPe32ExportRva(
                               runtime_path, bytes_export, &bytes_rva, &error) &&
                           re2dj::platform::windows::FindPe32ExportRva(
                               runtime_path, size_export, &size_rva, &error) &&
                           WriteRemoteBytes(child.hProcess,
                                            runtime_base + bytes_rva,
                                            profile_entry.bytes.data(),
                                            profile_entry.bytes.size(),
                                            &error) &&
                           WriteRemoteU32(child.hProcess,
                                          runtime_base + size_rva,
                                          static_cast<std::uint32_t>(profile_entry.bytes.size()),
                                          &error);
            if (vfs_prepared)
            {
                RecordDiagnostic("{\"event\":\"lptdi_response_profile_entry\",\"code\":\"0x%08x\",\"size\":%u}",
                                 profile_entry.control_code,
                                 static_cast<unsigned>(profile_entry.bytes.size()));
            }
        }
        for (std::size_t index = 0; vfs_prepared && index < std::size(vfs_exports); ++index)
        {
            std::uint32_t export_rva = 0;
            std::uint32_t slot_rva = 0;
            bool import_present = false;
            vfs_prepared = re2dj::platform::windows::FindPe32ExportRva(
                               runtime_path, vfs_exports[index], &export_rva, &error) &&
                           FindOptionalIatSlotByName(
                               info,
                               file.data(),
                               file.size(),
                               "KERNEL32.dll",
                               vfs_imports[index],
                               &slot_rva,
                               &import_present,
                               &error) &&
                           (!import_present ||
                            WriteRemoteU32(child.hProcess,
                                           main_image_base + slot_rva,
                                           runtime_base + export_rva,
                                           &error));
        }
        if (!vfs_prepared)
        {
            error = "VFS wrapper setup: " + error;
        }
        if (vfs_prepared)
        {
            std::uint32_t export_rva = 0;
            std::uint32_t slot_rva = 0;
            bool import_present = false;
            image_loader_prepared = re2dj::platform::windows::FindPe32ExportRva(
                                        runtime_path,
                                        "_Re2djVfsLoadImageA@24",
                                        &export_rva,
                                        &error) &&
                                    FindOptionalIatSlotByName(
                                        info,
                                        file.data(),
                                        file.size(),
                                        "USER32.dll",
                                        "LoadImageA",
                                        &slot_rva,
                                        &import_present,
                                        &error) &&
                                    (!import_present ||
                                     WriteRemoteU32(child.hProcess,
                                                    main_image_base + slot_rva,
                                                    runtime_base + export_rva,
                                                    &error));
            RecordDiagnostic("{\"event\":\"vfs_image_loader\",\"import\":\"USER32.dll!LoadImageA\",\"prepared\":%s}",
                             image_loader_prepared ? "true" : "false");
        }
        if (vfs_prepared &&
            device_ioctl_policy_count != 0)
        {
            std::uint32_t export_rva = 0;
            std::uint32_t slot_rva = 0;
            bool import_present = false;
            vfs_prepared = re2dj::platform::windows::FindPe32ExportRva(
                               runtime_path,
                               "_Re2djDeviceIoControlMock@32",
                               &export_rva,
                               &error) &&
                           FindOptionalIatSlotByName(
                               info,
                               file.data(),
                               file.size(),
                               "KERNEL32.dll",
                               "DeviceIoControl",
                               &slot_rva,
                               &import_present,
                               &error) &&
                           (!import_present ||
                            WriteRemoteU32(child.hProcess,
                                           main_image_base + slot_rva,
                                           runtime_base + export_rva,
                                           &error));
            if (vfs_prepared)
            {
                device_ioctl_wrapper_address = runtime_base + export_rva;
            }
        }
    }
    bool io_runtime_prepared = !run_detached || !hle_io_ports;
    if (run_detached && hle_io_ports && runtime_loaded)
    {
        std::uint32_t enable_rva = 0;
        std::uint32_t image_base_rva = 0;
        std::uint32_t in_byte_rva = 0;
        std::uint32_t out_byte_rva = 0;
        std::uint32_t config_path_rva = 0;
        io_runtime_prepared = re2dj::platform::windows::FindPe32ExportRva(
                                  runtime_path,
                                  "g_re2dj_hle_io_ports",
                                  &enable_rva,
                                  &error) &&
                              re2dj::platform::windows::FindPe32ExportRva(
                                  runtime_path,
                                  "g_re2dj_io_image_base",
                                  &image_base_rva,
                                  &error) &&
                              re2dj::platform::windows::FindPe32ExportRva(
                                  runtime_path,
                                  "g_re2dj_io_in_byte_rva",
                                  &in_byte_rva,
                                  &error) &&
                              re2dj::platform::windows::FindPe32ExportRva(
                                  runtime_path,
                                  "g_re2dj_io_out_byte_rva",
                                  &out_byte_rva,
                                  &error) &&
                              (io_config_path.empty() ||
                               (re2dj::platform::windows::FindPe32ExportRva(
                                    runtime_path,
                                    "g_re2dj_io_config_path",
                                    &config_path_rva,
                                    &error) &&
                                WriteRemoteAnsi(child.hProcess,
                                                runtime_base + config_path_rva,
                                                io_config_path.string(),
                                                &error))) &&
                              WriteRemoteU32(child.hProcess,
                                             runtime_base + image_base_rva,
                                             static_cast<std::uint32_t>(main_image_base),
                                             &error) &&
                              WriteRemoteU32(child.hProcess,
                                             runtime_base + in_byte_rva,
                                             static_cast<std::uint32_t>(io_policy.in_byte_rva),
                                             &error) &&
                              WriteRemoteU32(child.hProcess,
                                             runtime_base + out_byte_rva,
                                             static_cast<std::uint32_t>(io_policy.out_byte_rva),
                                             &error) &&
                              WriteRemoteU32(child.hProcess,
                                             runtime_base + enable_rva,
                                             1,
                                             &error);
        if (io_runtime_prepared)
        {
            RecordDiagnostic("{\"event\":\"io_port_runtime\",\"image_base\":\"0x%08x\",\"in_rva\":\"0x%08x\",\"out_rva\":\"0x%08x\",\"status\":\"prepared\"}",
                             static_cast<unsigned>(main_image_base),
                             static_cast<unsigned>(io_policy.in_byte_rva),
                             static_cast<unsigned>(io_policy.out_byte_rva));
        }
    }
    bool message_box_prepared = !hle_message_box;
    if (hle_message_box && runtime_loaded)
    {
        std::uint32_t message_box_enable_rva = 0;
        std::uint32_t message_box_result_rva = 0;
        message_box_prepared = re2dj::platform::windows::FindPe32ExportRva(
                                   runtime_path,
                                   "g_re2dj_message_box_result",
                                   &message_box_result_rva,
                                   &error) &&
                               re2dj::platform::windows::FindPe32ExportRva(
                                   runtime_path,
                                   "g_re2dj_hle_message_box",
                                   &message_box_enable_rva,
                                   &error) &&
                               WriteRemoteU32(child.hProcess,
                                              runtime_base + message_box_result_rva,
                                              1,
                                              &error) &&
                               WriteRemoteU32(child.hProcess,
                                              runtime_base + message_box_enable_rva,
                                              1,
                                              &error);
        RecordDiagnostic("{\"event\":\"message_box_boundary\",\"status\":\"%s\",\"result\":1}",
                         message_box_prepared ? "prepared" : "unavailable");
    }
    std::uint32_t exit_thunk_rva = 0;
    std::uint32_t exit_slot_rva = 0;
    bool exit_probe_prepared = !probe_exit_process;
    if (probe_exit_process && runtime_loaded)
    {
        exit_probe_prepared = re2dj::platform::windows::FindPe32ExportRva(
                                  runtime_path, "_Re2djProbeExitProcess@4", &exit_thunk_rva, &error) &&
                              re2dj::tools::windows_original_process_probe::FindIatSlotByName(
                                  info,
                                  file.data(),
                                  file.size(),
                                  "KERNEL32.dll",
                                  "ExitProcess",
                                  &exit_slot_rva,
                                  &error) &&
                              WriteRemoteU32(child.hProcess,
                                             main_image_base + exit_slot_rva,
                                             runtime_base + exit_thunk_rva,
                                             &error);
    }
    std::uint32_t exit_break_target = 0;
    std::uint8_t original_exit_byte = 0;
    bool exit_break_prepared = !break_exit_process;
    if (break_exit_process)
    {
        std::uint32_t exit_break_slot_rva = 0;
        SIZE_T copied = 0;
        const bool exit_import_present =
            re2dj::tools::windows_original_process_probe::FindIatSlotByName(
                info,
                file.data(),
                file.size(),
                "KERNEL32.dll",
                "ExitProcess",
                &exit_break_slot_rva,
                &error);
        if (!exit_import_present &&
            (target->id == "ez2dj4th" ||
             lptdi_post_ioctl_trace_steps != 0 || api_trace || slot_writer_trace ||
             null_context_object_source_trace ||
             null_context_field_writer_trace || null_context_field_access_trace ||
             null_context_field_reference_execution_trace ||
             null_context_object_state_trace ||
             null_context_object_reference_scan ||
             null_context_entry_trace ||
             // These two are bounded traces as well: they observe and never
             // need the static exit slot the packed 4th does not carry.
             !field_reference_scan_constants.empty() ||
             field_write_watch_address != 0 || !code_windows.empty()) &&
            error == "requested import is not present")
        {
            error.clear();
            exit_break_prepared = true;
            RecordDiagnostic("{\"event\":\"exit_breakpoint\",\"status\":\"bounded_trace_without_static_import\",\"api_trace\":%s,\"slot_writer_trace\":%s,\"null_context_object_source_trace\":%s,\"null_context_field_writer_trace\":%s,\"null_context_field_access_trace\":%s,\"null_context_field_reference_execution_trace\":%s,\"null_context_object_state_trace\":%s}",
                             api_trace ? "true" : "false",
                             slot_writer_trace ? "true" : "false",
                             null_context_object_source_trace ? "true" : "false",
                             null_context_field_writer_trace ? "true" : "false",
                             null_context_field_access_trace ? "true" : "false",
                             null_context_field_reference_execution_trace ? "true" : "false",
                             null_context_object_state_trace ? "true" : "false");
        }
        else
        {
            exit_break_prepared =
                exit_import_present &&
                ReadProcessMemory(
                    child.hProcess,
                    reinterpret_cast<const void*>(main_image_base + exit_break_slot_rva),
                    &exit_break_target,
                    sizeof(exit_break_target),
                    &copied) != FALSE &&
                copied == sizeof(exit_break_target) &&
                SetSoftwareEntryBreakpoint(child.hProcess,
                                           exit_break_target,
                                           &original_exit_byte,
                                           &error);
        }
        if (!exit_break_prepared && error.empty())
        {
            error = "cannot set ExitProcess software breakpoint";
        }
    }
    GuestReturnWatchMap guest_return_watches;
    bool d3d_init_trace_prepared = !d3d_init_trace;
    if (d3d_init_trace)
    {
        d3d_init_trace_prepared = reached && entry_restored && exit_break_prepared &&
                                  InstallD3dInitReturnBreakpoints(child.hProcess,
                                                                  main_image_base,
                                                                  &guest_return_watches,
                                                                  &error);
        if (!d3d_init_trace_prepared && error.empty())
        {
            error = "cannot install Direct3D initialization return breakpoints";
        }
    }
    bool ksnd_load_trace_prepared = !ksnd_load_trace;
    if (ksnd_load_trace)
    {
        ksnd_load_trace_prepared = reached && entry_restored && exit_break_prepared &&
                                   InstallKsndLoadReturnBreakpoints(child.hProcess,
                                                                    main_image_base,
                                                                    &guest_return_watches,
                                                                    &error);
        if (!ksnd_load_trace_prepared && error.empty())
        {
            error = "cannot install KSND load return breakpoints";
        }
    }
    ApiWatchMap api_watches;
    NullContextFieldWriterTraceState null_context_field_writer_state;
    NullContextFieldWriterTraceState null_context_field_access_state;
    re2dj::input::LegacyIoPortBus io_port_bus;
    bool api_trace_prepared = !api_trace;
    if (api_trace)
    {
        const bool system_watches_installed =
            !system_api_trace ||
             (reached && entry_restored && exit_break_prepared &&
             InstallApiTraceBreakpoints(child.hProcess,
                                        kernel32_base,
                                        kernelbase_base,
                                        user32_base,
                                        &api_watches,
                                        null_context_allocation_trace,
                                        &error));
        const bool runtime_watch_installed =
            lptdi_post_ioctl_trace_steps == 0 ||
            (system_watches_installed &&
             InstallRuntimeApiTraceBreakpoint(child.hProcess,
                                              device_ioctl_wrapper_address,
                                              "DeviceIoControl",
                                              8,
                                              &api_watches,
                                              &error));
        if (system_watches_installed && runtime_watch_installed)
        {
            api_trace_prepared = true;
        }
        else if (error.empty())
        {
            error = "cannot install API trace breakpoints";
        }
        RecordDiagnostic("{\"event\":\"api_trace_ready\",\"watches\":%u,\"prepared\":%s,\"kernel32\":\"0x%08x\",\"kernelbase\":\"0x%08x\",\"user32\":\"0x%08x\"}",
                         static_cast<unsigned>(api_watches.size()),
                         api_trace_prepared ? "true" : "false",
                         static_cast<unsigned>(kernel32_base),
                         static_cast<unsigned>(kernelbase_base),
                         static_cast<unsigned>(user32_base));
    }
    bool slot_writer_trace_prepared = !slot_writer_trace;
    if (slot_writer_trace)
    {
        slot_writer_trace_prepared = reached && entry_restored && exit_break_prepared &&
                                     SetSlotWriterBreakpoints(child.hThread,
                                                              main_image_base,
                                                              &error);
        RecordDiagnostic("{\"event\":\"slot_writer_trace_ready\",\"prepared\":%s,\"slot\":\"0x%08x\",\"writer0\":\"0x%08x\",\"writer1\":\"0x%08x\",\"writer2\":\"0x%08x\"}",
                         slot_writer_trace_prepared ? "true" : "false",
                         static_cast<unsigned>(main_image_base +
                                               kEz2dj4thPointerSlotRva),
                         static_cast<unsigned>(main_image_base +
                                               kEz2dj4thSlotWriterRvas[0]),
                         static_cast<unsigned>(main_image_base +
                                               kEz2dj4thSlotWriterRvas[1]),
                         static_cast<unsigned>(main_image_base +
                                               kEz2dj4thSlotWriterRvas[2]));
        if (!slot_writer_trace_prepared && error.empty())
        {
            error = "cannot install slot-writer execution breakpoints";
        }
    }
    std::uintptr_t null_context_object_source_prologue = 0;
    std::uintptr_t null_context_object_source_boundary = 0;
    bool null_context_object_source_trace_prepared =
        !null_context_object_source_trace;
    if (null_context_object_source_trace)
    {
        null_context_object_source_prologue =
            main_image_base + kEz2dj4thNullContextObjectSourcePrologueRva;
        null_context_object_source_boundary =
            main_image_base + kEz2dj4thNullContextObjectSourceBoundaryRva;
        null_context_object_source_trace_prepared =
            reached && entry_restored && exit_break_prepared &&
            SetNullContextObjectSourceBoundaryBreakpoint(
                child.hThread,
                null_context_object_source_boundary,
                &error);
        RecordDiagnostic(
            "{\"event\":\"null_context_object_source_trace_ready\",\"prepared\":%s,\"field_access_anchor\":\"0x%08x\",\"prologue\":\"0x%08x\",\"boundary\":\"0x%08x\",\"boundary_source\":\"runtime_scan_confirmed\",\"target_object\":\"0x%08x\",\"target_object_rva\":\"0x%08x\"}",
            null_context_object_source_trace_prepared ? "true" : "false",
            static_cast<unsigned>(main_image_base + kEz2dj4thNullContextFieldAccessRva),
            static_cast<unsigned>(null_context_object_source_prologue),
            static_cast<unsigned>(null_context_object_source_boundary),
            static_cast<unsigned>(main_image_base + kEz2dj4thNullContextObjectRva),
            kEz2dj4thNullContextObjectRva);
        if (!null_context_object_source_trace_prepared && error.empty())
        {
            error = "cannot install null-context object source boundary breakpoint";
        }
    }
    bool null_context_field_writer_trace_prepared =
        !null_context_field_writer_trace;
    if (null_context_field_writer_trace)
    {
        null_context_field_writer_trace_prepared =
            reached && entry_restored && exit_break_prepared &&
            SetNullContextFieldAccessBreakpoint(
                child.hThread,
                main_image_base,
                kNullContextFieldWriteBreakpointControl,
                &error);
        const std::uintptr_t field_address =
            main_image_base + kEz2dj4thNullContextFieldRva;
        std::uint32_t field_value = 0;
        SIZE_T field_copied = 0;
        const bool field_readable =
            null_context_field_writer_trace_prepared &&
            ReadProcessMemory(child.hProcess,
                              reinterpret_cast<const void*>(field_address),
                              &field_value,
                              sizeof(field_value),
                              &field_copied) != FALSE &&
            field_copied == sizeof(field_value);
        null_context_field_writer_state.last_field_value = field_value;
        null_context_field_writer_state.last_field_readable = field_readable;
        RecordDiagnostic("{\"event\":\"null_context_field_writer_trace_ready\",\"prepared\":%s,\"field\":\"0x%08x\",\"rva\":\"0x%08x\",\"field_readable\":%s,\"field_current\":\"0x%08x\"}",
                         null_context_field_writer_trace_prepared ? "true" : "false",
                         static_cast<unsigned>(field_address),
                         kEz2dj4thNullContextFieldRva,
                         field_readable ? "true" : "false",
                         static_cast<unsigned>(field_value));
        if (!null_context_field_writer_trace_prepared && error.empty())
        {
            error = "cannot install null-context field data breakpoint";
        }
        if (null_context_field_writer_trace_prepared)
        {
            ScanAccessViolationSlotReferences(
                child.hProcess,
                static_cast<std::uint32_t>(field_address),
                main_image_base,
                info);
        }
    }
    bool null_context_field_access_trace_prepared =
        !null_context_field_access_trace;
    if (null_context_field_access_trace)
    {
        null_context_field_access_trace_prepared =
            reached && entry_restored && exit_break_prepared &&
            SetNullContextFieldAccessBreakpoint(
                child.hThread,
                main_image_base,
                kNullContextFieldAccessBreakpointControl,
                &error);
        const std::uintptr_t field_address =
            main_image_base + kEz2dj4thNullContextFieldRva;
        std::uint32_t field_value = 0;
        SIZE_T field_copied = 0;
        const bool field_readable =
            null_context_field_access_trace_prepared &&
            ReadProcessMemory(child.hProcess,
                              reinterpret_cast<const void*>(field_address),
                              &field_value,
                              sizeof(field_value),
                              &field_copied) != FALSE &&
            field_copied == sizeof(field_value);
        null_context_field_access_state.last_field_value = field_value;
        null_context_field_access_state.last_field_readable = field_readable;
        RecordDiagnostic("{\"event\":\"null_context_field_access_trace_ready\",\"prepared\":%s,\"field\":\"0x%08x\",\"rva\":\"0x%08x\",\"field_readable\":%s,\"field_current\":\"0x%08x\"}",
                         null_context_field_access_trace_prepared ? "true" : "false",
                         static_cast<unsigned>(field_address),
                         kEz2dj4thNullContextFieldRva,
                         field_readable ? "true" : "false",
                         static_cast<unsigned>(field_value));
        if (!null_context_field_access_trace_prepared && error.empty())
        {
            error = "cannot install null-context field access breakpoint";
        }
    }
    bool null_context_field_reference_execution_trace_prepared =
        !null_context_field_reference_execution_trace;
    if (null_context_field_reference_execution_trace)
    {
        null_context_field_reference_execution_trace_prepared =
            reached && entry_restored && exit_break_prepared &&
            SetNullContextFieldReferenceExecutionBreakpoints(
                child.hThread,
                main_image_base,
                &error);
        RecordDiagnostic(
            "{\"event\":\"null_context_field_reference_execution_trace_ready\",\"prepared\":%s,\"target_object\":\"0x%08x\",\"target_field\":\"0x%08x\",\"candidate_count\":%u}",
            null_context_field_reference_execution_trace_prepared ? "true" : "false",
            static_cast<unsigned>(main_image_base + kEz2dj4thNullContextObjectRva),
            static_cast<unsigned>(main_image_base + kEz2dj4thNullContextFieldRva),
            static_cast<unsigned>(kNullContextFieldReferenceExecutionCandidates.size()));
        if (null_context_field_reference_execution_trace_prepared)
        {
            for (std::size_t index = 0;
                 index < kNullContextFieldReferenceExecutionCandidates.size();
                 ++index)
            {
                const auto& candidate =
                    kNullContextFieldReferenceExecutionCandidates[index];
                RecordDiagnostic(
                    "{\"event\":\"null_context_field_reference_execution_candidate\",\"candidate_index\":%u,\"rva\":\"0x%08x\",\"address\":\"0x%08x\",\"receiver_register\":\"%s\",\"write_source\":\"%s\",\"instruction_size\":%u}",
                    static_cast<unsigned>(index),
                    candidate.rva,
                    static_cast<unsigned>(main_image_base + candidate.rva),
                    candidate.receiver_name,
                    candidate.write_source,
                    candidate.instruction_size);
            }
        }
        if (!null_context_field_reference_execution_trace_prepared && error.empty())
        {
            error =
                "cannot install null-context field reference execution breakpoints";
        }
    }
    bool null_context_object_state_trace_prepared = !null_context_object_state_trace;
    if (null_context_object_state_trace)
    {
        // The protection stub has not decrypted runtime code yet, so the
        // boundary comes from the RVA confirmed by Task 150 rather than a scan.
        const std::uintptr_t state_boundary =
            main_image_base + kEz2dj4thNullContextObjectSourceBoundaryRva;
        null_context_object_state_trace_prepared =
            reached && entry_restored && exit_break_prepared &&
            SetNullContextObjectSourceBoundaryBreakpoint(child.hThread,
                                                         state_boundary,
                                                         &error);
        RecordDiagnostic(
            "{\"event\":\"null_context_object_state_trace_ready\",\"prepared\":%s,\"boundary\":\"0x%08x\",\"boundary_rva\":\"0x%08x\",\"field_access_anchor\":\"0x%08x\",\"target_object\":\"0x%08x\",\"target_field\":\"0x%08x\",\"window_bytes\":%u,\"frame_limit\":%u,\"hit_limit\":%u}",
            null_context_object_state_trace_prepared ? "true" : "false",
            static_cast<unsigned>(state_boundary),
            kEz2dj4thNullContextObjectSourceBoundaryRva,
            static_cast<unsigned>(main_image_base +
                                  kEz2dj4thNullContextFieldAccessRva),
            static_cast<unsigned>(main_image_base + kEz2dj4thNullContextObjectRva),
            static_cast<unsigned>(main_image_base + kEz2dj4thNullContextFieldRva),
            kNullContextObjectStateWindowBytes,
            static_cast<unsigned>(kNullContextObjectStateFrameLimit),
            kNullContextObjectStateHitLimit);
        if (!null_context_object_state_trace_prepared && error.empty())
        {
            error = "cannot install null-context object state breakpoint";
        }
    }
    bool null_context_object_reference_scan_prepared = !null_context_object_reference_scan;
    if (null_context_object_reference_scan)
    {
        const std::uintptr_t scan_boundary =
            main_image_base + kEz2dj4thNullContextObjectSourceBoundaryRva;
        null_context_object_reference_scan_prepared =
            reached && entry_restored && exit_break_prepared &&
            SetNullContextObjectSourceBoundaryBreakpoint(child.hThread,
                                                         scan_boundary,
                                                         &error);
        RecordDiagnostic(
            "{\"event\":\"null_context_object_reference_scan_ready\",\"prepared\":%s,\"boundary\":\"0x%08x\",\"target_object\":\"0x%08x\",\"text_rva\":\"0x%08x\",\"text_size\":\"0x%08x\",\"vtable_entries\":%u,\"match_limit\":%u}",
            null_context_object_reference_scan_prepared ? "true" : "false",
            static_cast<unsigned>(scan_boundary),
            static_cast<unsigned>(main_image_base + kEz2dj4thNullContextObjectRva),
            kEz2dj4thTextRva,
            kEz2dj4thTextVirtualSize,
            kNullContextObjectVtableEntries,
            static_cast<unsigned>(kNullContextObjectReferenceLimit));
        if (!null_context_object_reference_scan_prepared && error.empty())
        {
            error = "cannot install null-context object reference scan breakpoint";
        }
    }
    bool null_context_entry_trace_prepared = !null_context_entry_trace;
    if (null_context_entry_trace)
    {
        null_context_entry_trace_prepared =
            reached && entry_restored && exit_break_prepared &&
            SetNullContextEntryBreakpoints(child.hThread, main_image_base, &error);
        RecordDiagnostic(
            "{\"event\":\"null_context_entry_trace_ready\",\"prepared\":%s,\"singleton\":\"0x%08x\",\"entries\":%u}",
            null_context_entry_trace_prepared ? "true" : "false",
            static_cast<unsigned>(main_image_base + kEz2dj4thNullContextObjectRva),
            static_cast<unsigned>(kNullContextEntryPoints.size()));
        for (std::size_t index = 0; index < kNullContextEntryPoints.size(); ++index)
        {
            RecordDiagnostic(
                "{\"event\":\"null_context_entry_point\",\"index\":%u,\"name\":\"%s\",\"rva\":\"0x%08x\",\"address\":\"0x%08x\"}",
                static_cast<unsigned>(index),
                kNullContextEntryPoints[index].name,
                kNullContextEntryPoints[index].rva,
                static_cast<unsigned>(main_image_base + kNullContextEntryPoints[index].rva));
        }
        if (!null_context_entry_trace_prepared && error.empty())
        {
            error = "cannot install null-context entry breakpoints";
        }
    }
    bool field_write_watch_prepared = field_write_watch_address == 0;
    if (field_write_watch_address != 0)
    {
        // The primary thread exists before the loop starts, so it is armed here
        // rather than through the thread-creation path the loop uses.
        field_write_watch_prepared =
            reached && entry_restored && exit_break_prepared &&
            SetFieldWriteWatch(child.hThread, field_write_watch_address, &error);
        RecordDiagnostic(
            "{\"event\":\"field_write_watch_ready\",\"prepared\":%s,\"address\":\"0x%08x\",\"rva\":\"0x%08x\",\"limit\":%u}",
            field_write_watch_prepared ? "true" : "false",
            static_cast<unsigned>(field_write_watch_address),
            static_cast<unsigned>(field_write_watch_address >= main_image_base
                                      ? field_write_watch_address - main_image_base
                                      : 0),
            static_cast<unsigned>(kFieldWriteWatchHitLimit));
        if (!field_write_watch_prepared && error.empty())
        {
            error = "cannot install the field write watch";
        }
    }
    bool directinput_prepared = true;
    if (reached && entry_restored && runtime_loaded)
    {
        std::uint32_t directinput_thunk_rva = 0;
        std::string find_dinput_err;
        if (re2dj::platform::windows::FindPe32ExportRva(
                runtime_path,
                "_Re2djHleDirectInputCreateA@16",
                &directinput_thunk_rva,
                &find_dinput_err))
        {
            if (target->id == "ez2dj4th")
            {
                // On ez2dj4th, 0x00ad1634 (RVA 0x006d1634) is the DirectInputCreateA
                // import slot unpacked by the protector. Overwrite it so any call
                // via `jmp dword ptr [0x00ad1634]` lands on the HLE facade.
                constexpr std::uint32_t kEz2dj4thDirectInputCreateASlotRva = 0x006d1634u;
                directinput_prepared = WriteRemoteU32(
                    child.hProcess,
                    main_image_base + kEz2dj4thDirectInputCreateASlotRva,
                    runtime_base + directinput_thunk_rva,
                    &error);
                if (!directinput_prepared && error.empty())
                {
                    error = "cannot patch ez2dj4th DirectInputCreateA IAT slot";
                }
            }
            else
            {
                std::uint32_t dinput_slot_rva = 0;
                if (re2dj::tools::windows_original_process_probe::FindIatSlotByName(
                        info,
                        file.data(),
                        file.size(),
                        "DINPUT.dll",
                        "DirectInputCreateA",
                        &dinput_slot_rva,
                        &find_dinput_err))
                {
                    directinput_prepared = WriteRemoteU32(
                        child.hProcess,
                        main_image_base + dinput_slot_rva,
                        runtime_base + directinput_thunk_rva,
                        &error);
                }
            }
        }
        else
        {
            directinput_prepared = false;
            error = "cannot find _Re2djHleDirectInputCreateA@16 in runtime: " + find_dinput_err;
        }
    }
    if (reached && entry_restored && hle_directsound && target->id == "ez2dj4th")
    {
        std::uint32_t directsound_thunk_rva = 0;
        std::string find_dsound_err;
        if (re2dj::platform::windows::FindPe32ExportRva(
                runtime_path,
                "_Re2djHleDirectSoundCreate@12",
                &directsound_thunk_rva,
                &find_dsound_err))
        {
            constexpr std::uint32_t kEz2dj4thDirectSoundCreateSlotRva = 0x006d1664u;
            const bool dsound_patched = WriteRemoteU32(
                child.hProcess,
                main_image_base + kEz2dj4thDirectSoundCreateSlotRva,
                runtime_base + directsound_thunk_rva,
                &error);
            if (!dsound_patched && error.empty())
            {
                error = "cannot patch ez2dj4th DirectSoundCreate IAT slot";
            }
        }
        else
        {
            error = "cannot find _Re2djHleDirectSoundCreate@12 in runtime: " + find_dsound_err;
        }
    }
    if (reached && entry_restored && !code_windows.empty())
    {
        for (const GuestCodeWindowRequest& request : code_windows)
        {
            CaptureGuestCodeWindow(child.hProcess, main_image_base, &info, request);
        }
    }
    re2dj::tools::windows_original_process_probe::IatVerificationResult iat;
    const bool iat_verified = reached && entry_restored && runtime_loaded && handoff_prepared &&
                              display_prepared && d3d3_prepared && directsound_prepared &&
                              directinput_prepared &&
                              demo_volume_prepared && audio_trace_prepared && vfs_prepared &&
                              image_loader_prepared &&
                              io_runtime_prepared && message_box_prepared &&
                              exit_probe_prepared &&
                              exit_break_prepared && d3d_init_trace_prepared &&
                              ksnd_load_trace_prepared &&
                              api_trace_prepared &&
                              slot_writer_trace_prepared &&
                              null_context_object_source_trace_prepared &&
                              null_context_field_access_trace_prepared &&
                              null_context_field_writer_trace_prepared &&
                              null_context_field_reference_execution_trace_prepared &&
                              null_context_object_state_trace_prepared &&
                              null_context_object_reference_scan_prepared &&
                              null_context_entry_trace_prepared &&
                              re2dj::tools::windows_original_process_probe::VerifySuspendedIat(
                                  child.hProcess,
                                  main_image_base,
                                  info,
                                  file.data(),
                                  file.size(),
                                  &iat,
                                  &error);
    if (iat_verified && null_context_entry_trace)
    {
        for (std::uint32_t target_slot : {0x006d1908u, 0x006d1724u})
        {
            re2dj::tools::windows_original_process_probe::IatSlotResolution resolution;
            std::string resolve_error;
            const bool resolved = re2dj::tools::windows_original_process_probe::ResolveIatSlot(
                info, file.data(), file.size(), target_slot, &resolution, &resolve_error);
            RecordDiagnostic(
                "{\"event\":\"null_context_iat_slot_resolved\",\"slot_rva\":\"0x%08x\",\"resolved\":%s,\"module\":\"%s\",\"function\":\"%s\",\"ordinal\":%u,\"is_ordinal\":%s,\"error\":\"%s\"}",
                target_slot,
                resolved ? "true" : "false",
                resolution.module.c_str(),
                resolution.function.c_str(),
                static_cast<unsigned>(resolution.ordinal),
                resolution.is_ordinal ? "true" : "false",
                resolve_error.c_str());
        }
    }
    const bool resume_for_handoff = handoff_requested || hle_vfs || hle_display_mode || hle_d3d3 ||
                                    probe_exit_process || break_exit_process;
    const char* const expected_message = probe_exit_process ? "re2dj:probe:ExitProcess"
                                                             : (hle_vfs ? "re2dj:vfs:CreateFileA"
                                                                : hle_display_mode
                                                                    ? "re2dj:hle:display-mode:absorbed"
                                                                    : handoff_message);
    const auto resume_debuggee = [&]() {
        if (inject_runtime)
        {
            if (ResumeThread(child.hThread) != (std::numeric_limits<DWORD>::max)())
            {
                return true;
            }
            error = "cannot resume primary thread after runtime injection";
            return false;
        }
        if (ContinueDebugEvent(breakpoint_process_id, breakpoint_thread_id, DBG_CONTINUE) != FALSE)
        {
            return true;
        }
        error = "cannot continue entry breakpoint debug event";
        return false;
    };
    const auto run_detached_runtime = [&]() {
        if (DebugSetProcessKillOnExit(FALSE) == FALSE ||
            DebugActiveProcessStop(child.dwProcessId) == FALSE)
        {
            error = "cannot detach debugger from prepared original process";
            return false;
        }
        if (ResumeThread(child.hThread) == (std::numeric_limits<DWORD>::max)())
        {
            error = "cannot resume detached original process";
            return false;
        }
        RecordDiagnostic("{\"event\":\"runtime_detached\",\"process_id\":%u}",
                         static_cast<unsigned>(child.dwProcessId));
        if (WaitForSingleObject(child.hProcess, INFINITE) != WAIT_OBJECT_0)
        {
            error = "cannot wait for detached original process";
            return false;
        }
        DWORD exit_code = 0;
        if (GetExitCodeProcess(child.hProcess, &exit_code) == FALSE)
        {
            error = "cannot read detached original process exit code";
            return false;
        }
        RecordDiagnostic("{\"event\":\"runtime_detached_exit\",\"code\":\"0x%08x\"}",
                         static_cast<unsigned>(exit_code));
        return true;
    };
    const bool handoff_observed = run_detached
                                      ? (handoff_prepared && display_prepared && d3d3_prepared &&
                                         directsound_prepared && demo_volume_prepared &&
                                         audio_trace_prepared && vfs_prepared &&
                                         image_loader_prepared &&
                                         io_runtime_prepared && message_box_prepared &&
                                         exit_probe_prepared &&
                                         exit_break_prepared && d3d_init_trace_prepared &&
                                         ksnd_load_trace_prepared && api_trace_prepared &&
                                         slot_writer_trace_prepared &&
                                         null_context_object_source_trace_prepared &&
                                         null_context_field_access_trace_prepared &&
                                         null_context_field_writer_trace_prepared &&
                                         null_context_field_reference_execution_trace_prepared &&
                                         null_context_object_state_trace_prepared &&
                                         null_context_object_reference_scan_prepared &&
                                         null_context_entry_trace_prepared &&
                                         run_detached_runtime())
                                      : instruction_trace
                                      ? (entry_restored &&
                                         EnableSingleStep(child.hThread, entry, &error) &&
                                         resume_debuggee() &&
                                         WaitForInstructionTrace(child.hProcess,
                                                                 breakpoint_thread_id,
                                                                 instruction_trace_max_steps,
                                                                 &error))
                                      : (!resume_for_handoff ||
                                         (handoff_prepared && display_prepared && d3d3_prepared &&
                                          directsound_prepared && demo_volume_prepared &&
                                          audio_trace_prepared && vfs_prepared &&
                                          image_loader_prepared &&
                                          io_runtime_prepared &&
                                          message_box_prepared &&
                                          exit_probe_prepared &&
                                          exit_break_prepared && d3d_init_trace_prepared &&
                                          ksnd_load_trace_prepared &&
                                          api_trace_prepared &&
                                          slot_writer_trace_prepared &&
                                          null_context_object_source_trace_prepared &&
                                          null_context_field_access_trace_prepared &&
                                          null_context_field_writer_trace_prepared &&
                                          null_context_field_reference_execution_trace_prepared &&
                                          null_context_object_state_trace_prepared &&
                                          null_context_object_reference_scan_prepared &&
                                          null_context_entry_trace_prepared &&
                                          (break_exit_process
                                               ? (resume_debuggee() &&
                                                  WaitForExitProcessBreakpoint(child.hProcess,
                                                                               exit_break_target,
                                                                               api_trace && exit_break_target == 0,
                                                                               null_context_allocation_trace,
                                                                               slot_writer_trace,
                                                                               null_context_object_source_trace,
                                                                               null_context_field_access_trace,
                                                                               null_context_field_writer_trace,
                                                                               null_context_field_reference_execution_trace,
                                                                               null_context_object_state_trace,
                                                                               null_context_object_reference_scan,
                                                                               null_context_entry_trace,
                                                                               scan_fault_references,
                                                                               field_reference_scan_constants,
                                                                               field_write_watch_address,
                                                                               code_windows,
                                                                               trace,
                                                                               lptdi_post_ioctl_trace_steps,
                                                                               lptdi_post_ioctl_trace_code,
                                                                               diagnostic_idle_timeout_ms,
                                                                               main_image_base,
                                                                               null_context_object_source_boundary,
                                                                               io_policy,
                                                                               &info,
                                                                               &guest_return_watches,
                                                                               &api_watches,
                                                                               hle_io_ports ? &io_port_bus : nullptr,
                                                                               &error))
                                               : (resume_debuggee() &&
                                                  WaitForHandoff(child.hProcess,
                                                                 expected_message,
                                                                 trace,
                                                                 &error)))));
    if (probe_exit_process && exit_probe_prepared)
    {
        std::uint32_t current_exit_target = 0;
        SIZE_T copied = 0;
        if (ReadProcessMemory(child.hProcess,
                              reinterpret_cast<const void*>(main_image_base + exit_slot_rva),
                              &current_exit_target,
                              sizeof(current_exit_target),
                              &copied) != FALSE &&
            copied == sizeof(current_exit_target))
        {
            RecordDiagnostic("{\"exit_process_iat\":\"0x%08x\",\"probe_target\":\"0x%08x\"}",
                             current_exit_target,
                             runtime_base + exit_thunk_rva);
        }
    }
    if (!run_detached || !handoff_observed)
    {
        TerminateProcess(child.hProcess, 0);
    }
    if (reached && !inject_runtime && !break_exit_process && !instruction_trace)
    {
        ContinueDebugEvent(breakpoint_process_id, breakpoint_thread_id, DBG_CONTINUE);
    }
    CloseHandle(child.hThread);
    CloseHandle(child.hProcess);
    if (!reached || !entry_restored || !runtime_loaded || !handoff_prepared ||
        !display_prepared || !d3d3_prepared || !directsound_prepared ||
        !demo_volume_prepared || !audio_trace_prepared || !vfs_prepared ||
        !image_loader_prepared ||
        !io_runtime_prepared || !message_box_prepared || !exit_probe_prepared ||
        !exit_break_prepared || !d3d_init_trace_prepared || !ksnd_load_trace_prepared ||
        !slot_writer_trace_prepared ||
        !null_context_object_source_trace_prepared ||
        !null_context_field_access_trace_prepared ||
        !null_context_field_writer_trace_prepared ||
        !null_context_field_reference_execution_trace_prepared ||
        !null_context_object_state_trace_prepared ||
        !null_context_object_reference_scan_prepared ||
        !null_context_entry_trace_prepared ||
        !handoff_observed ||
        !iat_verified)
    {
        PrintDiagnosticError(error);
        return 3;
    }
    RecordDiagnostic("{\"event\":\"outcome\",\"status\":\"success\",\"target\":\"%s\"}",
                     target->id.c_str());
    std::printf("{\"target\":\"%s\",\"image_base\":\"0x%08x\",\"main_module_base\":\"0x%08x\",\"entry\":\"0x%08x\",\"iat_slots\":%u,\"iat_modules\":%u,\"runtime_base\":\"0x%08x\",\"handoff\":%s,\"breakpoint\":\"%s\",\"diagnostic_log\":\"%s\"}\\n",
                target->id.c_str(),
                expected_base,
                static_cast<unsigned>(main_image_base),
                entry,
                iat.slot_count,
                static_cast<unsigned>(iat.modules.size()),
                runtime_base,
                handoff_observed ? "true" : "false",
                software_breakpoint ? "software" : "hardware",
                diagnostic_log.path().generic_string().c_str());
    return 0;
}
