#include "re2dj/platform/self_process.h"

#include <cerrno>
#include <cstring>
#include <spawn.h>
#include <sys/wait.h>

extern char** environ;

namespace re2dj::platform
{

bool RunSelfAndWait(const std::vector<std::string>& arguments, int* exit_code, std::string* error)
{
    // The running executable itself rather than the path it was started
    // from, which a rebuild may have replaced (task 443).
    static constexpr char kExecutable[] = "/proc/self/exe";
    std::vector<std::string> storage;
    storage.reserve(arguments.size() + 1);
    storage.emplace_back(kExecutable);
    storage.insert(storage.end(), arguments.begin(), arguments.end());
    std::vector<char*> argv;
    argv.reserve(storage.size() + 1);
    for (std::string& argument : storage)
    {
        argv.push_back(argument.data());
    }
    argv.push_back(nullptr);

    pid_t pid = -1;
    const int spawned = posix_spawn(&pid, kExecutable, nullptr, nullptr, argv.data(), environ);
    if (spawned != 0)
    {
        *error = std::string("cannot start ") + kExecutable + ": " + std::strerror(spawned);
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
