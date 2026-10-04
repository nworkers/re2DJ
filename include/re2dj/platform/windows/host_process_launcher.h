#ifndef RE2DJ_PLATFORM_WINDOWS_HOST_PROCESS_LAUNCHER_H_
#define RE2DJ_PLATFORM_WINDOWS_HOST_PROCESS_LAUNCHER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "re2dj/hle/guest_child_process.h"
#include "re2dj/platform/native/child_run_options.h"

namespace re2dj::platform::windows
{

// Starts each child guest process as another run of this program, with the
// parent's own options and the child's request after them, as the Linux
// launcher does (task 449). The child writes its exit code to an inherited
// pipe whose handle value it gets through --guest-exit-code-fd, so a child
// run's CLI behaves the same on both hosts.
class WindowsHostProcessLauncher final : public hle::HostProcessLauncher
{
public:
    // base_arguments: this program's arguments without its own child
    // options, which the child's replace.
    explicit WindowsHostProcessLauncher(std::vector<std::string> base_arguments);
    ~WindowsHostProcessLauncher() override;
    WindowsHostProcessLauncher(const WindowsHostProcessLauncher&) = delete;
    WindowsHostProcessLauncher& operator=(const WindowsHostProcessLauncher&) = delete;

    bool Start(const hle::ChildProcessRequest& request, std::uint32_t* child, std::string* error) override;
    bool Poll(std::uint32_t child, bool* finished, std::uint32_t* exit_code) override;

private:
    struct Child
    {
        void* process = nullptr;
        void* exit_code_pipe = nullptr;
        bool finished = false;
        std::uint32_t exit_code = 0;
    };

    std::vector<std::string> base_arguments_;
    std::vector<Child> children_;
};

// Writes a child run's exit code to the pipe its parent gave it; fd is the
// inherited handle's value.
bool WriteGuestExitCode(int fd, std::uint32_t exit_code);

// One argument quoted for a command line as the C runtime splits it again.
std::string QuoteCommandLineArgument(const std::string& argument);

}  // namespace re2dj::platform::windows

#endif  // RE2DJ_PLATFORM_WINDOWS_HOST_PROCESS_LAUNCHER_H_
