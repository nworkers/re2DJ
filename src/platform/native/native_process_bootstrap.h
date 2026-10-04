#ifndef RE2DJ_PLATFORM_NATIVE_NATIVE_PROCESS_BOOTSTRAP_H_
#define RE2DJ_PLATFORM_NATIVE_NATIVE_PROCESS_BOOTSTRAP_H_

#include <cstdint>
#include <string>

#include "native_guest_fault.h"
#include "native_guest_seh.h"

namespace re2dj::platform::native
{

// The guest stack, TEB/PEB, FS selector, and fault handling around guest
// execution. x86/native_process_bootstrap.cpp runs the guest directly on an
// i386 host; x64/native_process_bootstrap.cpp runs it in compatibility mode.
class NativeProcessBootstrap
{
public:
    NativeProcessBootstrap();
    ~NativeProcessBootstrap();

    NativeProcessBootstrap(const NativeProcessBootstrap&) = delete;
    NativeProcessBootstrap& operator=(const NativeProcessBootstrap&) = delete;

    bool Initialize(std::uint32_t image_base, std::string* error);
    bool RunTlsCallback(std::uint32_t callback,
                        std::uint32_t image_base,
                        NativeGuestFault* fault,
                        std::string* error);
    bool RunEntry(std::uint32_t entry,
                  std::uint32_t* result,
                  NativeGuestFault* fault,
                  std::string* error);
    std::uint32_t GuestStackBase() const;
    std::uint32_t GuestStackLimit() const;
    bool IsGuestStackRange(std::uint32_t address, std::uint32_t size) const;
    std::uint32_t Teb() const;
    // Guest SEH handlers that resumed with ExceptionContinueExecution, the
    // last of them, and where it resumed.
    std::uint32_t SehDispatchCount() const;
    std::uint32_t LastSehHandler() const;
    std::uint32_t LastSehResumedEip() const;
    // Every exception delivered to the guest's SEH chain.
    NativeGuestExceptionCounters ExceptionCounters() const;
    // Whether an import such as ExitProcess ended the guest process. The run
    // that ended it returned as a normal completion.
    bool GuestProcessExited() const;
    std::uint32_t GuestExitCode() const;

    struct Impl;

private:
    Impl* impl_ = nullptr;
};

// Ends the guest process from inside an import handler without returning to
// the guest: the current RunTlsCallback or RunEntry returns true, and
// GuestProcessExited() reports exit_code. Only valid while this thread is
// running guest code through a NativeProcessBootstrap. From a thread other
// than the main one, the main thread's run ends that way instead.
[[noreturn]] void ExitNativeGuestProcess(std::uint32_t exit_code);

// A guest thread's end, reported on its own host thread while it holds the
// guest lock (native_guest_threads.h), with the token it was started with.
using NativeGuestThreadExit = void (*)(void* context, std::uint32_t token, std::uint32_t exit_code);

struct NativeGuestThreadStart
{
    // The ThreadProc, called stdcall with parameter.
    std::uint32_t start = 0;
    std::uint32_t parameter = 0;
    std::uint32_t token = 0;
    NativeGuestThreadExit on_exit = nullptr;
    void* exit_context = nullptr;
};

// Creates a guest thread in the process the calling import handler runs for:
// a guest stack and TEB of its own (sharing the PEB), ready when this returns
// with *teb, and a host thread that runs start(parameter) once it gets the
// guest lock. Returning from the ThreadProc ends the thread through on_exit;
// ExitProcess, or a fault or stop no handler takes, ends the process.
bool StartNativeGuestThread(const NativeGuestThreadStart& start, std::uint32_t* teb, std::string* error);

}  // namespace re2dj::platform::native

#endif  // RE2DJ_PLATFORM_NATIVE_NATIVE_PROCESS_BOOTSTRAP_H_
