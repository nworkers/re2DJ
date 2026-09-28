#ifndef RE2DJ_PLATFORM_LINUX_NATIVE_LEGACY_IO_H_
#define RE2DJ_PLATFORM_LINUX_NATIVE_LEGACY_IO_H_

#include <cstdint>

#include "native_guest_fault.h"
#include "re2dj/hle/host_input.h"
#include "re2dj/input/legacy_io_trap.h"

namespace re2dj::platform::linux
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
// claims are answered by the EZ2DJ board's port bus, starting from its power-on
// state. A disabled policy, or a word-wide one (EZ2Dancer, not modelled on
// Linux yet), leaves every such fault a fault. Before every read, as the
// Windows host does, the board's buttons and turntables follow the keys the
// host holds under the built-in EZ2DJ key map; input may be null for none.
void SetNativeLegacyIo(const input::LegacyIoTrapPolicy& policy, const hle::HostInputState* host_input);
void ClearNativeLegacyIo();

// Called from a width's guest signal handler for a SIGSEGV: answers a board
// access, stepping EIP past it and setting AL for a read. False leaves the
// fault to be reported. Async-signal-safe.
bool HandleNativeLegacyIoTrap(NativeTrapRegisters* registers);

NativeLegacyIoActivity NativeLegacyIoActivitySnapshot();

}  // namespace re2dj::platform::linux

#endif  // RE2DJ_PLATFORM_LINUX_NATIVE_LEGACY_IO_H_
