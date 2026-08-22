#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <array>
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
#include <string>
#include <vector>

#include "re2dj/exe/pe_image.h"
#include "re2dj/hdd/hdd_root.h"
#include "re2dj/hdd/hdd_scan.h"
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
    std::printf("Usage: re2dj_windows_x86_launcher_probe --hdd <directory> [--target <id>] [--software-breakpoint] [--instruction-trace <max-steps>] [--inject-runtime [path]] [--probe-handoff|--hle-command-line|--hle-windows-directory|--hle-vfs|--probe-exit-process|--break-exit-process|--scan-fault-references|--api-trace] [--trace]\\n");
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
        if (event.dwDebugEventCode == OUTPUT_DEBUG_STRING_EVENT &&
            event.u.DebugString.fUnicode == FALSE && event.u.DebugString.nDebugStringLength > 0)
        {
            std::vector<char> message(event.u.DebugString.nDebugStringLength + 1, '\0');
            SIZE_T copied = 0;
            if (ReadProcessMemory(process,
                                  event.u.DebugString.lpDebugStringData,
                                  message.data(),
                                  event.u.DebugString.nDebugStringLength,
                                  &copied) != FALSE &&
                copied == event.u.DebugString.nDebugStringLength)
            {
                RecordDiagnostic("{\"debug_event\":\"output_debug\",\"message\":\"%s\"}",
                                 message.data());
                if (std::string(message.data()) == expected_message)
                {
                    return true;
                }
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

struct ApiWatchPoint
{
    std::string name;
    int string_arg_index = -1;
    std::uint8_t original_byte = 0;
};

using ApiWatchMap = std::map<std::uintptr_t, ApiWatchPoint>;

// One ANSI string argument is decoded for APIs whose arguments identify what
// the guest loads, resolves, or opens. Index counts from the first stack
// argument.
int ApiStringArgumentIndex(const char* name)
{
    if (std::strcmp(name, "LoadLibraryA") == 0 || std::strcmp(name, "CreateFileA") == 0)
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
}

bool InstallApiTraceBreakpoints(HANDLE process,
                                std::uintptr_t kernel32_base,
                                std::uintptr_t kernelbase_base,
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

bool WaitForExitProcessBreakpoint(HANDLE process,
                                  std::uint32_t exit_target,
                                  bool scan_fault_references,
                                  bool trace,
                                  std::uintptr_t image_base,
                                  const re2dj::exe::PeImageInfo* image_info,
                                  ApiWatchMap* api_watches,
                                  std::string* error)
{
    (void)trace;
    std::map<DWORD, std::uintptr_t> pending_api_steps;
    std::set<std::uintptr_t> dynamic_module_bases;
    bool unload_tail_collecting = false;
    DWORD unload_tail_thread = 0;
    std::vector<InstructionSample> unload_history;
    constexpr std::size_t kUnloadHistoryCapacity = 48;
    unload_history.reserve(kUnloadHistoryCapacity);
    constexpr std::uint32_t kUnloadStepCap = 200000;
    std::uint32_t unload_steps = 0;
    DWORD last_activity_thread = 0;
    // One-shot software breakpoint planted on a detected syscall stub's
    // return address so execution can be re-traced after the WOW64 gate
    // kills single-step reporting.
    bool resume_bp_armed = false;
    bool resume_bp_consumed = false;
    std::uint32_t resume_bp_address = 0;
    std::uint8_t resume_bp_original = 0;
    std::map<std::pair<std::uintptr_t, std::uintptr_t>, std::string> symbol_cache;
    // Names the nearest known export for one sampled address. Image regions
    // resolve through their allocation base; private pages stay unnamed.
    auto annotate_sample_symbol = [&](InstructionSample& sample) {
        MEMORY_BASIC_INFORMATION region = {};
        if (VirtualQueryEx(process,
                           reinterpret_cast<const void*>(sample.address),
                           &region,
                           sizeof(region)) != sizeof(region) ||
            region.Type != MEM_IMAGE ||
            region.AllocationBase == nullptr)
        {
            return;
        }
        const std::uintptr_t module_base =
            reinterpret_cast<std::uintptr_t>(region.AllocationBase);
        const auto cached = symbol_cache.find({module_base, sample.address});
        if (cached != symbol_cache.end())
        {
            sample.symbol = cached->second;
            return;
        }
        re2dj::tools::windows_x86_launcher_probe::RemoteNearestExport nearest = {};
        std::string nearest_error;
        if (re2dj::tools::windows_x86_launcher_probe::FindRemotePe32NearestExport(
                process,
                module_base,
                sample.address,
                &nearest,
                &nearest_error))
        {
            char text[288] = {};
            std::snprintf(text,
                          sizeof(text),
                          "%s!%s+0x%x",
                          nearest.module,
                          nearest.function,
                          static_cast<unsigned>(nearest.offset));
            SanitizeJsonText(text);
            symbol_cache[{module_base, sample.address}] = text;
            sample.symbol = text;
        }
    };
    for (std::uint32_t event_count = 0;
         event_count < (unload_tail_collecting ? kUnloadStepCap : 128);
         ++event_count)
    {
        DEBUG_EVENT event = {};
        if (WaitForDebugEvent(&event, 5000) == FALSE)
        {
            *error = "cannot wait for ExitProcess breakpoint";
            return false;
        }
        TraceDebugEvent(event);
        if (event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT && api_watches != nullptr)
        {
            last_activity_thread = event.dwThreadId;
            const DWORD exception_code = event.u.Exception.ExceptionRecord.ExceptionCode;
            const std::uintptr_t exception_address =
                reinterpret_cast<std::uintptr_t>(
                    event.u.Exception.ExceptionRecord.ExceptionAddress);
            if (exception_code == EXCEPTION_SINGLE_STEP)
            {
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
                    if (ContinueDebugEvent(event.dwProcessId,
                                           event.dwThreadId,
                                           DBG_CONTINUE) == FALSE)
                    {
                        *error = "cannot continue watched API rearm step";
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
                    if (!resume_bp_armed && previous != nullptr &&
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
                            resume_bp_consumed = false;
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
                if (resume_bp_armed && !resume_bp_consumed &&
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
                    std::uint32_t stack_words[4] = {};
                    SIZE_T copied = 0;
                    const bool captured =
                        thread != nullptr && GetThreadContext(thread, &context) != FALSE &&
                        ReadProcessMemory(process,
                                          reinterpret_cast<const void*>(context.Esp),
                                          stack_words,
                                          sizeof(stack_words),
                                          &copied) != FALSE &&
                        copied == sizeof(stack_words);
                    if (!captured)
                    {
                        *error = "cannot capture syscall resume context";
                    }
                    else
                    {
                        RecordDiagnostic("{\"event\":\"syscall_resume_hit\",\"address\":\"0x%08x\",\"eax\":\"0x%08x\",\"ebx\":\"0x%08x\",\"ecx\":\"0x%08x\",\"edx\":\"0x%08x\",\"esi\":\"0x%08x\",\"edi\":\"0x%08x\",\"ebp\":\"0x%08x\",\"esp\":\"0x%08x\",\"stack\":[\"0x%08x\",\"0x%08x\",\"0x%08x\",\"0x%08x\"]}",
                                         resume_bp_address,
                                         static_cast<unsigned>(context.Eax),
                                         static_cast<unsigned>(context.Ebx),
                                         static_cast<unsigned>(context.Ecx),
                                         static_cast<unsigned>(context.Edx),
                                         static_cast<unsigned>(context.Esi),
                                         static_cast<unsigned>(context.Edi),
                                         static_cast<unsigned>(context.Ebp),
                                         static_cast<unsigned>(context.Esp),
                                         stack_words[0],
                                         stack_words[1],
                                         stack_words[2],
                                         stack_words[3]);
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
                            resume_bp_consumed = true;
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
                    std::uint32_t stack[5] = {};
                    SIZE_T copied = 0;
                    const bool captured =
                        thread != nullptr && GetThreadContext(thread, &context) != FALSE &&
                        ReadProcessMemory(process,
                                          reinterpret_cast<const void*>(context.Esp),
                                          stack,
                                          sizeof(stack),
                                          &copied) != FALSE &&
                        copied == sizeof(stack);
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
            SIZE_T copied = 0;
            const bool captured = thread != nullptr && GetThreadContext(thread, &context) != FALSE &&
                                  ReadProcessMemory(process,
                                                    reinterpret_cast<const void*>(context.Esp),
                                                    &return_address,
                                                    sizeof(return_address),
                                                    &copied) != FALSE &&
                                  copied == sizeof(return_address);
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

int main(int argc, char** argv)
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
    bool probe_exit_process = false;
    bool break_exit_process = false;
    bool scan_fault_references = false;
    bool api_trace = false;
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
    if (instruction_trace && (probe_handoff || hle_command_line || hle_windows_directory ||
                              hle_vfs || probe_exit_process || break_exit_process))
    {
        PrintUsage();
        return 1;
    }

    g_trace = trace;

    std::string error;
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
    RecordDiagnostic("{\"event\":\"launch\",\"target\":\"%s\",\"executable\":\"%s\",\"trace\":%s,\"software_breakpoint\":%s,\"instruction_trace_steps\":%u,\"api_trace\":%s}",
                     target->id.c_str(),
                     executable.generic_string().c_str(),
                     trace ? "true" : "false",
                     software_breakpoint ? "true" : "false",
                     instruction_trace_max_steps,
                     api_trace ? "true" : "false");
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
    const std::uint32_t expected_base = static_cast<std::uint32_t>(info.image_base);
    const std::uint32_t entry = expected_base + info.entry_point_rva;
    std::uint8_t original_entry_byte = 0;
    const bool reached_initial = WaitForInitialBreakpoint(&breakpoint_process_id,
                                                          &breakpoint_thread_id,
                                                          &main_image_base,
                                                          &kernel32_base,
                                                          &kernelbase_base,
                                                          &error);
    RecordDiagnostic("{\"event\":\"system_modules\",\"image_base\":\"0x%08x\",\"kernel32\":\"0x%08x\",\"kernelbase\":\"0x%08x\"}",
                     static_cast<unsigned>(main_image_base),
                     static_cast<unsigned>(kernel32_base),
                     static_cast<unsigned>(kernelbase_base));
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
    if (hle_vfs && runtime_loaded)
    {
        std::uint32_t hdd_root_rva = 0;
        std::uint32_t overlay_root_rva = 0;
        const std::filesystem::path overlay = std::filesystem::current_path() / "overlays" / target->id;
        vfs_prepared = re2dj::platform::windows::FindPe32ExportRva(
                           runtime_path, "g_re2dj_vfs_hdd_root", &hdd_root_rva, &error) &&
                       re2dj::platform::windows::FindPe32ExportRva(
                           runtime_path, "g_re2dj_vfs_overlay_root", &overlay_root_rva, &error) &&
                       WriteRemoteAnsi(child.hProcess,
                                       runtime_base + hdd_root_rva,
                                       root.root().string(),
                                       &error) &&
                       WriteRemoteAnsi(child.hProcess,
                                       runtime_base + overlay_root_rva,
                                       overlay.string(),
                                       &error);
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
    ApiWatchMap api_watches;
    bool api_trace_prepared = !api_trace;
    if (api_trace)
    {
        if (reached && entry_restored && exit_break_prepared &&
            InstallApiTraceBreakpoints(child.hProcess,
                                       kernel32_base,
                                       kernelbase_base,
                                       &api_watches,
                                       &error))
        {
            api_trace_prepared = true;
        }
        else if (error.empty())
        {
            error = "cannot install API trace breakpoints";
        }
        RecordDiagnostic("{\"event\":\"api_trace_ready\",\"watches\":%u,\"prepared\":%s,\"kernel32\":\"0x%08x\",\"kernelbase\":\"0x%08x\"}",
                         static_cast<unsigned>(api_watches.size()),
                         api_trace_prepared ? "true" : "false",
                         static_cast<unsigned>(kernel32_base),
                         static_cast<unsigned>(kernelbase_base));
    }
    re2dj::tools::windows_original_process_probe::IatVerificationResult iat;
    const bool iat_verified = reached && entry_restored && runtime_loaded && handoff_prepared &&
                              vfs_prepared && exit_probe_prepared && exit_break_prepared &&
                              api_trace_prepared &&
                              re2dj::tools::windows_original_process_probe::VerifySuspendedIat(
                                  child.hProcess,
                                  main_image_base,
                                  info,
                                  file.data(),
                                  file.size(),
                                  &iat,
                                  &error);
    const bool resume_for_handoff = handoff_requested || hle_vfs || probe_exit_process ||
                                    break_exit_process;
    const char* const expected_message = probe_exit_process ? "re2dj:probe:ExitProcess"
                                                             : (hle_vfs ? "re2dj:vfs:CreateFileA"
                                                                        : handoff_message);
    const bool handoff_observed = instruction_trace
                                      ? (entry_restored &&
                                         EnableSingleStep(child.hThread, entry, &error) &&
                                         ContinueDebugEvent(breakpoint_process_id,
                                                            breakpoint_thread_id,
                                                            DBG_CONTINUE) != FALSE &&
                                         WaitForInstructionTrace(child.hProcess,
                                                                 breakpoint_thread_id,
                                                                 instruction_trace_max_steps,
                                                                 &error))
                                      : (!resume_for_handoff ||
                                         (handoff_prepared && vfs_prepared && exit_probe_prepared &&
                                          exit_break_prepared && api_trace_prepared &&
                                          (break_exit_process
                                               ? (ContinueDebugEvent(breakpoint_process_id,
                                                                     breakpoint_thread_id,
                                                                     DBG_CONTINUE) != FALSE &&
                                            WaitForExitProcessBreakpoint(child.hProcess,
                                                                  exit_break_target,
                                                                  scan_fault_references,
                                                                  trace,
                                                                  main_image_base,
                                                                  &info,
                                                                  &api_watches,
                                                                          &error))
                                               : (ResumeThread(child.hThread) !=
                                                          (std::numeric_limits<DWORD>::max)() &&
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
    TerminateProcess(child.hProcess, 0);
    if (reached && !inject_runtime && !break_exit_process && !instruction_trace)
    {
        ContinueDebugEvent(breakpoint_process_id, breakpoint_thread_id, DBG_CONTINUE);
    }
    CloseHandle(child.hThread);
    CloseHandle(child.hProcess);
    if (!reached || !entry_restored || !runtime_loaded || !handoff_prepared || !vfs_prepared || !exit_probe_prepared || !exit_break_prepared || !handoff_observed ||
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
