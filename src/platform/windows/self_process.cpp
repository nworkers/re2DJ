#define NOMINMAX
#include <windows.h>

#include "re2dj/platform/self_process.h"

#include "re2dj/platform/windows/host_process_launcher.h"

namespace re2dj::platform
{

bool RunSelfAndWait(const std::vector<std::string>& arguments, int* exit_code, std::string* error)
{
    char executable[MAX_PATH];
    const DWORD length = GetModuleFileNameA(nullptr, executable, static_cast<DWORD>(sizeof(executable)));
    if (length == 0 || length >= sizeof(executable))
    {
        *error = "cannot find this program's path (error " + std::to_string(GetLastError()) + ")";
        return false;
    }
    std::string command_line = windows::QuoteCommandLineArgument(executable);
    for (const std::string& argument : arguments)
    {
        command_line += ' ';
        command_line += windows::QuoteCommandLineArgument(argument);
    }

    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessA(executable, command_line.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup,
                        &process))
    {
        *error = "cannot start " + std::string(executable) + " (error " + std::to_string(GetLastError()) + ")";
        return false;
    }
    CloseHandle(process.hThread);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 0;
    const BOOL got = GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hProcess);
    if (!got)
    {
        *error = "cannot read the child run's exit code (error " + std::to_string(GetLastError()) + ")";
        return false;
    }
    *exit_code = static_cast<int>(code);
    return true;
}

}  // namespace re2dj::platform
