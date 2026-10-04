#define NOMINMAX
#include <windows.h>

#include "re2dj/platform/windows/host_process_launcher.h"

#include <cstdint>

#include "re2dj/hle/hex_bytes.h"

namespace re2dj::platform::windows
{
namespace
{

std::string LastErrorText()
{
    return std::to_string(static_cast<unsigned long>(GetLastError()));
}

}  // namespace

std::string QuoteCommandLineArgument(const std::string& argument)
{
    if (!argument.empty() && argument.find_first_of(" \t\n\v\"") == std::string::npos)
    {
        return argument;
    }
    // Backslashes count only before a quote, where they are doubled, and the
    // quote itself is escaped; a run of them before the closing quote too.
    std::string quoted = "\"";
    std::size_t backslashes = 0;
    for (const char character : argument)
    {
        if (character == '\\')
        {
            ++backslashes;
            continue;
        }
        if (character == '"')
        {
            quoted.append(backslashes * 2 + 1, '\\');
        }
        else
        {
            quoted.append(backslashes, '\\');
        }
        backslashes = 0;
        quoted.push_back(character);
    }
    quoted.append(backslashes * 2, '\\');
    quoted.push_back('"');
    return quoted;
}

WindowsHostProcessLauncher::WindowsHostProcessLauncher(std::vector<std::string> base_arguments)
    : base_arguments_(std::move(base_arguments))
{
}

WindowsHostProcessLauncher::~WindowsHostProcessLauncher()
{
    // A child still running outlives its parent, as a Windows child does.
    for (const Child& child : children_)
    {
        if (child.exit_code_pipe != nullptr) CloseHandle(child.exit_code_pipe);
        if (child.process != nullptr) CloseHandle(child.process);
    }
}

bool WindowsHostProcessLauncher::Start(const hle::ChildProcessRequest& request, std::uint32_t* child, std::string* error)
{
    // The running executable itself, so parent and child are one build.
    char executable[MAX_PATH * 4] = {};
    const DWORD length = GetModuleFileNameA(nullptr, executable, static_cast<DWORD>(sizeof(executable)));
    if (length == 0 || length >= sizeof(executable))
    {
        *error = "cannot find the running executable: " + LastErrorText();
        return false;
    }
    SECURITY_ATTRIBUTES inherit = {sizeof(inherit), nullptr, TRUE};
    HANDLE read_end = nullptr;
    HANDLE write_end = nullptr;
    if (!CreatePipe(&read_end, &write_end, &inherit, 0))
    {
        *error = "cannot make the exit-code pipe: " + LastErrorText();
        return false;
    }
    // Only the write end reaches the child.
    SetHandleInformation(read_end, HANDLE_FLAG_INHERIT, 0);
    std::vector<std::string> arguments = base_arguments_;
    if (!arguments.empty())
    {
        arguments.front() = executable;
    }
    arguments.insert(arguments.end(),
                     {native::kGuestExecutableOption, request.image_path, native::kGuestCommandLineOption,
                      request.command_line, native::kGuestCurrentDirectoryOption, request.current_directory,
                      native::kGuestExitCodeFdOption,
                      std::to_string(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(write_end)))});
    if (!request.startup_reserved.empty())
    {
        arguments.insert(arguments.end(),
                         {native::kGuestStartupReservedOption, hle::EncodeHexBytes(request.startup_reserved)});
    }
    std::string command_line;
    for (const std::string& argument : arguments)
    {
        if (!command_line.empty()) command_line.push_back(' ');
        command_line += QuoteCommandLineArgument(argument);
    }
    STARTUPINFOA startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    const BOOL created = CreateProcessA(executable, command_line.data(), nullptr, nullptr, TRUE, 0, nullptr,
                                        nullptr, &startup, &process);
    CloseHandle(write_end);
    if (!created)
    {
        CloseHandle(read_end);
        *error = "cannot start the child run: " + LastErrorText();
        return false;
    }
    CloseHandle(process.hThread);
    Child started;
    started.process = process.hProcess;
    started.exit_code_pipe = read_end;
    children_.push_back(started);
    *child = static_cast<std::uint32_t>(children_.size() - 1);
    return true;
}

bool WindowsHostProcessLauncher::Poll(std::uint32_t child, bool* finished, std::uint32_t* exit_code)
{
    if (child >= children_.size())
    {
        return false;
    }
    Child& entry = children_[child];
    if (!entry.finished && WaitForSingleObject(entry.process, 0) == WAIT_OBJECT_0)
    {
        entry.finished = true;
        // The child's own report, when it wrote one; otherwise its process
        // exit code.
        std::uint32_t reported = 0;
        DWORD read = 0;
        if (entry.exit_code_pipe != nullptr &&
            ReadFile(entry.exit_code_pipe, &reported, sizeof(reported), &read, nullptr) && read == sizeof(reported))
        {
            entry.exit_code = reported;
        }
        else
        {
            DWORD status = 0xFFFFFFFFU;
            GetExitCodeProcess(entry.process, &status);
            entry.exit_code = status;
        }
        if (entry.exit_code_pipe != nullptr)
        {
            CloseHandle(entry.exit_code_pipe);
            entry.exit_code_pipe = nullptr;
        }
    }
    *finished = entry.finished;
    *exit_code = entry.exit_code;
    return true;
}

bool WriteGuestExitCode(int fd, std::uint32_t exit_code)
{
    HANDLE pipe = reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(fd)));
    DWORD written = 0;
    const bool ok = WriteFile(pipe, &exit_code, sizeof(exit_code), &written, nullptr) && written == sizeof(exit_code);
    CloseHandle(pipe);
    return ok;
}

}  // namespace re2dj::platform::windows
