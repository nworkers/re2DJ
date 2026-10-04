#define NOMINMAX
#include <windows.h>

#include "../native_guest_reservation.h"
#include "re2dj/platform/windows/guest_process_entry.h"

#include <atomic>
#include <string>

// The guest images' range on Windows (task 449). EZ2DJ executables carry no
// relocations and must sit at their own base, 0x400000. Windows places
// allocations bottom-up, and the loader maps its own data (NLS tables and
// the like) into the first hole big enough before any code of this program
// runs; with this program moved off 0x400000, that hole is the guest's.
// So this program starts itself again suspended, reserves the range in the
// new process before its loader runs, and lets it go; the new process finds
// the range held and releases it when the first guest image is mapped.
namespace
{

constexpr std::uintptr_t kGuestRangeBase = 0x00400000;
constexpr std::size_t kGuestRangeSize = 0x04000000;
// Set for the process started with the range reserved; it clears it at once
// so its own children (a launcher's) are started the same way.
constexpr const char* kReservedEnvironment = "RE2DJ_GUEST_RANGE_RESERVED";
constexpr SIZE_T kGuestThreadStack = 16 * 1024 * 1024;

std::atomic<bool> g_reserved{false};

// Whether the range is held as the parent reserved it: one reservation with
// nothing committed, starting at the base.
bool RangeHeldAtStart()
{
    MEMORY_BASIC_INFORMATION info = {};
    return VirtualQuery(reinterpret_cast<void*>(kGuestRangeBase), &info, sizeof(info)) == sizeof(info) &&
           info.State == MEM_RESERVE &&
           reinterpret_cast<std::uintptr_t>(info.AllocationBase) == kGuestRangeBase &&
           info.RegionSize >= kGuestRangeSize;
}

struct MainCall
{
    int (*run)(int, char**) = nullptr;
    int argc = 0;
    char** argv = nullptr;
    int result = 0;
};

DWORD WINAPI RunMainCall(void* parameter)
{
    auto* call = static_cast<MainCall*>(parameter);
    call->result = call->run(call->argc, call->argv);
    return 0;
}

// Runs the program on a thread with a guest-sized stack: the guest runs on
// the stack of the thread executing it (design 448).
int RunOnGuestThread(int (*run)(int, char**), int argc, char** argv)
{
    MainCall call{run, argc, argv, 0};
    HANDLE thread = CreateThread(nullptr, kGuestThreadStack, &RunMainCall, &call, STACK_SIZE_PARAM_IS_A_RESERVATION,
                                 nullptr);
    if (thread == nullptr)
    {
        return run(argc, argv);
    }
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    return call.result;
}

// Starts this program again, suspended, with the guest range reserved, and
// returns its exit code; -1 when it cannot.
int RelaunchWithReservation()
{
    char executable[MAX_PATH * 4] = {};
    const DWORD length = GetModuleFileNameA(nullptr, executable, static_cast<DWORD>(sizeof(executable)));
    if (length == 0 || length >= sizeof(executable))
    {
        return -1;
    }
    std::string command_line = GetCommandLineA();
    // The new process inherits this variable; this one goes on without it.
    SetEnvironmentVariableA(kReservedEnvironment, "1");
    STARTUPINFOA startup = {};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION process = {};
    const BOOL created = CreateProcessA(executable, command_line.data(), nullptr, nullptr, TRUE, CREATE_SUSPENDED,
                                        nullptr, nullptr, &startup, &process);
    SetEnvironmentVariableA(kReservedEnvironment, nullptr);
    if (!created)
    {
        return -1;
    }
    // Before its loader runs only the executable and ntdll are mapped, so the
    // range is free.
    VirtualAllocEx(process.hProcess, reinterpret_cast<void*>(kGuestRangeBase), kGuestRangeSize, MEM_RESERVE,
                   PAGE_NOACCESS);
    // The two live and die together; console interrupts reach both, and the
    // new one decides.
    HANDLE job = CreateJobObjectA(nullptr, nullptr);
    if (job != nullptr)
    {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_BREAKAWAY_OK;
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
        AssignProcessToJobObject(job, process.hProcess);
    }
    SetConsoleCtrlHandler(nullptr, TRUE);
    ResumeThread(process.hThread);
    CloseHandle(process.hThread);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exit_code = 1;
    GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hProcess);
    if (job != nullptr)
    {
        CloseHandle(job);
    }
    return static_cast<int>(exit_code);
}

}  // namespace

namespace re2dj::platform::native
{

bool ReleaseGuestImageReservation(std::uintptr_t address, std::size_t size)
{
    if (!g_reserved.load() || address < kGuestRangeBase || size > kGuestRangeSize ||
        address - kGuestRangeBase > kGuestRangeSize - size)
    {
        return false;
    }
    bool expected = true;
    if (!g_reserved.compare_exchange_strong(expected, false))
    {
        return false;
    }
    VirtualFree(reinterpret_cast<void*>(kGuestRangeBase), 0, MEM_RELEASE);
    return true;
}

bool GuestImageRangeReserved()
{
    return g_reserved.load();
}

}  // namespace re2dj::platform::native

namespace re2dj::platform::windows
{

int RunGuestReadyProcess(int (*run)(int, char**), int argc, char** argv)
{
    char flag[4] = {};
    const bool reserved_start = GetEnvironmentVariableA(kReservedEnvironment, flag, sizeof(flag)) != 0;
    if (reserved_start)
    {
        SetEnvironmentVariableA(kReservedEnvironment, nullptr);
        g_reserved.store(RangeHeldAtStart());
        return RunOnGuestThread(run, argc, argv);
    }
    const int relaunched = RelaunchWithReservation();
    if (relaunched >= 0)
    {
        return relaunched;
    }
    // Without the relaunch the range may still happen to be free.
    return RunOnGuestThread(run, argc, argv);
}

}  // namespace re2dj::platform::windows
