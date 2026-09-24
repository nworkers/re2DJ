#ifndef RE2DJ_PLATFORM_LINUX_NATIVE_GUEST_FAULT_H_
#define RE2DJ_PLATFORM_LINUX_NATIVE_GUEST_FAULT_H_

#include <cstdint>

namespace re2dj::platform::linux
{

// A signal raised by guest code, reduced to the 32-bit guest register view.
// Both host widths report faults in this form.
struct NativeGuestFault
{
    std::uint32_t status_code = 0;
    std::uint32_t instruction_pointer = 0;
    std::uint32_t stack_pointer = 0;
    std::uint32_t fault_address = 0;
    std::uint32_t signal_code = 0;
    std::uint32_t cpu_error_code = 0;
    std::uint32_t eax = 0;
    std::uint32_t ebx = 0;
    std::uint32_t ecx = 0;
    std::uint32_t edx = 0;
    std::uint32_t esi = 0;
    std::uint32_t edi = 0;
    std::uint32_t ebp = 0;
    std::uint32_t eflags = 0;
};

// The 32-bit guest register view of an interrupted guest, as each width's
// signal handler reads it from and writes it back to ucontext. Shared
// trap logic (instruction trace, SEH dispatch) works on this form only.
struct NativeTrapRegisters
{
    std::uint32_t eip = 0;
    std::uint32_t esp = 0;
    std::uint32_t eax = 0;
    std::uint32_t ebx = 0;
    std::uint32_t ecx = 0;
    std::uint32_t edx = 0;
    std::uint32_t esi = 0;
    std::uint32_t edi = 0;
    std::uint32_t ebp = 0;
    std::uint32_t eflags = 0;
    std::uint32_t cs = 0;
    std::uint32_t ss = 0;
    std::uint32_t ds = 0;
    std::uint32_t es = 0;
    std::uint32_t fs = 0;
    std::uint32_t gs = 0;
};

}  // namespace re2dj::platform::linux

#endif  // RE2DJ_PLATFORM_LINUX_NATIVE_GUEST_FAULT_H_
