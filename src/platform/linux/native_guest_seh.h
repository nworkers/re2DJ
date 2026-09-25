#ifndef RE2DJ_PLATFORM_LINUX_NATIVE_GUEST_SEH_H_
#define RE2DJ_PLATFORM_LINUX_NATIVE_GUEST_SEH_H_

#include <array>
#include <cstdint>

#include "native_guest_fault.h"

namespace re2dj::platform::linux
{

inline constexpr std::uint32_t kExceptionBreakpoint = 0x80000003U;
inline constexpr std::uint32_t kExceptionContinuable = 0x00000000U;

inline constexpr std::uint32_t kContextI386 = 0x00010000U;
inline constexpr std::uint32_t kContextControl = kContextI386 | 0x00000001U;
inline constexpr std::uint32_t kContextInteger = kContextI386 | 0x00000002U;
inline constexpr std::uint32_t kContextSegments = kContextI386 | 0x00000004U;
inline constexpr std::uint32_t kContextFull32 =
    kContextControl | kContextInteger | kContextSegments;

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

// Everything a width needs to call the first registered guest SEH handler for
// a guest INT3: the handler, its frame, and the record and CONTEXT it takes.
struct NativeGuestSehDispatch
{
    std::uint32_t frame_address = 0;
    Win32ExceptionRegistrationRecord32 frame;
    Win32ExceptionRecord32 record;
    Win32Context32 context;
};

// Marks [address, address + size) as host code whose INT3 bytes belong to the
// host, such as a diagnostic stop stub, so a guest SEH frame never receives
// them; size 0 clears the mark. One range is kept.
void SetNativeHostTrapRange(std::uint32_t address, std::uint32_t size);

// Recognizes a trap as a guest INT3 at or above image_base, outside the host
// trap range, whose TEB holds a registered SEH frame inside the guest stack,
// and prepares the handler call.
// Returns false for any other trap, leaving it to be reported as a fault.
bool PrepareNativeGuestBreakpointDispatch(const NativeTrapRegisters& registers,
                                          std::uint32_t teb,
                                          std::uint32_t image_base,
                                          std::uint32_t stack_limit,
                                          std::uint32_t stack_base,
                                          NativeGuestSehDispatch* dispatch);

// Applies the control and integer registers of a CONTEXT the handler
// returned with ExceptionContinueExecution. Segments are left untouched.
void ApplyNativeGuestSehContext(const Win32Context32& context, NativeTrapRegisters* registers);

}  // namespace re2dj::platform::linux

#endif  // RE2DJ_PLATFORM_LINUX_NATIVE_GUEST_SEH_H_
