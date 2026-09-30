#ifndef RE2DJ_HLE_GUEST_CHILD_PROCESS_H_
#define RE2DJ_HLE_GUEST_CHILD_PROCESS_H_

#include <cstdint>
#include <string>
#include <vector>

// Guest processes that start other guest processes, as a launcher does with
// CreateProcessA. Each guest process loads its image at the same base, so a
// child cannot share its parent's address space: the host starts it as a
// host process of its own, and the parent's HLE only waits on it and reads
// its exit code.
namespace re2dj::hle
{

// What CreateProcessA asks the host to start.
struct ChildProcessRequest
{
    // The executable's path in the image, for example "EZ2DJ/EZ2DJ6th.EXE".
    std::string image_path;
    // The command line as the parent passed it, which is what the child's
    // GetCommandLineA returns.
    std::string command_line;
    // The child's first current directory, a guest path.
    std::string current_directory;
    // STARTUPINFO's cbReserved2 bytes from lpReserved2, which the child's
    // GetStartupInfoA hands back.
    std::vector<std::uint8_t> startup_reserved;
};

// How a guest process started, as its launcher gave it: what its
// GetCommandLineA and GetStartupInfoA return. A process nothing launched has
// an empty command line (GetCommandLineA then gives the quoted module path)
// and no reserved bytes.
struct GuestStartup
{
    std::string command_line;
    std::vector<std::uint8_t> reserved;
};

// The host side of child processes.
class HostProcessLauncher
{
public:
    virtual ~HostProcessLauncher() = default;

    // Starts the child; false with error when the host cannot.
    virtual bool Start(const ChildProcessRequest& request, std::uint32_t* child, std::string* error) = 0;
    // Whether the child has ended, and then its 32-bit exit code.
    virtual bool Poll(std::uint32_t child, bool* finished, std::uint32_t* exit_code) = 0;
};

}  // namespace re2dj::hle

#endif  // RE2DJ_HLE_GUEST_CHILD_PROCESS_H_
