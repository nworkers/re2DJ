#ifndef RE2DJ_PLATFORM_NATIVE_CHILD_RUN_OPTIONS_H_
#define RE2DJ_PLATFORM_NATIVE_CHILD_RUN_OPTIONS_H_

// The host options a child run takes, by which a parent run hands a child
// its request: the child's executable in the image, its command line, first
// current directory and STARTUPINFO reserved bytes (as hex), and where it
// writes its 32-bit exit code (a pipe's fd on Linux, an inherited pipe
// handle's value on Windows). Both OS launchers and the CLI use them.
namespace re2dj::platform::native
{

inline constexpr const char* kGuestExecutableOption = "--guest-executable";
inline constexpr const char* kGuestCommandLineOption = "--guest-command-line";
inline constexpr const char* kGuestCurrentDirectoryOption = "--guest-current-directory";
inline constexpr const char* kGuestStartupReservedOption = "--guest-startup-reserved";
inline constexpr const char* kGuestExitCodeFdOption = "--guest-exit-code-fd";

}  // namespace re2dj::platform::native

#endif  // RE2DJ_PLATFORM_NATIVE_CHILD_RUN_OPTIONS_H_
