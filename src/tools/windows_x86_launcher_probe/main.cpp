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
    std::printf("Usage: re2dj_windows_x86_launcher_probe --hdd <directory> [--target <id>] [--software-breakpoint] [--instruction-trace <max-steps>] [--inject-runtime [path]] [--probe-handoff|--hle-command-line|--hle-windows-directory|--hle-vfs|--hle-display-mode|--hle-d3d3|--hle-directsound [--audio-gain-db <-24..18>]|--hle-io-ports|--run-detached|--d3d-init-trace|--ksnd-load-trace|--device-mock-lptdi|--device-mock-lptdi-ioctl-success|--device-mock-lptdi-ioctl-full-success|--device-mock-lptdi-response-profile <path>|--device-mock-lptdi-target-state <16-hex-digits>|--lptdi-post-ioctl-trace <max-steps>|--probe-exit-process|--break-exit-process|--scan-fault-references|--api-trace] [--trace]\n");
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

bool WaitForInitialBreakpoint(DWORD* process_id,
                              DWORD* thread_id,
                              std::uintptr_t* main_image_base,
                              std::uintptr_t* kernel32_base,
                              std::uintptr_t* kernelbase_base,
                              std::uintptr_t* user32_base,
                              std::string* error)
{
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
        RecordDiagnostic("{\"debug_event\":\"exit_process\",\"code\":\"0x%08x\"}",
                         static_cast<unsigned>(event.u.ExitProcess.dwExitCode));
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
    RecordDiagnostic("{\"event\":\"av_access\",\"kind\":\"%s\",\"code\":\"0x%08x\",\"address\":\"0x%08x\"}",
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

    RecordDiagnostic("{\"event\":\"av_registers\",\"eax\":\"0x%08x\",\"ebx\":\"0x%08x\",\"ecx\":\"0x%08x\",\"edx\":\"0x%08x\",\"esi\":\"0x%08x\",\"edi\":\"0x%08x\",\"ebp\":\"0x%08x\",\"esp\":\"0x%08x\",\"eip\":\"0x%08x\",\"flags\":\"0x%08x\"}",
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
        const std::uint32_t rva = static_cast<std::uint32_t>(address - image_base);
        RecordDiagnostic("{\"event\":\"av_stack_code_window\",\"index\":%u,\"address\":\"0x%08x\",\"base\":\"0x%08x\",\"section\":\"%s\",\"bytes\":\"%s\"}",
                         static_cast<unsigned>(index),
                         address,
                         static_cast<unsigned>(start),
                         FindSectionNameForRva(*image_info, rva),
                         text);
    }
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
        bool recorded = false;
        for (const WatchedModule& module : modules)
        {
            if (module.base == 0)
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
    re2dj::input::LegacyIoPortBus* bus,
    bool trace,
    std::string* error)
{
    if (bus == nullptr || exception.ExceptionCode != EXCEPTION_PRIV_INSTRUCTION)
    {
        return IoPortTrapResult::kNotHandled;
    }

    constexpr std::uintptr_t kInByteRva = 0x00038987;
    constexpr std::uintptr_t kOutByteRva = 0x000389ab;
    const std::uintptr_t address =
        reinterpret_cast<std::uintptr_t>(exception.ExceptionAddress);
    const bool is_read = address == image_base + kInByteRva;
    const bool is_write = address == image_base + kOutByteRva;
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
        RecordDiagnostic("{\"event\":\"io_port_%s\",\"thread\":%u,\"address\":\"0x%08x\",\"port\":\"0x%03x\",\"width\":1,\"value\":\"0x%02x\"}",
                         is_read ? "read" : "write",
                         static_cast<unsigned>(thread_id),
                         static_cast<unsigned>(address),
                         static_cast<unsigned>(port),
                         static_cast<unsigned>(value));
    }
    return IoPortTrapResult::kHandled;
}

bool WaitForExitProcessBreakpoint(HANDLE process,
                                  std::uint32_t exit_target,
                                  bool scan_fault_references,
                                  bool trace,
                                  std::uint32_t lptdi_post_ioctl_trace_steps,
                                  std::uintptr_t image_base,
                                  const re2dj::exe::PeImageInfo* image_info,
                                  GuestReturnWatchMap* guest_return_watches,
                                  ApiWatchMap* api_watches,
                                  re2dj::input::LegacyIoPortBus* io_port_bus,
                                  std::string* error)
{
    std::map<DWORD, std::uintptr_t> pending_api_steps;
    std::map<DWORD, std::uintptr_t> pending_guest_return_steps;
    std::map<DWORD, PendingDeviceIoControl> pending_device_io_controls;
    std::map<DWORD, PostDeviceIoControlTrace> post_device_io_control_traces;
    std::set<std::uintptr_t> dynamic_module_bases;
    bool original_initializer_recorded = false;
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
    const std::uint64_t requested_event_cap =
        (io_port_bus == nullptr ? 128ull : 8192ull) +
        static_cast<std::uint64_t>(lptdi_post_ioctl_trace_steps) * 16ull;
    const std::uint32_t normal_event_cap = static_cast<std::uint32_t>(
        (std::min)(requested_event_cap,
                   static_cast<std::uint64_t>((std::numeric_limits<std::uint32_t>::max)())));
    for (std::uint32_t event_count = 0;
         event_count < (unload_tail_collecting ? kUnloadStepCap : normal_event_cap);
         ++event_count)
    {
        DEBUG_EVENT event = {};
        if (WaitForDebugEvent(&event, 5000) == FALSE)
        {
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
        if (event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT)
        {
            const IoPortTrapResult io_result = HandleLegacyIoPortTrap(
                process,
                event.dwThreadId,
                event.u.Exception.ExceptionRecord,
                image_base,
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
        if (event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT && api_watches != nullptr)
        {
            last_activity_thread = event.dwThreadId;
            const DWORD exception_code = event.u.Exception.ExceptionRecord.ExceptionCode;
            const std::uintptr_t exception_address =
                reinterpret_cast<std::uintptr_t>(
                    event.u.Exception.ExceptionRecord.ExceptionAddress);
            if (exception_code == EXCEPTION_SINGLE_STEP)
            {
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
                            IsSyntheticDeviceHandle(pending.args[0]))
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
                context.ContextFlags = CONTEXT_CONTROL;
                std::uint32_t stack[4] = {};
                SIZE_T stack_copied = 0;
                if (thread != nullptr && GetThreadContext(thread, &context) != FALSE &&
                    ReadProcessMemory(process,
                                      reinterpret_cast<const void*>(context.Esp),
                                      stack,
                                      sizeof(stack),
                                      &stack_copied) != FALSE &&
                    stack_copied == sizeof(stack))
                {
                    RecordDiagnostic("{\"exception_esp\":\"0x%08x\",\"exception_stack\":[\"0x%08x\",\"0x%08x\",\"0x%08x\",\"0x%08x\"]}",
                                     static_cast<unsigned>(context.Esp),
                                     stack[0],
                                     stack[1],
                                     stack[2],
                                     stack[3]);
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
            !unload_tail_collecting && last_activity_thread != 0)
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
    *error = "ExitProcess breakpoint was not observed";
    return false;
}

}  // namespace

int re2dj::platform::windows::RunOriginalProcessLauncherCommand(int argc, char** argv)
{
    std::filesystem::path hdd_path;
    std::string target_id = "ez2dj1stse";
    bool trace = false;
    bool software_breakpoint = false;
    bool instruction_trace = false;
    std::uint32_t instruction_trace_max_steps = 0;
    bool inject_runtime = false;
    bool probe_handoff = false;
    bool hle_command_line = false;
    bool hle_windows_directory = false;
    bool hle_vfs = false;
    bool hle_display_mode = false;
    bool hle_d3d3 = false;
    bool hle_directsound = false;
    float audio_gain_db = 0.0f;
    bool audio_gain_set = false;
    bool hle_io_ports = false;
    bool run_detached = false;
    bool d3d_init_trace = false;
    bool ksnd_load_trace = false;
    bool device_mock_lptdi = false;
    bool device_mock_lptdi_ioctl_success = false;
    bool device_mock_lptdi_ioctl_full_success = false;
    std::filesystem::path device_mock_lptdi_response_profile_path;
    std::string device_mock_lptdi_target_state_hex;
    bool probe_exit_process = false;
    bool break_exit_process = false;
    bool scan_fault_references = false;
    bool api_trace = false;
    std::uint32_t lptdi_post_ioctl_trace_steps = 0;
    std::filesystem::path runtime_path;
    for (int index = 1; index < argc; ++index)
    {
        const std::string option = argv[index];
        if (option == "--hdd" && index + 1 < argc)
        {
            hdd_path = argv[++index];
        }
        else if (option == "--target" && index + 1 < argc)
        {
            target_id = argv[++index];
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
        else if (option == "--hle-display-mode")
        {
            hle_display_mode = true;
            inject_runtime = true;
            software_breakpoint = true;
        }
        else if (option == "--hle-d3d3")
        {
            hle_d3d3 = true;
            hle_display_mode = true;
            inject_runtime = true;
            software_breakpoint = true;
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
        else if (option == "--hle-io-ports")
        {
            hle_io_ports = true;
            break_exit_process = true;
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
        else if (option == "--api-trace")
        {
            api_trace = true;
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
    if (audio_gain_set && !hle_directsound)
    {
        PrintUsage();
        return 1;
    }
    if (instruction_trace && (probe_handoff || hle_command_line || hle_windows_directory ||
                              hle_vfs || hle_display_mode || hle_d3d3 || hle_directsound || hle_io_ports || d3d_init_trace || ksnd_load_trace || probe_exit_process ||
                              break_exit_process))
    {
        PrintUsage();
        return 1;
    }
    if (run_detached &&
        (!hle_io_ports || trace || instruction_trace || d3d_init_trace || ksnd_load_trace ||
         api_trace || probe_exit_process || scan_fault_references ||
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

    re2dj::hdd::HddRoot root;
    if (!re2dj::hdd::HddRoot::Open(hdd_path, &root, &error))
    {
        std::fprintf(stderr, "{\"error\":\"%s\"}\\n", error.c_str());
        return 2;
    }
    const re2dj::hdd::HddScanResult scan = re2dj::hdd::ScanHdd(root);
    const std::vector<re2dj::target::TargetProfile> profiles =
        re2dj::target::BuildTargetProfiles(root, scan);
    const re2dj::target::TargetProfile* target =
        re2dj::target::FindTargetProfileById(profiles, target_id);
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
    if (hle_d3d3 && target->id != "ez2dj1stse")
    {
        std::fprintf(stderr, "{\"error\":\"Direct3D3 HLE requires ez2dj1stse target\"}\\n");
        return 2;
    }
    if (hle_directsound && target->id != "ez2dj1stse")
    {
        std::fprintf(stderr, "{\"error\":\"DirectSound HLE requires ez2dj1stse target\"}\\n");
        return 2;
    }
    if (hle_io_ports && target->id != "ez2dj1stse")
    {
        std::fprintf(stderr, "{\"error\":\"legacy I/O port HLE requires ez2dj1stse target\"}\\n");
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
    RecordDiagnostic("{\"event\":\"launch\",\"target\":\"%s\",\"executable\":\"%s\",\"trace\":%s,\"software_breakpoint\":%s,\"instruction_trace_steps\":%u,\"api_trace\":%s,\"hle_display_mode\":%s,\"hle_d3d3\":%s,\"hle_directsound\":%s,\"hle_io_ports\":%s,\"run_detached\":%s,\"d3d_init_trace\":%s,\"ksnd_load_trace\":%s,\"device_mock_lptdi\":%s,\"device_mock_lptdi_ioctl_success\":%s,\"device_mock_lptdi_ioctl_full_success\":%s,\"device_response_profile_entries\":%u,\"device_target_state\":%s,\"lptdi_post_ioctl_trace_steps\":%u}",
                     target->id.c_str(),
                     executable.generic_string().c_str(),
                     trace ? "true" : "false",
                     software_breakpoint ? "true" : "false",
                     instruction_trace_max_steps,
                     api_trace ? "true" : "false",
                     hle_display_mode ? "true" : "false",
                     hle_d3d3 ? "true" : "false",
                     hle_directsound ? "true" : "false",
                     hle_io_ports ? "true" : "false",
                     run_detached ? "true" : "false",
                     d3d_init_trace ? "true" : "false",
                     ksnd_load_trace ? "true" : "false",
                     device_mock_lptdi ? "true" : "false",
                     device_mock_lptdi_ioctl_success ? "true" : "false",
                     device_mock_lptdi_ioctl_full_success ? "true" : "false",
                     static_cast<unsigned>(device_response_profile.entries.size()),
                     device_mock_lptdi_target_state_hex.empty() ? "false" : "true",
                     lptdi_post_ioctl_trace_steps);
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
    const bool reached_initial = WaitForInitialBreakpoint(&breakpoint_process_id,
                                                          &breakpoint_thread_id,
                                                          &main_image_base,
                                                          &kernel32_base,
                                                          &kernelbase_base,
                                                          &user32_base,
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
    bool display_prepared = !hle_display_mode;
    if (hle_display_mode && runtime_loaded)
    {
        std::uint32_t display_thunk_rva = 0;
        std::uint32_t display_slot_rva = 0;
        display_prepared = re2dj::platform::windows::FindPe32ExportRva(
                               runtime_path,
                               "_Re2djHleChangeDisplaySettingsExA@20",
                               &display_thunk_rva,
                               &error) &&
                           re2dj::tools::windows_original_process_probe::FindIatSlotByName(
                               info,
                               file.data(),
                               file.size(),
                               "USER32.dll",
                               "ChangeDisplaySettingsExA",
                               &display_slot_rva,
                               &error) &&
                           WriteRemoteU32(child.hProcess,
                                          main_image_base + display_slot_rva,
                                          runtime_base + display_thunk_rva,
                                          &error);
    }
    bool d3d3_prepared = !hle_d3d3;
    if (hle_d3d3 && runtime_loaded)
    {
        std::uint32_t d3d3_thunk_rva = 0;
        std::uint32_t d3d3_slot_rva = 0;
        std::uint32_t graphics_trace_path_rva = 0;
        std::filesystem::path graphics_trace_path = diagnostic_log.path();
        graphics_trace_path.replace_extension(".ddraw.log");
        d3d3_prepared = re2dj::platform::windows::FindPe32ExportRva(
                            runtime_path,
                            "_Re2djHleDirectDrawCreate@12",
                            &d3d3_thunk_rva,
                            &error) &&
                        re2dj::platform::windows::FindPe32ExportRva(
                            runtime_path,
                            "g_re2dj_graphics_trace_path",
                            &graphics_trace_path_rva,
                            &error) &&
                        re2dj::tools::windows_original_process_probe::FindIatSlotByName(
                            info,
                            file.data(),
                            file.size(),
                            "DDRAW.dll",
                            "DirectDrawCreate",
                            &d3d3_slot_rva,
                            &error) &&
                        WriteRemoteU32(child.hProcess,
                                       main_image_base + d3d3_slot_rva,
                                       runtime_base + d3d3_thunk_rva,
                                       &error) &&
                        WriteRemoteAnsi(child.hProcess,
                                        runtime_base + graphics_trace_path_rva,
                                        graphics_trace_path.string(),
                                       &error);
        if (d3d3_prepared)
        {
            RecordDiagnostic("{\"event\":\"graphics_trace\",\"path\":\"%s\"}",
                             graphics_trace_path.generic_string().c_str());
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
        if (directsound_prepared && audio_gain_set)
        {
            RecordDiagnostic("{\"event\":\"audio_master_gain\",\"db\":%.3f,\"linear\":%.6f}",
                             static_cast<double>(audio_gain_db),
                             static_cast<double>(audio_master_gain));
        }
    }
    const char* const vfs_exports[] = {"_Re2djVfsCreateFileA@28",
                                       "_Re2djVfsReadFile@20",
                                       "_Re2djVfsWriteFile@20",
                                       "_Re2djVfsSetFilePointer@16",
                                       "_Re2djVfsGetFileSize@8",
                                       "_Re2djVfsCloseHandle@4",
                                       "_Re2djVfsGetFileType@4"};
    const char* const vfs_imports[] = {"CreateFileA",
                                       "ReadFile",
                                       "WriteFile",
                                       "SetFilePointer",
                                       "GetFileSize",
                                       "CloseHandle",
                                       "GetFileType"};
    bool vfs_prepared = !hle_vfs;
    // Tracked apart from vfs_prepared so a failed image-loader patch reports
    // itself instead of silently skipping the device patches that follow it.
    bool image_loader_prepared = !hle_vfs;
    std::uint32_t device_ioctl_wrapper_address = 0;
    if (hle_vfs && runtime_loaded)
    {
        std::uint32_t hdd_root_rva = 0;
        std::uint32_t overlay_root_rva = 0;
        std::uint32_t vfs_trace_path_rva = 0;
        std::uint32_t device_mock_rva = 0;
        std::uint32_t device_ioctl_mode_rva = 0;
        const std::filesystem::path overlay = std::filesystem::current_path() / "overlays" / target->id;
        std::filesystem::path vfs_trace_path = diagnostic_log.path();
        vfs_trace_path.replace_extension(".vfs.log");
        vfs_prepared = re2dj::platform::windows::FindPe32ExportRva(
                           runtime_path, "g_re2dj_vfs_hdd_root", &hdd_root_rva, &error) &&
                       re2dj::platform::windows::FindPe32ExportRva(
                           runtime_path, "g_re2dj_vfs_overlay_root", &overlay_root_rva, &error) &&
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
                                       runtime_base + vfs_trace_path_rva,
                                       vfs_trace_path.string(),
                                       &error);
        if (vfs_prepared)
        {
            RecordDiagnostic("{\"event\":\"vfs_mount\",\"dump_root\":\"%s\",\"working_directory\":\"%s\",\"source_root\":\"%s\",\"overlay_root\":\"%s\"}",
                             root.root().generic_string().c_str(),
                             target->working_directory_relative_path.c_str(),
                             vfs_source_root.generic_string().c_str(),
                             overlay.generic_string().c_str());
            RecordDiagnostic("{\"event\":\"vfs_trace\",\"path\":\"%s\"}",
                             vfs_trace_path.generic_string().c_str());
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
            vfs_prepared = re2dj::platform::windows::FindPe32ExportRva(
                               runtime_path, vfs_exports[index], &export_rva, &error) &&
                           re2dj::tools::windows_original_process_probe::FindIatSlotByName(
                               info,
                               file.data(),
                               file.size(),
                               "KERNEL32.dll",
                               vfs_imports[index],
                               &slot_rva,
                               &error) &&
                           WriteRemoteU32(child.hProcess,
                                          main_image_base + slot_rva,
                                          runtime_base + export_rva,
                                          &error);
        }
        if (vfs_prepared)
        {
            std::uint32_t export_rva = 0;
            std::uint32_t slot_rva = 0;
            image_loader_prepared = re2dj::platform::windows::FindPe32ExportRva(
                                        runtime_path,
                                        "_Re2djVfsLoadImageA@24",
                                        &export_rva,
                                        &error) &&
                                    re2dj::tools::windows_original_process_probe::FindIatSlotByName(
                                        info,
                                        file.data(),
                                        file.size(),
                                        "USER32.dll",
                                        "LoadImageA",
                                        &slot_rva,
                                        &error) &&
                                    WriteRemoteU32(child.hProcess,
                                                   main_image_base + slot_rva,
                                                   runtime_base + export_rva,
                                                   &error);
            RecordDiagnostic("{\"event\":\"vfs_image_loader\",\"import\":\"USER32.dll!LoadImageA\",\"prepared\":%s}",
                             image_loader_prepared ? "true" : "false");
        }
        if (vfs_prepared &&
            device_ioctl_policy_count != 0)
        {
            std::uint32_t export_rva = 0;
            std::uint32_t slot_rva = 0;
            vfs_prepared = re2dj::platform::windows::FindPe32ExportRva(
                               runtime_path,
                               "_Re2djDeviceIoControlMock@32",
                               &export_rva,
                               &error) &&
                           re2dj::tools::windows_original_process_probe::FindIatSlotByName(
                               info,
                               file.data(),
                               file.size(),
                               "KERNEL32.dll",
                               "DeviceIoControl",
                               &slot_rva,
                               &error) &&
                           WriteRemoteU32(child.hProcess,
                                          main_image_base + slot_rva,
                                          runtime_base + export_rva,
                                          &error);
            if (vfs_prepared)
            {
                device_ioctl_wrapper_address = runtime_base + export_rva;
            }
        }
    }
    bool io_runtime_prepared = !run_detached;
    if (run_detached && runtime_loaded)
    {
        std::uint32_t enable_rva = 0;
        std::uint32_t image_base_rva = 0;
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
                              WriteRemoteU32(child.hProcess,
                                             runtime_base + image_base_rva,
                                             static_cast<std::uint32_t>(main_image_base),
                                             &error) &&
                              WriteRemoteU32(child.hProcess,
                                             runtime_base + enable_rva,
                                             1,
                                             &error);
        if (io_runtime_prepared)
        {
            RecordDiagnostic("{\"event\":\"io_port_runtime\",\"image_base\":\"0x%08x\",\"status\":\"prepared\"}",
                             static_cast<unsigned>(main_image_base));
        }
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
        exit_break_prepared = re2dj::tools::windows_original_process_probe::FindIatSlotByName(
                                  info,
                                  file.data(),
                                  file.size(),
                                  "KERNEL32.dll",
                                  "ExitProcess",
                                  &exit_break_slot_rva,
                                  &error) &&
                              ReadProcessMemory(child.hProcess,
                                                reinterpret_cast<const void*>(main_image_base + exit_break_slot_rva),
                                                &exit_break_target,
                                                sizeof(exit_break_target),
                                                &copied) != FALSE &&
                              copied == sizeof(exit_break_target) &&
                              SetSoftwareEntryBreakpoint(child.hProcess,
                                                         exit_break_target,
                                                         &original_exit_byte,
                                                         &error);
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
    re2dj::input::LegacyIoPortBus io_port_bus;
    bool api_trace_prepared = !api_trace;
    if (api_trace)
    {
        const bool system_watches_installed =
            reached && entry_restored && exit_break_prepared &&
            InstallApiTraceBreakpoints(child.hProcess,
                                       kernel32_base,
                                       kernelbase_base,
                                       user32_base,
                                       &api_watches,
                                       &error);
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
    re2dj::tools::windows_original_process_probe::IatVerificationResult iat;
    const bool iat_verified = reached && entry_restored && runtime_loaded && handoff_prepared &&
                              display_prepared && d3d3_prepared && directsound_prepared && vfs_prepared &&
                              image_loader_prepared &&
                              io_runtime_prepared && exit_probe_prepared &&
                              exit_break_prepared && d3d_init_trace_prepared &&
                              ksnd_load_trace_prepared &&
                              api_trace_prepared &&
                              re2dj::tools::windows_original_process_probe::VerifySuspendedIat(
                                  child.hProcess,
                                  main_image_base,
                                  info,
                                  file.data(),
                                  file.size(),
                                  &iat,
                                  &error);
    const bool resume_for_handoff = handoff_requested || hle_vfs || hle_display_mode || hle_d3d3 ||
                                    probe_exit_process || break_exit_process;
    const char* const expected_message = probe_exit_process ? "re2dj:probe:ExitProcess"
                                                             : (hle_vfs ? "re2dj:vfs:CreateFileA"
                                                                : hle_display_mode
                                                                    ? "re2dj:hle:ChangeDisplaySettingsExA"
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
                                         directsound_prepared && vfs_prepared &&
                                         image_loader_prepared &&
                                         io_runtime_prepared && exit_probe_prepared &&
                                         exit_break_prepared && d3d_init_trace_prepared &&
                                         ksnd_load_trace_prepared && api_trace_prepared &&
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
                                         (handoff_prepared && display_prepared && d3d3_prepared && directsound_prepared && vfs_prepared &&
                                          image_loader_prepared &&
                                          io_runtime_prepared &&
                                          exit_probe_prepared &&
                                          exit_break_prepared && d3d_init_trace_prepared &&
                                          ksnd_load_trace_prepared &&
                                          api_trace_prepared &&
                                          (break_exit_process
                                               ? (resume_debuggee() &&
                                                  WaitForExitProcessBreakpoint(child.hProcess,
                                                                               exit_break_target,
                                                                               scan_fault_references,
                                                                               trace,
                                                                               lptdi_post_ioctl_trace_steps,
                                                                               main_image_base,
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
        !display_prepared || !d3d3_prepared || !directsound_prepared || !vfs_prepared ||
        !image_loader_prepared ||
        !io_runtime_prepared || !exit_probe_prepared ||
        !exit_break_prepared || !d3d_init_trace_prepared || !ksnd_load_trace_prepared ||
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
