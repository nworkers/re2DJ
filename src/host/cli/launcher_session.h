#ifndef RE2DJ_HOST_CLI_LAUNCHER_SESSION_H_
#define RE2DJ_HOST_CLI_LAUNCHER_SESSION_H_

namespace re2dj::host
{

// Whether a run with `argc` arguments (the program name counted) opens the
// launcher (#12): only a run without arguments, and not when
// RE2DJ_LAUNCHER=0 asks for the bare usage text, as automation may.
bool LauncherRequested(int argc, const char* launcher_variable);

struct LauncherSessionResult
{
    // False when no launcher window could be opened at all; the caller then
    // behaves as it did without a launcher.
    bool ran = false;
    int exit_code = 0;
};

// The launcher's loop: read cfg/re2dj.ini, list the built-in profiles under
// the current directory, show the window, save what changed, run the chosen
// profile as a new process of this program and come back when it ends, until
// the user quits.
LauncherSessionResult RunLauncherSession();

}  // namespace re2dj::host

#endif  // RE2DJ_HOST_CLI_LAUNCHER_SESSION_H_
