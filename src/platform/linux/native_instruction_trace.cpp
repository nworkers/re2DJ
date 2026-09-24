#include "native_instruction_trace.h"

#include <cstring>

namespace re2dj::platform::linux
{
namespace
{

constexpr std::uint32_t kTrapFlag = 0x100;

// Signal-handler state. Aligned 32-bit loads and stores are single
// instructions on both host widths, so volatile is enough.
struct RawInstructionTraceFrame
{
    volatile std::uint32_t instruction_pointer = 0;
    volatile std::uint32_t stack_pointer = 0;
    volatile std::uint32_t eax = 0;
    volatile std::uint32_t ebx = 0;
    volatile std::uint32_t ecx = 0;
    volatile std::uint32_t edx = 0;
    volatile std::uint32_t esi = 0;
    volatile std::uint32_t edi = 0;
    volatile std::uint32_t ebp = 0;
    volatile std::uint32_t eflags = 0;
};

volatile std::uint32_t g_trace_armed = 0;
volatile std::uint32_t g_trace_started = 0;
volatile std::uint32_t g_trace_paused = 0;
volatile std::uint32_t g_trace_breakpoint_pending = 0;
volatile std::uint32_t g_trace_limit_reached = 0;
volatile std::uint32_t g_trace_breakpoint = 0;
volatile std::uint32_t g_trace_original_byte = 0;
volatile std::uint32_t g_trace_image_base = 0;
volatile std::uint32_t g_trace_image_size = 0;
volatile std::uint32_t g_trace_frame_count = 0;
std::array<RawInstructionTraceFrame, kNativeInstructionTraceMaximumFrames> g_trace_frames = {};

std::uint8_t* GuestByte(std::uint32_t address)
{
    return reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(address));
}

void CaptureInstructionTraceFrame(const NativeTrapRegisters& registers)
{
    const std::uint32_t frame_index = g_trace_frame_count;
    if (frame_index >= g_trace_frames.size())
    {
        return;
    }
    RawInstructionTraceFrame& frame = g_trace_frames[frame_index];
    frame.instruction_pointer = registers.eip;
    frame.stack_pointer = registers.esp;
    frame.eax = registers.eax;
    frame.ebx = registers.ebx;
    frame.ecx = registers.ecx;
    frame.edx = registers.edx;
    frame.esi = registers.esi;
    frame.edi = registers.edi;
    frame.ebp = registers.ebp;
    frame.eflags = registers.eflags;
    g_trace_frame_count = frame_index + 1;
}

bool IsTraceGuestInstructionAddress(std::uint32_t address)
{
    return address >= g_trace_image_base && address - g_trace_image_base < g_trace_image_size;
}

}  // namespace

bool HandleNativeInstructionTraceTrap(NativeTrapRegisters* registers)
{
    if (registers == nullptr || g_trace_armed == 0)
    {
        return false;
    }
    if (g_trace_breakpoint_pending != 0 && registers->eip == g_trace_breakpoint + 1)
    {
        *GuestByte(g_trace_breakpoint) = static_cast<std::uint8_t>(g_trace_original_byte);
        g_trace_started = 1;
        g_trace_paused = 0;
        g_trace_breakpoint_pending = 0;
        registers->eip = g_trace_breakpoint;
        registers->eflags |= kTrapFlag;
        CaptureInstructionTraceFrame(*registers);
        return true;
    }
    if (g_trace_started != 0 && g_trace_paused == 0)
    {
        CaptureInstructionTraceFrame(*registers);
        if (!IsTraceGuestInstructionAddress(registers->eip))
        {
            // Leaving the image (an import thunk): stop stepping until the
            // bridge re-arms the trace at the caller's return address.
            g_trace_paused = 1;
            registers->eflags &= ~kTrapFlag;
            return true;
        }
        if (g_trace_frame_count >= g_trace_frames.size())
        {
            g_trace_limit_reached = 1;
            g_trace_armed = 0;
            registers->eflags &= ~kTrapFlag;
        }
        return true;
    }
    return false;
}

bool ArmNativeInstructionTrace(NativeInstructionTrace* trace,
                               std::uint32_t breakpoint,
                               std::uint32_t image_base,
                               std::uint32_t image_size,
                               std::string* error)
{
    if (trace == nullptr || error == nullptr || breakpoint == 0 || image_base == 0 ||
        image_size == 0 || breakpoint < image_base || breakpoint - image_base >= image_size ||
        g_trace_armed != 0)
    {
        if (error != nullptr)
        {
            *error = "invalid native instruction trace arguments";
        }
        return false;
    }
    *trace = {};
    std::uint8_t* byte = GuestByte(breakpoint);
    const std::uint8_t original_byte = *byte;
    *byte = 0xCC;
    g_trace_breakpoint = breakpoint;
    g_trace_original_byte = original_byte;
    g_trace_started = 0;
    g_trace_paused = 0;
    g_trace_breakpoint_pending = 1;
    g_trace_limit_reached = 0;
    g_trace_image_base = image_base;
    g_trace_image_size = image_size;
    g_trace_frame_count = 0;
    g_trace_armed = 1;
    trace->armed = true;
    trace->breakpoint = breakpoint;
    error->clear();
    return true;
}

bool ResumeNativeInstructionTrace(std::uint32_t return_address)
{
    if (g_trace_armed == 0 || g_trace_started == 0 || g_trace_paused == 0 ||
        g_trace_breakpoint_pending != 0 || !IsTraceGuestInstructionAddress(return_address))
    {
        return false;
    }
    std::uint8_t* byte = GuestByte(return_address);
    g_trace_breakpoint = return_address;
    g_trace_original_byte = *byte;
    *byte = 0xCC;
    g_trace_breakpoint_pending = 1;
    return true;
}

void StopNativeInstructionTrace()
{
    if (g_trace_armed == 0)
    {
        return;
    }
    if (g_trace_breakpoint_pending != 0 && g_trace_breakpoint != 0)
    {
        *GuestByte(g_trace_breakpoint) = static_cast<std::uint8_t>(g_trace_original_byte);
        g_trace_breakpoint_pending = 0;
    }
    g_trace_armed = 0;
}

void FinalizeNativeInstructionTrace(NativeInstructionTrace* trace)
{
    if (trace == nullptr)
    {
        return;
    }
    if (g_trace_armed != 0 && g_trace_breakpoint_pending != 0 && g_trace_breakpoint != 0)
    {
        *GuestByte(g_trace_breakpoint) = static_cast<std::uint8_t>(g_trace_original_byte);
    }
    trace->armed = trace->armed || g_trace_breakpoint != 0;
    trace->started = g_trace_started != 0;
    trace->limit_reached = g_trace_limit_reached != 0;
    if (trace->breakpoint == 0)
    {
        trace->breakpoint = g_trace_breakpoint;
    }
    trace->frame_count = g_trace_frame_count;
    if (trace->frame_count > trace->frames.size())
    {
        trace->frame_count = static_cast<std::uint32_t>(trace->frames.size());
    }
    for (std::uint32_t index = 0; index < trace->frame_count; ++index)
    {
        const RawInstructionTraceFrame& source = g_trace_frames[index];
        NativeInstructionTraceFrame& destination = trace->frames[index];
        destination.instruction_pointer = source.instruction_pointer;
        destination.stack_pointer = source.stack_pointer;
        destination.eax = source.eax;
        destination.ebx = source.ebx;
        destination.ecx = source.ecx;
        destination.edx = source.edx;
        destination.esi = source.esi;
        destination.edi = source.edi;
        destination.ebp = source.ebp;
        destination.eflags = source.eflags;
    }
    g_trace_armed = 0;
    g_trace_started = 0;
    g_trace_paused = 0;
    g_trace_breakpoint_pending = 0;
    g_trace_limit_reached = 0;
    g_trace_breakpoint = 0;
    g_trace_original_byte = 0;
    g_trace_image_base = 0;
    g_trace_image_size = 0;
    g_trace_frame_count = 0;
}

}  // namespace re2dj::platform::linux
