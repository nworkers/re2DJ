#ifndef RE2DJ_PLATFORM_NATIVE_NATIVE_LEGACY_IO_H_
#define RE2DJ_PLATFORM_NATIVE_NATIVE_LEGACY_IO_H_

#include <cstdint>

#include "native_guest_fault.h"
#include "re2dj/hle/host_input.h"
#include "re2dj/input/io_bindings.h"
#include "re2dj/input/legacy_io_trap.h"

namespace re2dj::platform::native
{

// What the guest did with the I/O board during a run.
struct NativeLegacyIoActivity
{
    std::uint32_t reads = 0;
    std::uint32_t writes = 0;
    // Accesses the policy claimed that the port bus did not answer.
    std::uint32_t unanswered = 0;
    // The first access, for the run's report.
    std::uint16_t first_port = 0;
    bool first_read = false;
};

// Arms the trap for a run: the guest's `in`/`out` faults that the policy
// claims are answered by the EZ2DJ board's port bus, or the EZ2Dancer
// board's for a word-wide policy, starting from its power-on state. A
// disabled policy leaves every such fault a fault. Before every read, as the
// Windows host does, the board's buttons and turntables follow the keys and
// gamepad controls the host holds under the resolved bindings (the built-in
// defaults, or an --io-config INI's over them; task 444); input may be null
// for none.
void SetNativeLegacyIo(const input::LegacyIoTrapPolicy& policy,
                       const hle::HostInputState* host_input,
                       const input::IoBindings& bindings);
void ClearNativeLegacyIo();

// Called from a backend's guest fault handler for the fault an in or out
// raises in user mode (SIGSEGV on Linux, a privileged instruction exception
// on Windows): answers a board access, stepping EIP past it and setting AL
// for a read. False leaves the fault to be reported. Async-signal-safe.
bool HandleNativeLegacyIoTrap(NativeTrapRegisters* registers);

NativeLegacyIoActivity NativeLegacyIoActivitySnapshot();

}  // namespace re2dj::platform::native

#endif  // RE2DJ_PLATFORM_NATIVE_NATIVE_LEGACY_IO_H_
