#include "native_guest_seh.h"
#include "native_host_services.h"

#include <cstring>

namespace re2dj::platform::native
{
namespace
{

// Written by the diagnostic before the guest runs and read in the trap
// handler; one guest thread runs at a time.
std::uint32_t g_host_trap_begin = 0;
std::uint32_t g_host_trap_size = 0;

// The segment selectors a 32-bit program sees in its CONTEXT on Windows 11.
constexpr std::uint32_t kWindowsCodeSelector = 0x23;
constexpr std::uint32_t kWindowsDataSelector = 0x2B;
constexpr std::uint32_t kWindowsTebSelector = 0x53;

constexpr std::uint32_t kEflagsTrap = 0x00000100U;
constexpr std::uint32_t kEflagsResume = 0x00010000U;

// Where Windows 11 puts the CONTEXT below the faulting ESP, and the record
// right below it (measured).
constexpr std::uint32_t kContextBelowStack = 1128;
constexpr std::uint32_t kRecordBelowStack = kContextBelowStack + sizeof(Win32ExceptionRecord32);
// Stack the dispatcher and the handlers it calls need below the record.
constexpr std::uint32_t kDispatchStackReserve = 16 * 1024;

// The dispatcher, assembled from this source with "as --32":
//
//   dispatcher:     mov ebp, esp               ; [ebp] record, [ebp+4] CONTEXT
//                   sub esp, 16                ; [ebp-4] frame, [ebp-16] dispatcher context
//                   mov eax, fs:[0]
//                   mov [ebp-4], eax
//   next_frame:     mov eax, [ebp-4]
//                   cmp eax, -1                ; end of the chain
//                   je unhandled
//                   cmp eax, fs:[8]            ; below the TEB's stack limit
//                   jb unhandled
//                   lea ecx, [eax+8]
//                   cmp ecx, fs:[4]            ; past the TEB's stack base
//                   ja unhandled
//                   test al, 3                 ; unaligned
//                   jnz unhandled
//                   lea ecx, [ebp-16]
//                   push ecx
//                   push dword ptr [ebp+4]
//                   push eax
//                   push dword ptr [ebp]
//                   call dword ptr [eax+4]     ; handler(record, frame, CONTEXT, dispatcher context)
//                   mov esp, ebp
//                   sub esp, 16
//                   test eax, eax              ; ExceptionContinueExecution
//                   jz resume
//                   cmp eax, 1                 ; ExceptionContinueSearch
//                   jne unhandled
//                   mov eax, [ebp-4]
//                   mov eax, [eax]
//                   mov [ebp-4], eax
//                   jmp next_frame
//   resume:         mov ecx, [ebp+4]
//                   mov eax, [ebp-4]
//                   mov eax, [eax+4]
//                   mov ds:last_handler, eax
//                   mov eax, [ecx+0xB8]
//                   mov ds:last_resumed_eip, eax
//                   inc dword ptr ds:resumed
//                   mov edi, [ecx+0x9C]        ; the CONTEXT's registers
//                   mov esi, [ecx+0xA0]
//                   mov ebx, [ecx+0xA4]
//                   mov edx, [ecx+0xA8]
//                   mov eax, [ecx+0xB0]
//                   mov ebp, [ecx+0xB4]
//                   mov esp, [ecx+0xC4]
//                   push dword ptr [ecx+0xB8]  ; eip
//                   push dword ptr [ecx+0xC0]  ; eflags
//                   push dword ptr [ecx+0xAC]  ; ecx
//                   pop ecx
//                   popfd
//                   ret
//   unhandled:      mov ebx, [ebp]
//                   mov esi, [ebp+4]
//   stop:           hlt
constexpr std::uint8_t kDispatcherCode[] = {
    0x89, 0xE5, 0x83, 0xEC, 0x10, 0x64, 0xA1, 0x00, 0x00, 0x00, 0x00, 0x89, 0x45, 0xFC, 0x8B, 0x45,
    0xFC, 0x83, 0xF8, 0xFF, 0x0F, 0x84, 0xA9, 0x00, 0x00, 0x00, 0x64, 0x3B, 0x05, 0x08, 0x00, 0x00,
    0x00, 0x0F, 0x82, 0x9C, 0x00, 0x00, 0x00, 0x8D, 0x48, 0x08, 0x64, 0x3B, 0x0D, 0x04, 0x00, 0x00,
    0x00, 0x0F, 0x87, 0x8C, 0x00, 0x00, 0x00, 0xA8, 0x03, 0x0F, 0x85, 0x84, 0x00, 0x00, 0x00, 0x8D,
    0x4D, 0xF0, 0x51, 0xFF, 0x75, 0x04, 0x50, 0xFF, 0x75, 0x00, 0xFF, 0x50, 0x04, 0x89, 0xEC, 0x83,
    0xEC, 0x10, 0x85, 0xC0, 0x74, 0x0F, 0x83, 0xF8, 0x01, 0x75, 0x68, 0x8B, 0x45, 0xFC, 0x8B, 0x00,
    0x89, 0x45, 0xFC, 0xEB, 0xA9, 0x8B, 0x4D, 0x04, 0x8B, 0x45, 0xFC, 0x8B, 0x40, 0x04, 0xA3, 0x22,
    0x22, 0x22, 0x22, 0x8B, 0x81, 0xB8, 0x00, 0x00, 0x00, 0xA3, 0x33, 0x33, 0x33, 0x33, 0xFF, 0x05,
    0x11, 0x11, 0x11, 0x11, 0x8B, 0xB9, 0x9C, 0x00, 0x00, 0x00, 0x8B, 0xB1, 0xA0, 0x00, 0x00, 0x00,
    0x8B, 0x99, 0xA4, 0x00, 0x00, 0x00, 0x8B, 0x91, 0xA8, 0x00, 0x00, 0x00, 0x8B, 0x81, 0xB0, 0x00,
    0x00, 0x00, 0x8B, 0xA9, 0xB4, 0x00, 0x00, 0x00, 0x8B, 0xA1, 0xC4, 0x00, 0x00, 0x00, 0xFF, 0xB1,
    0xB8, 0x00, 0x00, 0x00, 0xFF, 0xB1, 0xC0, 0x00, 0x00, 0x00, 0xFF, 0xB1, 0xAC, 0x00, 0x00, 0x00,
    0x59, 0x9D, 0xC3, 0x8B, 0x5D, 0x00, 0x8B, 0x75, 0x04, 0xF4};
static_assert(sizeof(kDispatcherCode) == 202);
// Where the data page's addresses are patched in, and the stop's offset.
constexpr std::size_t kLastHandlerPatch = 0x6F;
constexpr std::size_t kLastResumedEipPatch = 0x7A;
constexpr std::size_t kResumedPatch = 0x80;
constexpr std::uint32_t kStopOffset = 0xC9;

// The data page's slots.
constexpr std::uint32_t kResumedSlot = 0;
constexpr std::uint32_t kLastHandlerSlot = 4;
constexpr std::uint32_t kLastResumedEipSlot = 8;
constexpr std::uint32_t kDeliveredSlot = 12;
constexpr std::uint32_t kLastCodeSlot = 16;
constexpr std::uint32_t kLastAddressSlot = 20;

std::uint32_t ReadU32(std::uint32_t address)
{
    std::uint32_t value = 0;
    std::memcpy(&value, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(address)), sizeof(value));
    return value;
}

void WriteU32(std::uint32_t address, std::uint32_t value)
{
    std::memcpy(reinterpret_cast<void*>(static_cast<std::uintptr_t>(address)), &value, sizeof(value));
}

std::uint8_t ReadU8(std::uint32_t address)
{
    return *reinterpret_cast<const std::uint8_t*>(static_cast<std::uintptr_t>(address));
}

// The privileged instructions a user-mode general protection fault can come
// from that Windows 11 reports as STATUS_PRIVILEGED_INSTRUCTION: HLT, CLI,
// MOV to or from a control register (measured), and port I/O (the Windows
// product's legacy I/O trap receives these as such).
bool IsPrivilegedInstruction(std::uint32_t eip)
{
    std::uint8_t opcode = ReadU8(eip);
    if (opcode == 0x66)
    {
        opcode = ReadU8(eip + 1);
    }
    switch (opcode)
    {
    case 0xF4:  // hlt
    case 0xFA:  // cli
    case 0xE4:  // in al, imm8
    case 0xE5:
    case 0xE6:  // out imm8, al
    case 0xE7:
    case 0xEC:  // in al, dx
    case 0xED:
    case 0xEE:  // out dx, al
    case 0xEF:
        return true;
    case 0x0F:
    {
        const std::uint8_t second = ReadU8(eip + 1);
        return second == 0x20 || second == 0x22;  // mov r32, crN / mov crN, r32
    }
    default:
        return false;
    }
}

}  // namespace

bool CreateNativeGuestExceptionDispatcher(NativeGuestExceptionDispatcher* dispatcher, std::string* error)
{
    if (dispatcher == nullptr || error == nullptr)
    {
        return false;
    }
    if (dispatcher->entry != 0)
    {
        return true;
    }
    constexpr std::uint32_t kPage = 4096;
    if (!MapNativeLowMemory(kPage, HostProtection::kReadWrite, &dispatcher->code, error))
    {
        return false;
    }
    if (!MapNativeLowMemory(kPage, HostProtection::kReadWrite, &dispatcher->data, error))
    {
        ReleaseNativeLowMemory(&dispatcher->code);
        return false;
    }
    auto* code = static_cast<std::uint8_t*>(dispatcher->code.memory);
    std::memcpy(code, kDispatcherCode, sizeof(kDispatcherCode));
    const std::uint32_t data = dispatcher->data.address;
    const auto patch = [&](std::size_t offset, std::uint32_t value) { std::memcpy(code + offset, &value, 4); };
    patch(kLastHandlerPatch, data + kLastHandlerSlot);
    patch(kLastResumedEipPatch, data + kLastResumedEipSlot);
    patch(kResumedPatch, data + kResumedSlot);
    if (!HostProtect(dispatcher->code.memory, kPage, HostProtection::kReadExecute))
    {
        *error = "cannot make the guest exception dispatcher executable";
        ReleaseNativeGuestExceptionDispatcher(dispatcher);
        return false;
    }
    dispatcher->entry = dispatcher->code.address;
    dispatcher->stop = dispatcher->code.address + kStopOffset;
    error->clear();
    return true;
}

void ReleaseNativeGuestExceptionDispatcher(NativeGuestExceptionDispatcher* dispatcher)
{
    if (dispatcher == nullptr)
    {
        return;
    }
    ReleaseNativeLowMemory(&dispatcher->code);
    ReleaseNativeLowMemory(&dispatcher->data);
    dispatcher->entry = 0;
    dispatcher->stop = 0;
}

NativeGuestExceptionCounters ReadNativeGuestExceptionCounters(const NativeGuestExceptionDispatcher& dispatcher)
{
    NativeGuestExceptionCounters counters;
    if (dispatcher.entry == 0)
    {
        return counters;
    }
    const std::uint32_t data = dispatcher.data.address;
    counters.delivered = ReadU32(data + kDeliveredSlot);
    counters.last_code = ReadU32(data + kLastCodeSlot);
    counters.last_address = ReadU32(data + kLastAddressSlot);
    counters.resumed = ReadU32(data + kResumedSlot);
    counters.last_handler = ReadU32(data + kLastHandlerSlot);
    counters.last_resumed_eip = ReadU32(data + kLastResumedEipSlot);
    return counters;
}

bool DescribeNativeGuestException(const NativeGuestTrapCause& cause,
                                  const NativeTrapRegisters& registers,
                                  Win32ExceptionRecord32* record,
                                  Win32Context32* context)
{
    if (record == nullptr || context == nullptr)
    {
        return false;
    }
    Win32ExceptionRecord32 described;
    std::uint32_t eip = registers.eip;
    std::uint32_t eflags = registers.eflags & ~kEflagsTrap;
    // Faults report the instruction itself with RF set; traps report where
    // the CPU stopped (measured).
    bool fault = true;
    switch (cause.trap_number)
    {
    case kTrapPageFault:
        described.exception_code = kExceptionAccessViolation;
        described.number_parameters = 2;
        // 8 for an instruction fetch, 1 for a write, 0 for a read.
        described.exception_information[0] = (cause.error_code & 0x10U) != 0 ? 8U
                                             : (cause.error_code & 0x02U) != 0 ? 1U
                                                                               : 0U;
        described.exception_information[1] = cause.fault_address;
        break;
    case kTrapGeneralProtection:
        if (!IsPrivilegedInstruction(eip))
        {
            return false;
        }
        described.exception_code = kExceptionPrivilegedInstruction;
        break;
    case kTrapInvalidOpcode:
        described.exception_code = kExceptionIllegalInstruction;
        break;
    case kTrapDivideError:
        // A quotient overflow is also #DE; the model does not tell it apart.
        described.exception_code = kExceptionIntDivideByZero;
        break;
    case kTrapBreakpoint:
        // The CPU stops after the INT3 byte; Windows reports that byte, also
        // one byte back into "int 3" (CD 03).
        described.exception_code = kExceptionBreakpoint;
        described.number_parameters = 1;
        eip -= 1;
        fault = false;
        break;
    case kTrapOverflow:
        // INTO: the address is the instruction, the CONTEXT resumes after it.
        described.exception_code = kExceptionIntOverflow;
        described.exception_address = eip - 1;
        fault = false;
        break;
    case kTrapDebug:
        described.exception_code = kExceptionSingleStep;
        fault = false;
        break;
    default:
        return false;
    }
    if (described.exception_address == 0)
    {
        described.exception_address = eip;
    }
    if (fault)
    {
        eflags |= kEflagsResume;
    }
    described.exception_flags = kExceptionContinuable;
    *record = described;

    *context = {};
    context->context_flags = kContextAll32;
    context->edi = registers.edi;
    context->esi = registers.esi;
    context->ebx = registers.ebx;
    context->edx = registers.edx;
    context->ecx = registers.ecx;
    context->eax = registers.eax;
    context->ebp = registers.ebp;
    context->eip = eip;
    context->eflags = eflags;
    context->esp = registers.esp;
    context->seg_cs = kWindowsCodeSelector;
    context->seg_ss = kWindowsDataSelector;
    context->seg_ds = kWindowsDataSelector;
    context->seg_es = kWindowsDataSelector;
    context->seg_gs = kWindowsDataSelector;
    context->seg_fs = kWindowsTebSelector;
    return true;
}

void SetNativeHostTrapRange(std::uint32_t address, std::uint32_t size)
{
    g_host_trap_begin = address;
    g_host_trap_size = size;
}

bool IsNativeHostTrap(std::uint32_t address)
{
    return g_host_trap_size != 0 && address >= g_host_trap_begin && address - g_host_trap_begin < g_host_trap_size;
}

bool DeliverNativeGuestException(NativeGuestExceptionDispatcher* dispatcher,
                                 std::uint32_t stack_limit,
                                 std::uint32_t stack_base,
                                 const Win32ExceptionRecord32& record,
                                 const Win32Context32& context,
                                 NativeTrapRegisters* registers)
{
    if (dispatcher == nullptr || registers == nullptr || dispatcher->entry == 0)
    {
        return false;
    }
    const std::uint32_t esp = registers->esp;
    if (esp > stack_base || esp < stack_limit || esp - stack_limit < kRecordBelowStack + kDispatchStackReserve)
    {
        return false;
    }
    const std::uint32_t context_address = esp - kContextBelowStack;
    const std::uint32_t record_address = esp - kRecordBelowStack;
    std::memcpy(reinterpret_cast<void*>(static_cast<std::uintptr_t>(context_address)), &context, sizeof(context));
    std::memcpy(reinterpret_cast<void*>(static_cast<std::uintptr_t>(record_address)), &record, sizeof(record));
    const std::uint32_t arguments = record_address - 2 * sizeof(std::uint32_t);
    WriteU32(arguments, record_address);
    WriteU32(arguments + 4, context_address);

    const std::uint32_t data = dispatcher->data.address;
    WriteU32(data + kDeliveredSlot, ReadU32(data + kDeliveredSlot) + 1);
    WriteU32(data + kLastCodeSlot, record.exception_code);
    WriteU32(data + kLastAddressSlot, record.exception_address);

    registers->esp = arguments;
    registers->eip = dispatcher->entry;
    registers->eflags &= ~kEflagsTrap;
    return true;
}

bool ReadNativeGuestUnhandledException(const NativeGuestExceptionDispatcher& dispatcher,
                                       const NativeTrapRegisters& registers,
                                       Win32ExceptionRecord32* record,
                                       Win32Context32* context)
{
    if (dispatcher.entry == 0 || registers.eip != dispatcher.stop || record == nullptr || context == nullptr)
    {
        return false;
    }
    std::memcpy(record, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(registers.ebx)), sizeof(*record));
    std::memcpy(context, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(registers.esi)), sizeof(*context));
    return true;
}

}  // namespace re2dj::platform::native
