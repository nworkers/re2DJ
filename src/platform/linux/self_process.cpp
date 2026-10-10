#include "re2dj/platform/self_process.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace re2dj::platform
{

bool RunSelfAndWait(const std::vector<std::string>& arguments, int* exit_code, std::string* error)
{
    // The running executable itself rather than the path it was started
    // from, which a rebuild may have replaced (task 443).
    return RunExecutableAndWait("/proc/self/exe", arguments, exit_code, error);
}

std::filesystem::path SelfExecutablePath()
{
    std::error_code code;
    std::filesystem::path path = std::filesystem::read_symlink("/proc/self/exe", code);
    return code ? std::filesystem::path() : path;
}

bool ReplaceSelfProcess(const std::filesystem::path& executable, std::string* error)
{
    std::string path = executable.string();
    char* const argv[] = {path.data(), nullptr};
    execve(path.c_str(), argv, environ);
    *error = "cannot start " + path + ": " + std::strerror(errno);
    return false;
}

void ClearEnvironmentVariable(const char* name)
{
    unsetenv(name);
}

bool RunExecutableAndWait(const std::filesystem::path& executable,
                          const std::vector<std::string>& arguments,
                          int* exit_code,
                          std::string* error)
{
    const std::string program = executable.string();
    std::vector<std::string> storage;
    storage.reserve(arguments.size() + 1);
    storage.push_back(program);
    storage.insert(storage.end(), arguments.begin(), arguments.end());
    std::vector<char*> argv;
    argv.reserve(storage.size() + 1);
    for (std::string& argument : storage)
    {
        argv.push_back(argument.data());
    }
    argv.push_back(nullptr);

    pid_t pid = -1;
    const int spawned = posix_spawn(&pid, program.c_str(), nullptr, nullptr, argv.data(), environ);
    if (spawned != 0)
    {
        *error = "cannot start " + program + ": " + std::strerror(spawned);
        return false;
    }
    int status = 0;
    pid_t waited = -1;
    do
    {
        waited = waitpid(pid, &status, 0);
    } while (waited < 0 && errno == EINTR);
    if (waited != pid)
    {
        *error = std::string("cannot wait for the child run: ") + std::strerror(errno);
        return false;
    }
    if (WIFEXITED(status))
    {
        *exit_code = WEXITSTATUS(status);
    }
    else if (WIFSIGNALED(status))
    {
        *exit_code = 128 + WTERMSIG(status);
    }
    else
    {
        *exit_code = -1;
    }
    return true;
}

}  // namespace re2dj::platform
