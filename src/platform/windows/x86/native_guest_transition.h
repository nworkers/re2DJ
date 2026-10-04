#ifndef RE2DJ_PLATFORM_WINDOWS_X86_NATIVE_GUEST_TRANSITION_H_
#define RE2DJ_PLATFORM_WINDOWS_X86_NATIVE_GUEST_TRANSITION_H_

#include <cstdint>

// The x86 code that moves between host and guest on the Windows backend
// (task 448), in MSVC naked functions. The guest runs on the calling host
// thread's own stack with the thread's real TEB in FS; only fs:0 changes
// hands. While guest code runs, fs:0 is the guest's SEH chain, which starts
// at -1; while host code runs it is the host's.
namespace re2dj::platform::native
{

// What EnterGuestRun saves so EscapeGuestRun can return from it from
// anywhere below: the callee-saved registers, the stack pointer and return
// address of the EnterGuestRun call, and fs:0. Offsets are fixed for the asm.
struct GuestEscapeFrame
{
    std::uint32_t ebx = 0;
    std::uint32_t esi = 0;
    std::uint32_t edi = 0;
    std::uint32_t ebp = 0;
    std::uint32_t esp = 0;
    std::uint32_t eip = 0;
    std::uint32_t exception_list = 0;
};

}  // namespace re2dj::platform::native

extern "C"
{

// Records frame, then calls body(context). Returns 0 when body returns, or 1
// when EscapeGuestRun(frame) ends it from anywhere inside.
std::uint32_t __cdecl EnterGuestRun(re2dj::platform::native::GuestEscapeFrame* frame,
                                    void(__cdecl* body)(void*),
                                    void* context);
// Returns 1 from the EnterGuestRun call that recorded frame, restoring its
// registers and fs:0; host frames in between are abandoned. The vectored
// exception handler reaches it by setting a CONTEXT's Eip here and Ecx to
// frame, which is why it is fastcall.
[[noreturn]] void __fastcall EscapeGuestRun(re2dj::platform::native::GuestEscapeFrame* frame);

// Call guest code on the current stack, 16-byte aligned, with fs:0 = -1 for
// the guest's own SEH chain; the caller's registers, stack and fs:0 come
// back whatever the guest leaves.
std::uint32_t __cdecl CallGuestEntry(std::uint32_t entry);
std::uint32_t __cdecl CallGuestThread(std::uint32_t start, std::uint32_t parameter);
void __cdecl CallGuestTls(std::uint32_t callback, std::uint32_t image_base);

// Calls a guest stdcall function from host code handling an import: copies
// count argument words below a 16-byte aligned stack pointer, makes
// *exception_list (the guest chain kept in the shadow TEB) the live fs:0 for
// the call, and stores the chain the guest leaves back there.
std::uint32_t __cdecl CallGuestStdcallWords(std::uint32_t function,
                                            const std::uint32_t* words,
                                            std::uint32_t count,
                                            std::uint32_t* exception_list);

}

#endif  // RE2DJ_PLATFORM_WINDOWS_X86_NATIVE_GUEST_TRANSITION_H_
