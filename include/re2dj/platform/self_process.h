#ifndef RE2DJ_PLATFORM_SELF_PROCESS_H_
#define RE2DJ_PLATFORM_SELF_PROCESS_H_

#include <filesystem>
#include <string>
#include <vector>

namespace re2dj::platform
{

// Runs this program's own executable again with `arguments` (after the program
// name) and waits for it to end. The child inherits the environment, the
// current directory and the console, so its output lands where the parent's
// does. The launcher (#12) starts each game this way, in a process whose
// address space no GPU driver has touched yet. False, with `error` set, when
// the child could not be started; otherwise `exit_code` is its exit code (on
// Linux 128 plus the signal number when a signal ended it).
bool RunSelfAndWait(const std::vector<std::string>& arguments, int* exit_code, std::string* error);

// This program's own executable as an absolute path, or empty when the host
// cannot say. The launcher reads it before an update renames the file (#17).
std::filesystem::path SelfExecutablePath();

// Runs `executable` with `arguments` and waits, as RunSelfAndWait does for this
// program; the launcher starts a freshly installed one this way on Windows.
bool RunExecutableAndWait(const std::filesystem::path& executable,
                          const std::vector<std::string>& arguments,
                          int* exit_code,
                          std::string* error);

// Replaces this process with `executable`, started with no arguments, keeping
// the process id, which lets a game launcher such as Steam keep tracking it
// across an update (#17). Returns only on failure, with `error` set. Linux
// only; Windows has no such call and returns false at once, and its caller
// runs the new executable as a child instead.
bool ReplaceSelfProcess(const std::filesystem::path& executable, std::string* error);

// Removes a variable from this process's environment, which the processes it
// starts inherit; the launcher withdraws its update-test override this way
// before starting the updated program.
void ClearEnvironmentVariable(const char* name);

}  // namespace re2dj::platform

#endif  // RE2DJ_PLATFORM_SELF_PROCESS_H_
