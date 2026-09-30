#include "re2dj/platform/linux/host_process_launcher.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <limits.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace re2dj::platform::linux
{

std::string EncodeHex(const std::vector<std::uint8_t>& bytes)
{
    constexpr char kDigits[] = "0123456789abcdef";
    std::string text;
    text.reserve(bytes.size() * 2);
    for (const std::uint8_t byte : bytes)
    {
        text.push_back(kDigits[byte >> 4]);
        text.push_back(kDigits[byte & 0x0f]);
    }
    return text;
}

bool DecodeHex(const std::string& text, std::vector<std::uint8_t>* bytes)
{
    const auto digit = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    if (text.size() % 2 != 0)
    {
        return false;
    }
    bytes->clear();
    bytes->reserve(text.size() / 2);
    for (std::size_t index = 0; index < text.size(); index += 2)
    {
        const int high = digit(text[index]);
        const int low = digit(text[index + 1]);
        if (high < 0 || low < 0)
        {
            return false;
        }
        bytes->push_back(static_cast<std::uint8_t>((high << 4) | low));
    }
    return true;
}

LinuxHostProcessLauncher::LinuxHostProcessLauncher(std::vector<std::string> base_arguments)
    : base_arguments_(std::move(base_arguments))
{
}

LinuxHostProcessLauncher::~LinuxHostProcessLauncher()
{
    // A child still running outlives its parent, as a Windows child does.
    for (const Child& child : children_)
    {
        if (child.exit_code_fd >= 0)
        {
            close(child.exit_code_fd);
        }
    }
}

bool LinuxHostProcessLauncher::Start(const hle::ChildProcessRequest& request,
                                     std::uint32_t* child,
                                     std::string* error)
{
    char executable[PATH_MAX] = {};
    const ssize_t length = readlink("/proc/self/exe", executable, sizeof(executable) - 1);
    if (length <= 0)
    {
        *error = std::string("cannot find this program: ") + std::strerror(errno);
        return false;
    }
    int pipe_fds[2] = {-1, -1};
    if (pipe2(pipe_fds, O_CLOEXEC) != 0)
    {
        *error = std::string("cannot make the exit-code pipe: ") + std::strerror(errno);
        return false;
    }
    // Only the write end reaches the child.
    fcntl(pipe_fds[1], F_SETFD, 0);

    std::vector<std::string> arguments = base_arguments_;
    arguments.insert(arguments.end(),
                     {kGuestExecutableOption, request.image_path, kGuestCommandLineOption, request.command_line,
                      kGuestCurrentDirectoryOption, request.current_directory, kGuestExitCodeFdOption,
                      std::to_string(pipe_fds[1])});
    if (!request.startup_reserved.empty())
    {
        arguments.insert(arguments.end(), {kGuestStartupReservedOption, EncodeHex(request.startup_reserved)});
    }
    std::vector<char*> argv;
    argv.reserve(arguments.size() + 1);
    for (std::string& argument : arguments)
    {
        argv.push_back(argument.data());
    }
    argv.push_back(nullptr);

    pid_t pid = -1;
    const int spawned = posix_spawn(&pid, executable, nullptr, nullptr, argv.data(), environ);
    close(pipe_fds[1]);
    if (spawned != 0)
    {
        close(pipe_fds[0]);
        *error = std::string("cannot start the child run: ") + std::strerror(spawned);
        return false;
    }
    Child started;
    started.pid = pid;
    started.exit_code_fd = pipe_fds[0];
    children_.push_back(started);
    *child = static_cast<std::uint32_t>(children_.size() - 1);
    return true;
}

bool LinuxHostProcessLauncher::Poll(std::uint32_t child, bool* finished, std::uint32_t* exit_code)
{
    if (child >= children_.size())
    {
        return false;
    }
    Child& entry = children_[child];
    if (!entry.finished)
    {
        int status = 0;
        const pid_t waited = waitpid(entry.pid, &status, WNOHANG);
        if (waited == entry.pid)
        {
            entry.finished = true;
            // The child's own report, when it wrote one; otherwise what the
            // host exit status says, a signal being a failure.
            std::uint32_t reported = 0;
            if (entry.exit_code_fd >= 0 && read(entry.exit_code_fd, &reported, sizeof(reported)) ==
                                               static_cast<ssize_t>(sizeof(reported)))
            {
                entry.exit_code = reported;
            }
            else
            {
                entry.exit_code = WIFEXITED(status) ? static_cast<std::uint32_t>(WEXITSTATUS(status)) : 0xFFFFFFFFU;
            }
            if (entry.exit_code_fd >= 0)
            {
                close(entry.exit_code_fd);
                entry.exit_code_fd = -1;
            }
        }
        else if (waited < 0)
        {
            entry.finished = true;
            entry.exit_code = 0xFFFFFFFFU;
        }
    }
    *finished = entry.finished;
    *exit_code = entry.exit_code;
    return true;
}

bool WriteGuestExitCode(int fd, std::uint32_t exit_code)
{
    const bool written = write(fd, &exit_code, sizeof(exit_code)) == static_cast<ssize_t>(sizeof(exit_code));
    close(fd);
    return written;
}

}  // namespace re2dj::platform::linux
