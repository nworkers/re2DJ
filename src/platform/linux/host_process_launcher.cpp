#include "re2dj/platform/linux/host_process_launcher.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include "re2dj/hle/hex_bytes.h"

extern char** environ;

namespace re2dj::platform::linux
{

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
    // The running executable itself rather than the path it was started
    // from: a rebuild replaces that file while the launcher waits, and its
    // path then names nothing (task 443). Parent and child stay one build.
    static constexpr char kExecutable[] = "/proc/self/exe";
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
                     {native::kGuestExecutableOption, request.image_path, native::kGuestCommandLineOption, request.command_line,
                      native::kGuestCurrentDirectoryOption, request.current_directory, native::kGuestExitCodeFdOption,
                      std::to_string(pipe_fds[1])});
    if (!request.startup_reserved.empty())
    {
        arguments.insert(arguments.end(),
                         {native::kGuestStartupReservedOption, hle::EncodeHexBytes(request.startup_reserved)});
    }
    std::vector<char*> argv;
    argv.reserve(arguments.size() + 1);
    for (std::string& argument : arguments)
    {
        argv.push_back(argument.data());
    }
    argv.push_back(nullptr);

    pid_t pid = -1;
    const int spawned = posix_spawn(&pid, kExecutable, nullptr, nullptr, argv.data(), environ);
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
