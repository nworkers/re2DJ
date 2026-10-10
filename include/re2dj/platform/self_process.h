#ifndef RE2DJ_PLATFORM_SELF_PROCESS_H_
#define RE2DJ_PLATFORM_SELF_PROCESS_H_

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

}  // namespace re2dj::platform

#endif  // RE2DJ_PLATFORM_SELF_PROCESS_H_
