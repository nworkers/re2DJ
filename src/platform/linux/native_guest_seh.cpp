#include "native_guest_seh.h"

#include <cstring>

namespace re2dj::platform::linux
{
namespace
{

bool IsStackRange(std::uint32_t address,
                  std::uint32_t size,
                  std::uint32_t stack_limit,
                  std::uint32_t stack_base)
{
    return address >= stack_limit && address <= stack_base && size <= stack_base - address;
}

// Written by the diagnostic before the guest runs and read in the trap
// handler; one guest thread runs at a time.
std::uint32_t g_host_trap_begin = 0;
std::uint32_t g_host_trap_size = 0;

}  // namespace

void SetNativeHostTrapRange(std::uint32_t address, std::uint32_t size)
{
    g_host_trap_begin = address;
    g_host_trap_size = size;
}

bool PrepareNativeGuestBreakpointDispatch(const NativeTrapRegisters& registers,
                                          std::uint32_t teb,
                                          std::uint32_t image_base,
                                          std::uint32_t stack_limit,
                                          std::uint32_t stack_base,
                                          NativeGuestSehDispatch* dispatch)
{
    if (dispatch == nullptr || registers.eip == 0 || teb == 0)
    {
        return false;
    }
    // A breakpoint trap reports the address after the INT3 byte.
    const std::uint32_t int3_address = registers.eip - 1;
    if ((image_base != 0 && int3_address < image_base) ||
        (g_host_trap_size != 0 && int3_address >= g_host_trap_begin &&
         int3_address - g_host_trap_begin < g_host_trap_size))
    {
        return false;
    }
    std::uint8_t code_byte = 0;
    std::memcpy(&code_byte,
                reinterpret_cast<const void*>(static_cast<std::uintptr_t>(int3_address)),
                sizeof(code_byte));
    if (code_byte != 0xCC)
    {
        return false;
    }

    std::uint32_t exception_list = 0;
    std::memcpy(&exception_list,
                reinterpret_cast<const void*>(static_cast<std::uintptr_t>(teb)),
                sizeof(exception_list));
    if (exception_list == 0 || exception_list == 0xFFFFFFFFU ||
        !IsStackRange(exception_list, sizeof(Win32ExceptionRegistrationRecord32),
                      stack_limit, stack_base))
    {
        return false;
    }
    Win32ExceptionRegistrationRecord32 frame;
    std::memcpy(&frame,
                reinterpret_cast<const void*>(static_cast<std::uintptr_t>(exception_list)),
                sizeof(frame));
    if (frame.handler == 0)
    {
        return false;
    }

    *dispatch = {};
    dispatch->frame_address = exception_list;
    dispatch->frame = frame;
    dispatch->record.exception_code = kExceptionBreakpoint;
    dispatch->record.exception_flags = kExceptionContinuable;
    dispatch->record.exception_address = int3_address;

    Win32Context32& context = dispatch->context;
    context.context_flags = kContextFull32;
    context.edi = registers.edi;
    context.esi = registers.esi;
    context.ebx = registers.ebx;
    context.edx = registers.edx;
    context.ecx = registers.ecx;
    context.eax = registers.eax;
    context.ebp = registers.ebp;
    context.eip = int3_address;
    context.seg_cs = registers.cs;
    context.eflags = registers.eflags;
    context.esp = registers.esp;
    context.seg_ss = registers.ss;
    context.seg_fs = registers.fs;
    context.seg_ds = registers.ds;
    context.seg_es = registers.es;
    context.seg_gs = registers.gs;
    return true;
}

void ApplyNativeGuestSehContext(const Win32Context32& context, NativeTrapRegisters* registers)
{
    if (registers == nullptr)
    {
        return;
    }
    registers->eip = context.eip;
    registers->esp = context.esp;
    registers->eax = context.eax;
    registers->ebx = context.ebx;
    registers->ecx = context.ecx;
    registers->edx = context.edx;
    registers->esi = context.esi;
    registers->edi = context.edi;
    registers->ebp = context.ebp;
    registers->eflags = context.eflags;
}

}  // namespace re2dj::platform::linux
