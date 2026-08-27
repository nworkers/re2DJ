#include "re2dj/platform/windows/original_process_backend.h"

int main(int argc, char** argv)
{
    return re2dj::platform::windows::RunOriginalProcessLauncherCommand(argc, argv);
}
