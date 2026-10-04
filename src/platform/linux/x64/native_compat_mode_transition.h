#ifndef RE2DJ_PLATFORM_LINUX_X64_NATIVE_COMPAT_MODE_TRANSITION_H_
#define RE2DJ_PLATFORM_LINUX_X64_NATIVE_COMPAT_MODE_TRANSITION_H_

#include <signal.h>

#include <cstddef>
#include <cstdint>

namespace re2dj::platform::native
{

// Linux x86-64 user segment selectors (arch/x86/include/asm/segment.h).
constexpr std::uint16_t kCompatUser32CodeSelector = 0x23;
constexpr std::uint16_t kCompatUser64CodeSelector = 0x33;
constexpr std::uint16_t kCompatUserDataSelector = 0x2B;

// Host state the asm routines reach without thread-local storage, because FS
// holds the guest TEB while guest code runs. It lives in a page below 4 GiB so
// guest import thunks can address cleanup_bytes with a 32-bit absolute
// operand. The asm in native_compat_mode_transition.cpp hard-codes these
// offsets; the static_asserts below keep the two in step.
struct NativeCompatTransitionState
{
    std::uint64_t host_stack_pointer;
    std::uint64_t host_fs_base;
    std::uint64_t guest_fs_selector;
    std::uint64_t use_fsgsbase;
    std::uint32_t cleanup_bytes;
    std::uint32_t reserved;
};

static_assert(offsetof(NativeCompatTransitionState, host_stack_pointer) == 0);
static_assert(offsetof(NativeCompatTransitionState, host_fs_base) == 8);
static_assert(offsetof(NativeCompatTransitionState, guest_fs_selector) == 16);
static_assert(offsetof(NativeCompatTransitionState, use_fsgsbase) == 24);
static_assert(offsetof(NativeCompatTransitionState, cleanup_bytes) == 32);

using NativeCompatEnterFunction = std::uint64_t (*)(NativeCompatTransitionState* state,
                                                    std::uint32_t entry,
                                                    std::uint32_t guest_stack_pointer);

}  // namespace re2dj::platform::native

extern "C"
{

// The blob copied into the low transition page. Offsets of the labels from
// native_compat_blob_start locate the stubs and the fields patched after copy.
extern const unsigned char native_compat_blob_start[];
extern const unsigned char native_compat_blob_end[];
extern const unsigned char native_compat_exit32[];
extern const unsigned char native_compat_exit32_target[];
extern const unsigned char native_compat_gate32[];
extern const unsigned char native_compat_gate32_target[];
extern const unsigned char native_compat_trial32[];
extern const unsigned char native_compat_exit64[];
extern const unsigned char native_compat_exit64_state[];
extern const unsigned char native_compat_exit64_target[];
extern const unsigned char native_compat_gate64[];
extern const unsigned char native_compat_gate64_state[];
extern const unsigned char native_compat_gate64_target[];

// Enters 32-bit guest code at entry with the given guest stack pointer, whose
// top slot must hold the exit32 stub address. Returns edx:eax.
std::uint64_t NativeCompatEnterGuest(re2dj::platform::native::NativeCompatTransitionState* state,
                                     std::uint32_t entry,
                                     std::uint32_t guest_stack_pointer);
void NativeCompatGuestExit();
void NativeCompatImportLanding();
void NativeCompatSignalEntry(int signal_number, siginfo_t* signal_info, void* context);

// Defined in native_compat_mode.cpp and reached from the asm above.
extern re2dj::platform::native::NativeCompatTransitionState* g_native_compat_active_state;
std::uint64_t NativeCompatImportDispatch(
    re2dj::platform::native::NativeCompatTransitionState* state,
    std::uint32_t guest_stack_pointer);
// Returns 1 to resume the guest with the (possibly edited) ucontext, 0 to
// return to a host context; a guest fault never returns.
int NativeCompatSignalHandler(int signal_number, siginfo_t* signal_info, void* context);

}  // extern "C"

#endif  // RE2DJ_PLATFORM_LINUX_X64_NATIVE_COMPAT_MODE_TRANSITION_H_
