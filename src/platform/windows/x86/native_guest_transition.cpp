#include "native_guest_transition.h"

#include <cstddef>

// The asm here moves SEH chains between fs:0 and the shadow TEB rather than
// registering handlers of its own, so C4733 (fs:0 written without a safe
// handler) does not apply.
#pragma warning(disable : 4733)

static_assert(offsetof(re2dj::platform::native::GuestEscapeFrame, ebx) == 0);
static_assert(offsetof(re2dj::platform::native::GuestEscapeFrame, esi) == 4);
static_assert(offsetof(re2dj::platform::native::GuestEscapeFrame, edi) == 8);
static_assert(offsetof(re2dj::platform::native::GuestEscapeFrame, ebp) == 12);
static_assert(offsetof(re2dj::platform::native::GuestEscapeFrame, esp) == 16);
static_assert(offsetof(re2dj::platform::native::GuestEscapeFrame, eip) == 20);
static_assert(offsetof(re2dj::platform::native::GuestEscapeFrame, exception_list) == 24);

extern "C" __declspec(naked) std::uint32_t __cdecl EnterGuestRun(re2dj::platform::native::GuestEscapeFrame*,
                                                                 void(__cdecl*)(void*),
                                                                 void*)
{
    __asm
    {
        mov eax, [esp + 4]
        mov [eax], ebx
        mov [eax + 4], esi
        mov [eax + 8], edi
        mov [eax + 12], ebp
        // The stack pointer as it is once this call has returned.
        lea ecx, [esp + 4]
        mov [eax + 16], ecx
        mov ecx, [esp]
        mov [eax + 20], ecx
        mov ecx, fs:[0]
        mov [eax + 24], ecx
        mov ecx, [esp + 8]
        mov edx, [esp + 12]
        push edx
        call ecx
        add esp, 4
        xor eax, eax
        ret
    }
}

extern "C" __declspec(naked) void __fastcall EscapeGuestRun(re2dj::platform::native::GuestEscapeFrame*)
{
    __asm
    {
        mov ebx, [ecx]
        mov esi, [ecx + 4]
        mov edi, [ecx + 8]
        mov ebp, [ecx + 12]
        mov eax, [ecx + 24]
        mov fs:[0], eax
        mov esp, [ecx + 16]
        mov eax, 1
        jmp dword ptr [ecx + 20]
    }
}

extern "C" __declspec(naked) std::uint32_t __cdecl CallGuestEntry(std::uint32_t)
{
    __asm
    {
        push ebp
        mov ebp, esp
        push ebx
        push esi
        push edi
        push dword ptr fs:[0]
        mov dword ptr fs:[0], 0FFFFFFFFh
        and esp, 0FFFFFFF0h
        call dword ptr [ebp + 8]
        mov ecx, [ebp - 16]
        mov fs:[0], ecx
        lea esp, [ebp - 12]
        pop edi
        pop esi
        pop ebx
        pop ebp
        ret
    }
}

extern "C" __declspec(naked) std::uint32_t __cdecl CallGuestThread(std::uint32_t, std::uint32_t)
{
    __asm
    {
        push ebp
        mov ebp, esp
        push ebx
        push esi
        push edi
        push dword ptr fs:[0]
        mov dword ptr fs:[0], 0FFFFFFFFh
        and esp, 0FFFFFFF0h
        sub esp, 12
        push dword ptr [ebp + 12]
        call dword ptr [ebp + 8]
        mov ecx, [ebp - 16]
        mov fs:[0], ecx
        lea esp, [ebp - 12]
        pop edi
        pop esi
        pop ebx
        pop ebp
        ret
    }
}

extern "C" __declspec(naked) void __cdecl CallGuestTls(std::uint32_t, std::uint32_t)
{
    __asm
    {
        push ebp
        mov ebp, esp
        push ebx
        push esi
        push edi
        push dword ptr fs:[0]
        mov dword ptr fs:[0], 0FFFFFFFFh
        and esp, 0FFFFFFF0h
        sub esp, 4
        // PIMAGE_TLS_CALLBACK(DllHandle, DLL_PROCESS_ATTACH, Reserved).
        push 0
        push 1
        push dword ptr [ebp + 12]
        call dword ptr [ebp + 8]
        mov ecx, [ebp - 16]
        mov fs:[0], ecx
        lea esp, [ebp - 12]
        pop edi
        pop esi
        pop ebx
        pop ebp
        ret
    }
}

extern "C" __declspec(naked) std::uint32_t __cdecl CallGuestStdcallWords(std::uint32_t,
                                                                         const std::uint32_t*,
                                                                         std::uint32_t,
                                                                         std::uint32_t*)
{
    __asm
    {
        push ebp
        mov ebp, esp
        push ebx
        push esi
        push edi
        // The host chain, back once the guest returns.
        push dword ptr fs:[0]
        mov eax, [ebp + 20]
        mov eax, [eax]
        mov fs:[0], eax
        mov ecx, [ebp + 16]
        mov esi, [ebp + 12]
        lea eax, [ecx * 4]
        sub esp, eax
        and esp, 0FFFFFFF0h
        mov edi, esp
        cld
        rep movsd
        call dword ptr [ebp + 8]
        mov ecx, [ebp + 20]
        mov edx, fs:[0]
        mov [ecx], edx
        mov edx, [ebp - 16]
        mov fs:[0], edx
        lea esp, [ebp - 12]
        pop edi
        pop esi
        pop ebx
        pop ebp
        ret
    }
}
