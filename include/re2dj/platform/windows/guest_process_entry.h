#ifndef RE2DJ_PLATFORM_WINDOWS_GUEST_PROCESS_ENTRY_H_
#define RE2DJ_PLATFORM_WINDOWS_GUEST_PROCESS_ENTRY_H_

namespace re2dj::platform::windows
{

// The Windows product's entry (task 449): makes sure this process holds the
// guest images' range (0x400000 up) from before its loader ran, starting
// itself again suspended to reserve it when it does not, then calls run on a
// thread with a guest-sized stack. Returns run's result, or the restarted
// process's exit code.
int RunGuestReadyProcess(int (*run)(int, char**), int argc, char** argv);

}  // namespace re2dj::platform::windows

#endif  // RE2DJ_PLATFORM_WINDOWS_GUEST_PROCESS_ENTRY_H_
