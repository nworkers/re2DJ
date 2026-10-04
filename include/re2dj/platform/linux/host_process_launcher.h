#ifndef RE2DJ_PLATFORM_LINUX_HOST_PROCESS_LAUNCHER_H_
#define RE2DJ_PLATFORM_LINUX_HOST_PROCESS_LAUNCHER_H_

#include <cstdint>
#include <string>
#include <sys/types.h>
#include <vector>

#include "re2dj/hle/guest_child_process.h"
#include "re2dj/platform/native/child_run_options.h"

namespace re2dj::platform::linux
{

// Starts each child guest process as another run of this program, with the
// parent's own options and the child's request after them. The child writes
// its exit code to a pipe, since a host exit status holds only 8 bits.
class LinuxHostProcessLauncher final : public hle::HostProcessLauncher
{
public:
    // base_arguments: this program's arguments without its own child
    // options, which the child's replace.
    explicit LinuxHostProcessLauncher(std::vector<std::string> base_arguments);
    ~LinuxHostProcessLauncher() override;
    LinuxHostProcessLauncher(const LinuxHostProcessLauncher&) = delete;
    LinuxHostProcessLauncher& operator=(const LinuxHostProcessLauncher&) = delete;

    bool Start(const hle::ChildProcessRequest& request, std::uint32_t* child, std::string* error) override;
    bool Poll(std::uint32_t child, bool* finished, std::uint32_t* exit_code) override;

private:
    struct Child
    {
        pid_t pid = -1;
        int exit_code_fd = -1;
        bool finished = false;
        std::uint32_t exit_code = 0;
    };

    std::vector<std::string> base_arguments_;
    std::vector<Child> children_;
};

// Writes a child run's exit code to the pipe its parent gave it.
bool WriteGuestExitCode(int fd, std::uint32_t exit_code);

}  // namespace re2dj::platform::linux

#endif  // RE2DJ_PLATFORM_LINUX_HOST_PROCESS_LAUNCHER_H_
