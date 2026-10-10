#define NOMINMAX
#include <windows.h>

#include "re2dj/platform/self_process.h"

#include <cstdlib>

#include "re2dj/platform/windows/host_process_launcher.h"

namespace re2dj::platform
{

bool RunSelfAndWait(const std::vector<std::string>& arguments, int* exit_code, std::string* error)
{
    const std::filesystem::path executable = SelfExecutablePath();
    if (executable.empty())
    {
        *error = "cannot find this program's path (error " + std::to_string(GetLastError()) + ")";
        return false;
    }
    return RunExecutableAndWait(executable, arguments, exit_code, error);
}

std::filesystem::path SelfExecutablePath()
{
    char executable[MAX_PATH];
    const DWORD length = GetModuleFileNameA(nullptr, executable, static_cast<DWORD>(sizeof(executable)));
    if (length == 0 || length >= sizeof(executable))
    {
        return std::filesystem::path();
    }
    return std::filesystem::path(executable);
}

bool ReplaceSelfProcess(const std::filesystem::path&, std::string* error)
{
    *error = "Windows cannot replace a running process";
    return false;
}

void ClearEnvironmentVariable(const char* name)
{
    // The C runtime's copy and the process block, which CreateProcessA hands on.
    _putenv_s(name, "");
    SetEnvironmentVariableA(name, nullptr);
}

bool RunExecutableAndWait(const std::filesystem::path& executable_path,
                          const std::vector<std::string>& arguments,
                          int* exit_code,
                          std::string* error)
{
    const std::string executable = executable_path.string();
    std::string command_line = windows::QuoteCommandLineArgument(executable);
    for (const std::string& argument : arguments)
    {
        command_line += ' ';
        command_line += windows::QuoteCommandLineArgument(argument);
    }

    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessA(executable.c_str(), command_line.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup,
                        &process))
    {
        *error = "cannot start " + executable + " (error " + std::to_string(GetLastError()) + ")";
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
