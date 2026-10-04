#ifndef RE2DJ_PLATFORM_LINUX_NATIVE_SIGNAL_FAULT_H_
#define RE2DJ_PLATFORM_LINUX_NATIVE_SIGNAL_FAULT_H_

#include <signal.h>

#include "../native/native_guest_fault.h"

namespace re2dj::platform::native
{

// The fault kind a guest signal stands for, as the Linux backends report it.
inline NativeFaultKind NativeFaultKindFromSignal(int signal_number)
{
    switch (signal_number)
    {
    case 0:
        return NativeFaultKind::kNone;
    case SIGSEGV:
    case SIGBUS:
        return NativeFaultKind::kAccessViolation;
    case SIGILL:
        return NativeFaultKind::kIllegalInstruction;
    case SIGTRAP:
        return NativeFaultKind::kBreakpoint;
    case SIGFPE:
        return NativeFaultKind::kDivide;
    default:
        return NativeFaultKind::kOther;
    }
}

}  // namespace re2dj::platform::native

#endif  // RE2DJ_PLATFORM_LINUX_NATIVE_SIGNAL_FAULT_H_
