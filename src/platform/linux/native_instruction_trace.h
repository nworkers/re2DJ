#ifndef RE2DJ_PLATFORM_LINUX_NATIVE_INSTRUCTION_TRACE_H_
#define RE2DJ_PLATFORM_LINUX_NATIVE_INSTRUCTION_TRACE_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "native_guest_fault.h"

namespace re2dj::platform::linux
{

// Single-step tracing from a breakpoint through the guest image, shared by
// both host widths. Each width's signal handler feeds SIGTRAPs to
// HandleNativeInstructionTraceTrap.
constexpr std::size_t kNativeInstructionTraceMaximumFrames = 128;

struct NativeInstructionTraceFrame
{
    std::uint32_t instruction_pointer = 0;
    std::uint32_t stack_pointer = 0;
    std::uint32_t eax = 0;
    std::uint32_t ebx = 0;
    std::uint32_t ecx = 0;
    std::uint32_t edx = 0;
    std::uint32_t esi = 0;
    std::uint32_t edi = 0;
    std::uint32_t ebp = 0;
    std::uint32_t eflags = 0;
};

struct NativeInstructionTrace
{
    bool armed = false;
    bool started = false;
    bool limit_reached = false;
    std::uint32_t breakpoint = 0;
    std::uint32_t frame_count = 0;
    std::array<NativeInstructionTraceFrame, kNativeInstructionTraceMaximumFrames> frames = {};
};

bool ArmNativeInstructionTrace(NativeInstructionTrace* trace,
                               std::uint32_t breakpoint,
                               std::uint32_t image_base,
                               std::uint32_t image_size,
                               std::string* error);
bool ResumeNativeInstructionTrace(std::uint32_t return_address);
// Stops capturing while keeping the frames collected so far, as the frame
// limit does. A diagnostic that bounds execution with its own INT3 must call
// this first, or the trace consumes that INT3 as a single-step event.
void StopNativeInstructionTrace();
void FinalizeNativeInstructionTrace(NativeInstructionTrace* trace);

// Called from a signal handler for a guest SIGTRAP. Returns true when the trap
// belongs to the trace; the caller then resumes the guest with the updated
// EIP and EFLAGS. Async-signal-safe.
bool HandleNativeInstructionTraceTrap(NativeTrapRegisters* registers);

}  // namespace re2dj::platform::linux

#endif  // RE2DJ_PLATFORM_LINUX_NATIVE_INSTRUCTION_TRACE_H_
