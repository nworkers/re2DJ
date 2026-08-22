#define NOMINMAX
#include <windows.h>

#include "runtime_loader.h"

#include <limits>
#include <vector>

namespace re2dj::tools::windows_original_process_probe
{

bool LoadInjectedRuntime(HANDLE process,
                         HANDLE primary_thread,
                         DWORD breakpoint_process_id,
                         DWORD breakpoint_thread_id,
                         const std::filesystem::path& runtime_path,
                         std::uint32_t* runtime_base,
                         std::string* error)
{
    if (process == nullptr || primary_thread == nullptr || runtime_base == nullptr ||
        error == nullptr || runtime_path.empty() || SuspendThread(primary_thread) ==
                                                   (std::numeric_limits<DWORD>::max)())
    {
        *error = "cannot suspend original primary thread";
        return false;
    }
    if (ContinueDebugEvent(breakpoint_process_id, breakpoint_thread_id, DBG_CONTINUE) == FALSE)
    {
        *error = "cannot release entry breakpoint after suspending primary thread";
        return false;
    }
    const std::wstring path = runtime_path.native();
    const SIZE_T bytes = (path.size() + 1) * sizeof(wchar_t);
    void* remote_path = VirtualAllocEx(process, nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    SIZE_T copied = 0;
    if (remote_path == nullptr ||
        WriteProcessMemory(process, remote_path, path.c_str(), bytes, &copied) == FALSE ||
        copied != bytes)
    {
        if (remote_path != nullptr) VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
        *error = "cannot write injected runtime path";
        return false;
    }
    const HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    const auto load_library = kernel32 == nullptr ? nullptr : GetProcAddress(kernel32, "LoadLibraryW");
    HANDLE thread = load_library == nullptr ? nullptr : CreateRemoteThread(
        process, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(load_library), remote_path, 0, nullptr);
    if (thread == nullptr)
    {
        VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
        *error = "cannot create injected-runtime loader thread";
        return false;
    }
    for (std::uint32_t wait_count = 0; WaitForSingleObject(thread, 0) == WAIT_TIMEOUT;
         ++wait_count)
    {
        DEBUG_EVENT event = {};
        if (WaitForDebugEvent(&event, 100) == FALSE)
        {
            if (GetLastError() == ERROR_SEM_TIMEOUT && wait_count < 100)
            {
                continue;
            }
            CloseHandle(thread);
            VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
            *error = "cannot service injected-runtime debug event";
            return false;
        }
        if (event.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT && event.u.LoadDll.hFile != nullptr)
        {
            CloseHandle(event.u.LoadDll.hFile);
        }
        if (ContinueDebugEvent(event.dwProcessId, event.dwThreadId, DBG_CONTINUE) == FALSE)
        {
            CloseHandle(thread);
            VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
            *error = "cannot continue injected-runtime debug event";
            return false;
        }
    }
    DWORD exit_code = 0;
    const bool loaded = GetExitCodeThread(thread, &exit_code) != FALSE && exit_code != 0;
    CloseHandle(thread);
    VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
    if (!loaded)
    {
        *error = "injected-runtime LoadLibraryW returned no module base";
        return false;
    }
    *runtime_base = exit_code;
    return true;
}

}  // namespace re2dj::tools::windows_original_process_probe
