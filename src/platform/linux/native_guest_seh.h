#ifndef RE2DJ_PLATFORM_LINUX_NATIVE_GUEST_SEH_H_
#define RE2DJ_PLATFORM_LINUX_NATIVE_GUEST_SEH_H_

#include <array>
#include <cstdint>
#include <string>

#include "native_guest_fault.h"
#include "native_low_memory.h"

namespace re2dj::platform::linux
{

inline constexpr std::uint32_t kExceptionAccessViolation = 0xC0000005U;
inline constexpr std::uint32_t kExceptionIllegalInstruction = 0xC000001DU;
inline constexpr std::uint32_t kExceptionIntDivideByZero = 0xC0000094U;
inline constexpr std::uint32_t kExceptionIntOverflow = 0xC0000095U;
inline constexpr std::uint32_t kExceptionPrivilegedInstruction = 0xC0000096U;
inline constexpr std::uint32_t kExceptionBreakpoint = 0x80000003U;
inline constexpr std::uint32_t kExceptionSingleStep = 0x80000004U;
inline constexpr std::uint32_t kExceptionContinuable = 0x00000000U;

inline constexpr std::uint32_t kContextI386 = 0x00010000U;
inline constexpr std::uint32_t kContextControl = kContextI386 | 0x00000001U;
inline constexpr std::uint32_t kContextInteger = kContextI386 | 0x00000002U;
inline constexpr std::uint32_t kContextSegments = kContextI386 | 0x00000004U;
inline constexpr std::uint32_t kContextFull32 =
    kContextControl | kContextInteger | kContextSegments;
// What Windows 11 hands a 32-bit SEH handler: every part, extended registers
// included.
inline constexpr std::uint32_t kContextAll32 = 0x0001007FU;

inline constexpr std::uint32_t kExceptionContinueExecution = 0U;
inline constexpr std::uint32_t kExceptionContinueSearch = 1U;
inline constexpr std::uint32_t kExceptionNestedException = 2U;
inline constexpr std::uint32_t kExceptionCollidedUnwind = 3U;

struct Win32ExceptionRecord32
{
    std::uint32_t exception_code = 0;
    std::uint32_t exception_flags = 0;
    std::uint32_t exception_record = 0;
    std::uint32_t exception_address = 0;
    std::uint32_t number_parameters = 0;
    std::array<std::uint32_t, 15> exception_information = {};
};
static_assert(sizeof(Win32ExceptionRecord32) == 80);

struct Win32FloatingSaveArea32
{
    std::uint32_t control_word = 0;
    std::uint32_t status_word = 0;
    std::uint32_t tag_word = 0;
    std::uint32_t error_offset = 0;
    std::uint32_t error_selector = 0;
    std::uint32_t data_offset = 0;
    std::uint32_t data_selector = 0;
    std::array<std::uint8_t, 80> register_area = {};
    std::uint32_t cr0_npx_state = 0;
};
static_assert(sizeof(Win32FloatingSaveArea32) == 112);

struct Win32Context32
{
    std::uint32_t context_flags = 0;

    std::uint32_t dr0 = 0;
    std::uint32_t dr1 = 0;
    std::uint32_t dr2 = 0;
    std::uint32_t dr3 = 0;
    std::uint32_t dr6 = 0;
    std::uint32_t dr7 = 0;

    Win32FloatingSaveArea32 float_save = {};

    std::uint32_t seg_gs = 0;
    std::uint32_t seg_fs = 0;
    std::uint32_t seg_es = 0;
    std::uint32_t seg_ds = 0;

    std::uint32_t edi = 0;
    std::uint32_t esi = 0;
    std::uint32_t ebx = 0;
    std::uint32_t edx = 0;
    std::uint32_t ecx = 0;
    std::uint32_t eax = 0;

    std::uint32_t ebp = 0;
    std::uint32_t eip = 0;
    std::uint32_t seg_cs = 0;
    std::uint32_t eflags = 0;
    std::uint32_t esp = 0;
    std::uint32_t seg_ss = 0;

    std::array<std::uint8_t, 512> extended_registers = {};
};
static_assert(sizeof(Win32Context32) == 716);

struct Win32ExceptionRegistrationRecord32
{
    std::uint32_t next = 0;
    std::uint32_t handler = 0;
};
static_assert(sizeof(Win32ExceptionRegistrationRecord32) == 8);

// Where a guest fault's exception is delivered: a small 32-bit dispatcher in
// low memory that walks the guest's SEH chain in guest mode, as Windows'
// KiUserExceptionDispatcher does, and a data page it and the host record
// into. A handler that never returns (an __except block reached through
// RtlUnwind) simply leaves the dispatcher's frame behind, as on Windows.
struct NativeGuestExceptionDispatcher
{
    NativeLowMemory code;
    NativeLowMemory data;
    // The dispatcher's entry, taking [esp] = record and [esp + 4] = CONTEXT.
    std::uint32_t entry = 0;
    // The HLT the dispatcher stops at when no handler continues, with ebx the
    // record and esi the CONTEXT.
    std::uint32_t stop = 0;
};

// What the dispatcher and the host have recorded so far.
struct NativeGuestExceptionCounters
{
    // Exceptions delivered to the guest, and the last one's code and address.
    std::uint32_t delivered = 0;
    std::uint32_t last_code = 0;
    std::uint32_t last_address = 0;
    // Handlers that returned ExceptionContinueExecution, the last of them,
    // and the EIP its CONTEXT resumed at.
    std::uint32_t resumed = 0;
    std::uint32_t last_handler = 0;
    std::uint32_t last_resumed_eip = 0;
};

bool CreateNativeGuestExceptionDispatcher(NativeGuestExceptionDispatcher* dispatcher, std::string* error);
void ReleaseNativeGuestExceptionDispatcher(NativeGuestExceptionDispatcher* dispatcher);
NativeGuestExceptionCounters ReadNativeGuestExceptionCounters(const NativeGuestExceptionDispatcher& dispatcher);

// A guest fault as the CPU reported it to the signal handler.
struct NativeGuestTrapCause
{
    std::uint32_t trap_number = 0;
    std::uint32_t error_code = 0;
    // CR2 for a page fault.
    std::uint32_t fault_address = 0;
};

inline constexpr std::uint32_t kTrapDivideError = 0;
inline constexpr std::uint32_t kTrapDebug = 1;
inline constexpr std::uint32_t kTrapBreakpoint = 3;
inline constexpr std::uint32_t kTrapOverflow = 4;
inline constexpr std::uint32_t kTrapInvalidOpcode = 6;
inline constexpr std::uint32_t kTrapGeneralProtection = 13;
inline constexpr std::uint32_t kTrapPageFault = 14;

// The exception record and CONTEXT Windows 11 gives a guest SEH handler for
// this fault, as measured with a 32-bit program (see design 404): the code,
// its parameters, the exception address, and the CONTEXT's EIP and EFLAGS.
// Returns false for a fault the model does not translate, such as a general
// protection fault from an instruction that is not a privileged one.
bool DescribeNativeGuestException(const NativeGuestTrapCause& cause,
                                  const NativeTrapRegisters& registers,
                                  Win32ExceptionRecord32* record,
                                  Win32Context32* context);

// Marks [address, address + size) as host code whose INT3 bytes belong to the
// host, such as a diagnostic stop stub, so its traps are never delivered to
// the guest; size 0 clears the mark. One range is kept.
void SetNativeHostTrapRange(std::uint32_t address, std::uint32_t size);
bool IsNativeHostTrap(std::uint32_t address);

// Places the record and CONTEXT on the guest stack where Windows 11 puts
// them, the CONTEXT 1128 bytes below the faulting ESP and the record right
// below it, and points the registers at the dispatcher. Returns false,
// changing nothing, when the guest stack has no room.
bool DeliverNativeGuestException(NativeGuestExceptionDispatcher* dispatcher,
                                 std::uint32_t stack_limit,
                                 std::uint32_t stack_base,
                                 const Win32ExceptionRecord32& record,
                                 const Win32Context32& context,
                                 NativeTrapRegisters* registers);

// True when the registers are the dispatcher's stop: no handler continued
// the exception. The CONTEXT and record it was dispatching are copied out.
bool ReadNativeGuestUnhandledException(const NativeGuestExceptionDispatcher& dispatcher,
                                       const NativeTrapRegisters& registers,
                                       Win32ExceptionRecord32* record,
                                       Win32Context32* context);

}  // namespace re2dj::platform::linux

#endif  // RE2DJ_PLATFORM_LINUX_NATIVE_GUEST_SEH_H_
