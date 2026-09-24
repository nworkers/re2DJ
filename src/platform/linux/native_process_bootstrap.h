#ifndef RE2DJ_PLATFORM_LINUX_NATIVE_PROCESS_BOOTSTRAP_H_
#define RE2DJ_PLATFORM_LINUX_NATIVE_PROCESS_BOOTSTRAP_H_

#include <cstdint>
#include <string>

#include "native_guest_fault.h"

namespace re2dj::platform::linux
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
    std::uint32_t SehDispatchCount() const;
    std::uint32_t LastSehHandler() const;
    std::uint32_t LastSehResumedEip() const;
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
// running guest code through a NativeProcessBootstrap.
[[noreturn]] void ExitNativeGuestProcess(std::uint32_t exit_code);

}  // namespace re2dj::platform::linux

#endif  // RE2DJ_PLATFORM_LINUX_NATIVE_PROCESS_BOOTSTRAP_H_
