#define NOMINMAX
#include <windows.h>
#include <winternl.h>

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#include "re2dj/exe/pe_image.h"
#include "re2dj/hdd/hdd_root.h"
#include "re2dj/hdd/hdd_scan.h"
#include "re2dj/target/target_profile.h"

#include "iat_verifier.h"
#include "runtime_loader.h"

namespace
{

void PrintUsage()
{
    std::printf("Usage: re2dj_windows_original_process_probe --hdd <directory> [--target <id>] [--verify-iat] [--initial-breakpoint|--entry-breakpoint] [--inject-runtime <dll>]\n");
}

bool FindBundledRuntime(std::filesystem::path* runtime_path, std::string* error)
{
    std::vector<wchar_t> buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size())
    {
        *error = "cannot determine probe executable path";
        return false;
    }
    const std::filesystem::path candidate = std::filesystem::path(buffer.data()).parent_path() /
                                            L"helpers" / L"win32" /
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

bool ReadWow64MainImageBase(HANDLE process, std::uintptr_t* base, std::string* error)
{
    using NtQueryInformationProcessFunction = NTSTATUS(NTAPI*)(HANDLE,
                                                                PROCESSINFOCLASS,
                                                                PVOID,
                                                                ULONG,
                                                                PULONG);
    const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    const auto query = ntdll == nullptr
                           ? nullptr
                           : reinterpret_cast<NtQueryInformationProcessFunction>(
                                 GetProcAddress(ntdll, "NtQueryInformationProcess"));
    ULONG_PTR peb_address = 0;
    constexpr PROCESSINFOCLASS kProcessWow64Information =
        static_cast<PROCESSINFOCLASS>(26);
    if (query == nullptr ||
        query(process,
              kProcessWow64Information,
              &peb_address,
              static_cast<ULONG>(sizeof(peb_address)),
              nullptr) < 0 ||
        peb_address == 0)
    {
        *error = "cannot query suspended process WOW64 PEB";
        return false;
    }
    constexpr std::uintptr_t kPeb32ImageBaseOffset = 8;
    std::uint32_t image_base = 0;
    SIZE_T copied = 0;
    if (ReadProcessMemory(process,
                          reinterpret_cast<const void*>(peb_address +
                                                        kPeb32ImageBaseOffset),
                          &image_base,
                          sizeof(image_base),
                          &copied) == FALSE ||
        copied != sizeof(image_base) || image_base == 0)
    {
        *error = "cannot read suspended process WOW64 PEB image base";
        return false;
    }
    *base = image_base;
    return true;
}

bool WaitForInitialBreakpoint(DWORD* process_id, DWORD* thread_id, std::string* error)
{
    for (std::uint32_t event_count = 0; event_count < 128; ++event_count)
    {
        DEBUG_EVENT event = {};
        if (WaitForDebugEvent(&event, 5000) == FALSE)
        {
            *error = "cannot wait for original-process debug event";
            return false;
        }
        if (event.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT &&
            event.u.CreateProcessInfo.hFile != nullptr)
        {
            CloseHandle(event.u.CreateProcessInfo.hFile);
        }
        if (event.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT && event.u.LoadDll.hFile != nullptr)
        {
            CloseHandle(event.u.LoadDll.hFile);
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

bool SetWow64EntryBreakpoint(HANDLE thread, std::uint32_t entry, std::string* error)
{
    WOW64_CONTEXT context = {};
    context.ContextFlags = WOW64_CONTEXT_DEBUG_REGISTERS;
    if (Wow64GetThreadContext(thread, &context) == FALSE)
    {
        *error = "cannot read WOW64 thread debug registers";
        return false;
    }
    context.Dr0 = entry;
    context.Dr7 = (context.Dr7 & ~static_cast<DWORD>(3)) | 1;
    if (Wow64SetThreadContext(thread, &context) == FALSE)
    {
        *error = "cannot set WOW64 entry breakpoint";
        return false;
    }
    return true;
}

bool WaitForEntryBreakpoint(std::uint32_t entry,
                            DWORD* process_id,
                            DWORD* thread_id,
                            std::string* error)
{
    constexpr DWORD kStatusWx86Breakpoint = 0x4000001F;
    constexpr DWORD kStatusWx86SingleStep = 0x4000001E;
    for (std::uint32_t event_count = 0; event_count < 128; ++event_count)
    {
        DEBUG_EVENT event = {};
        if (WaitForDebugEvent(&event, 5000) == FALSE)
        {
            *error = "cannot wait for entry breakpoint";
            return false;
        }
        if (event.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT && event.u.LoadDll.hFile != nullptr)
        {
            CloseHandle(event.u.LoadDll.hFile);
        }
        if (event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT &&
            (event.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_SINGLE_STEP ||
             event.u.Exception.ExceptionRecord.ExceptionCode == kStatusWx86SingleStep))
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
                          "entry breakpoint produced single-step at 0x%08x instead of 0x%08x",
                          static_cast<unsigned>(address),
                          entry);
            *error = message;
            return false;
        }
        if (event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT &&
            event.u.Exception.ExceptionRecord.ExceptionCode == kStatusWx86Breakpoint)
        {
            if (ContinueDebugEvent(event.dwProcessId, event.dwThreadId, DBG_CONTINUE) == FALSE)
            {
                *error = "cannot continue WOW64 transition breakpoint";
                return false;
            }
            continue;
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
            *error = "original process exited before entry breakpoint";
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

}  // namespace

int main(int argc, char** argv)
{
    std::filesystem::path hdd_path;
    std::string target_id = "ez2dj1stse_unpacked";
    bool verify_iat = false;
    bool initial_breakpoint = false;
    bool entry_breakpoint = false;
    bool inject_runtime = false;
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
        else if (option == "--verify-iat")
        {
            verify_iat = true;
        }
        else if (option == "--initial-breakpoint")
        {
            initial_breakpoint = true;
            verify_iat = true;
        }
        else if (option == "--entry-breakpoint")
        {
            entry_breakpoint = true;
            verify_iat = true;
        }
        else if (option == "--inject-runtime")
        {
            inject_runtime = true;
            if (index + 1 < argc && std::string(argv[index + 1]).rfind("--", 0) != 0)
            {
                runtime_path = argv[++index];
            }
            entry_breakpoint = true;
            verify_iat = true;
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
    re2dj::hdd::HddRoot root;
    std::string error;
    if (inject_runtime && runtime_path.empty() && !FindBundledRuntime(&runtime_path, &error))
    {
        std::fprintf(stderr, "{\"error\":\"%s\"}\n", error.c_str());
        return 1;
    }
    if (inject_runtime && !std::filesystem::is_regular_file(runtime_path))
    {
        std::fprintf(stderr, "{\"error\":\"injected runtime does not exist\"}\n");
        return 1;
    }
    if (!re2dj::hdd::HddRoot::Open(hdd_path, &root, &error))
    {
        std::fprintf(stderr, "{\"error\":\"%s\"}\n", error.c_str());
        return 2;
    }
    const re2dj::hdd::HddScanResult scan = re2dj::hdd::ScanHdd(root);
    const std::vector<re2dj::target::TargetProfile> profiles =
        re2dj::target::BuildTargetProfiles(root, scan);
    const re2dj::target::TargetProfile* target =
        re2dj::target::FindTargetProfileById(profiles, target_id);
    std::filesystem::path executable;
    re2dj::exe::PeImageInfo info;
    if (target == nullptr || !target->bring_up_target ||
        !root.ResolveFile(target->executable_relative_path, &executable) ||
        !re2dj::exe::ReadPeImageInfo(executable, &info, &error) ||
        !re2dj::exe::IsGuestExecutable(info) ||
        info.image_base > (std::numeric_limits<std::uint32_t>::max)())
    {
        std::fprintf(stderr, "{\"error\":\"cannot resolve valid bring-up target\"}\n");
        return 2;
    }
    std::vector<std::uint8_t> file;
    if (verify_iat && !ReadFile(executable, &file, &error))
    {
        std::fprintf(stderr, "{\"error\":\"%s\"}\n", error.c_str());
        return 2;
    }

    std::vector<wchar_t> command(executable.native().begin(), executable.native().end());
    command.push_back(L'\0');
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION child = {};
    const bool debug_process = initial_breakpoint || entry_breakpoint;
    const DWORD creation_flags = CREATE_NO_WINDOW |
                                 (debug_process ? DEBUG_ONLY_THIS_PROCESS : CREATE_SUSPENDED);
    if (CreateProcessW(executable.c_str(),
                       command.data(),
                       nullptr,
                       nullptr,
                       FALSE,
                       creation_flags,
                       nullptr,
                       executable.parent_path().c_str(),
                       &startup,
                       &child) == FALSE)
    {
        std::fprintf(stderr, "{\"error\":\"cannot create stopped original process\"}\n");
        return 3;
    }

    DWORD breakpoint_process_id = 0;
    DWORD breakpoint_thread_id = 0;
    bool breakpoint_reached = !debug_process ||
                              WaitForInitialBreakpoint(&breakpoint_process_id,
                                                       &breakpoint_thread_id,
                                                       &error);
    if (breakpoint_reached && entry_breakpoint)
    {
        const std::uint32_t entry = static_cast<std::uint32_t>(info.image_base) +
                                    info.entry_point_rva;
        breakpoint_reached = SetWow64EntryBreakpoint(child.hThread, entry, &error) &&
                             ContinueDebugEvent(breakpoint_process_id,
                                                breakpoint_thread_id,
                                                DBG_CONTINUE) != FALSE &&
                             WaitForEntryBreakpoint(entry,
                                                    &breakpoint_process_id,
                                                    &breakpoint_thread_id,
                                                    &error);
    }
    std::uintptr_t main_module_base = 0;
    const bool found = breakpoint_reached &&
                       ReadWow64MainImageBase(child.hProcess, &main_module_base, &error);
    re2dj::tools::windows_original_process_probe::IatVerificationResult iat;
    const bool iat_verified = found &&
                              (!verify_iat ||
                               re2dj::tools::windows_original_process_probe::VerifySuspendedIat(
                                   child.hProcess,
                                   main_module_base,
                                   info,
                                   file.data(),
                                   file.size(),
                                   &iat,
                                   &error));
    std::uint32_t runtime_base = 0;
    const bool runtime_loaded = !runtime_path.empty() && iat_verified
                                    ? re2dj::tools::windows_original_process_probe::LoadInjectedRuntime(
                                          child.hProcess,
                                          child.hThread,
                                          breakpoint_process_id,
                                          breakpoint_thread_id,
                                          runtime_path,
                                          &runtime_base,
                                          &error)
                                    : runtime_path.empty();
    TerminateProcess(child.hProcess, 0);
    if (debug_process && breakpoint_reached && runtime_path.empty())
    {
        ContinueDebugEvent(breakpoint_process_id, breakpoint_thread_id, DBG_CONTINUE);
    }
    CloseHandle(child.hThread);
    CloseHandle(child.hProcess);
    if (!breakpoint_reached || !found || !iat_verified || !runtime_loaded)
    {
        std::fprintf(stderr, "{\"error\":\"%s\"}\n", error.c_str());
        return 3;
    }
    const std::uint32_t expected_base = static_cast<std::uint32_t>(info.image_base);
    if (main_module_base != expected_base)
    {
        std::fprintf(stderr,
                     "{\"error\":\"main image base mismatch\",\"expected\":\"0x%08x\",\"actual\":\"0x%08x\"}\n",
                     expected_base,
                     static_cast<unsigned>(main_module_base));
        return 4;
    }
    std::printf("{\"target\":\"%s\",\"image_base\":\"0x%08x\",\"main_module_base\":\"0x%08x\",\"iat_slots\":%u,\"iat_modules\":%u,\"runtime_base\":\"0x%08x\",\"initial_breakpoint\":%s,\"suspended\":%s}\n",
                target->id.c_str(),
                expected_base,
                static_cast<unsigned>(main_module_base),
                verify_iat ? iat.slot_count : 0,
                verify_iat ? static_cast<unsigned>(iat.modules.size()) : 0,
                runtime_base,
                debug_process ? "true" : "false",
                debug_process ? "false" : "true");
    return 0;
}
