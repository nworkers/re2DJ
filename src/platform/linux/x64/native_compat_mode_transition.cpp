#include "native_compat_mode_transition.h"

// Every transition between the 64-bit host (CS 0x33) and 32-bit guest code
// (CS 0x23) lives here as top-level asm. Offsets into
// NativeCompatTransitionState: 0 host rsp, 8 host FS base, 16 guest FS
// selector, 24 use_fsgsbase, 32 cleanup bytes.
//
// After a switch from compatibility mode to 64-bit mode the upper halves of
// the general-purpose registers and r8-r15 are undefined (Intel SDM Vol. 1
// 3.4.1.1), so the 64-bit landings zero-extend esp before using it and never
// rely on a value kept in a register across guest execution.

// The blob is copied into a page below 4 GiB because far pointers from 32-bit
// code carry only a 32-bit offset. It is data here; only the copy executes.
__asm__(
    ".pushsection .rodata.re2dj_native_compat, \"a\"\n"
    ".balign 16\n"
    ".globl native_compat_blob_start\n"
    ".hidden native_compat_blob_start\n"
    "native_compat_blob_start:\n"
    ".code32\n"

    // Return target of a guest entry: far-jump back to 64-bit mode.
    ".globl native_compat_exit32\n"
    ".hidden native_compat_exit32\n"
    "native_compat_exit32:\n"
    "    .byte 0xEA\n"
    ".globl native_compat_exit32_target\n"
    ".hidden native_compat_exit32_target\n"
    "native_compat_exit32_target:\n"
    "    .long 0\n"
    "    .word 0x33\n"

    // Import bridge seen by guest thunks: a one-argument stdcall function
    // returning edx:eax, like the i386 NativeImportGateBridge.
    ".globl native_compat_gate32\n"
    ".hidden native_compat_gate32\n"
    "native_compat_gate32:\n"
    "    .byte 0x9A\n"
    ".globl native_compat_gate32_target\n"
    ".hidden native_compat_gate32_target\n"
    "native_compat_gate32_target:\n"
    "    .long 0\n"
    "    .word 0x33\n"
    "    ret $4\n"

    // Trial entry used at initialization to prove the transition works.
    ".globl native_compat_trial32\n"
    ".hidden native_compat_trial32\n"
    "native_compat_trial32:\n"
    "    movl $0x2301C0DE, %eax\n"
    "    ret\n"

    ".code64\n"
    ".balign 16\n"
    // movabs $state, %r10; movabs $target, %r11; jmp *%r11
    ".globl native_compat_exit64\n"
    ".hidden native_compat_exit64\n"
    "native_compat_exit64:\n"
    "    .byte 0x49, 0xBA\n"
    ".globl native_compat_exit64_state\n"
    ".hidden native_compat_exit64_state\n"
    "native_compat_exit64_state:\n"
    "    .quad 0\n"
    "    .byte 0x49, 0xBB\n"
    ".globl native_compat_exit64_target\n"
    ".hidden native_compat_exit64_target\n"
    "native_compat_exit64_target:\n"
    "    .quad 0\n"
    "    jmp *%r11\n"

    ".balign 16\n"
    ".globl native_compat_gate64\n"
    ".hidden native_compat_gate64\n"
    "native_compat_gate64:\n"
    "    .byte 0x49, 0xBA\n"
    ".globl native_compat_gate64_state\n"
    ".hidden native_compat_gate64_state\n"
    "native_compat_gate64_state:\n"
    "    .quad 0\n"
    "    .byte 0x49, 0xBB\n"
    ".globl native_compat_gate64_target\n"
    ".hidden native_compat_gate64_target\n"
    "native_compat_gate64_target:\n"
    "    .quad 0\n"
    "    jmp *%r11\n"

    ".globl native_compat_blob_end\n"
    ".hidden native_compat_blob_end\n"
    "native_compat_blob_end:\n"
    ".popsection\n");

__asm__(
    ".pushsection .text\n"

    // Clears the FS selector and restores the host FS base from the state in
    // rdi. Clobbers rax, rcx, rsi, rdi, r11; preserves r8-r10.
    ".balign 16\n"
    ".hidden NativeCompatRestoreHostFs\n"
    ".type NativeCompatRestoreHostFs, @function\n"
    "NativeCompatRestoreHostFs:\n"
    "    xorl %eax, %eax\n"
    "    movw %ax, %fs\n"
    "    cmpq $0, 24(%rdi)\n"
    "    je 1f\n"
    "    movq 8(%rdi), %rax\n"
    "    wrfsbase %rax\n"
    "    ret\n"
    "1:\n"
    "    movq 8(%rdi), %rsi\n"
    "    movl $0x1002, %edi\n"  // ARCH_SET_FS
    "    movl $158, %eax\n"     // SYS_arch_prctl
    "    syscall\n"
    "    ret\n"
    ".size NativeCompatRestoreHostFs, . - NativeCompatRestoreHostFs\n"

    // rdi = state, esi = entry, edx = guest esp.
    ".balign 16\n"
    ".globl NativeCompatEnterGuest\n"
    ".type NativeCompatEnterGuest, @function\n"
    "NativeCompatEnterGuest:\n"
    "    pushq %rbp\n"
    "    pushq %rbx\n"
    "    pushq %r12\n"
    "    pushq %r13\n"
    "    pushq %r14\n"
    "    pushq %r15\n"
    "    subq $8, %rsp\n"
    "    movq %rsp, 0(%rdi)\n"
    "    movl $0x2B, %eax\n"
    "    movw %ax, %ds\n"
    "    movw %ax, %es\n"
    "    movzwl 16(%rdi), %eax\n"
    "    movw %ax, %fs\n"
    "    movl %esi, %esi\n"
    "    movl %edx, %esp\n"
    "    pushq $0x23\n"
    "    pushq %rsi\n"
    "    lretq\n"
    ".size NativeCompatEnterGuest, . - NativeCompatEnterGuest\n"

    // Reached from exit64 with r10 = state and the guest result in edx:eax.
    ".balign 16\n"
    ".hidden NativeCompatGuestExit\n"
    ".globl NativeCompatGuestExit\n"
    ".type NativeCompatGuestExit, @function\n"
    "NativeCompatGuestExit:\n"
    "    cld\n"
    "    movl %eax, %eax\n"
    "    movl %edx, %edx\n"
    "    shlq $32, %rdx\n"
    "    orq %rdx, %rax\n"
    "    movq %rax, %r9\n"
    "    movq 0(%r10), %rsp\n"
    "    movq %r10, %rdi\n"
    "    call NativeCompatRestoreHostFs\n"
    "    movq %r9, %rax\n"
    "    addq $8, %rsp\n"
    "    popq %r15\n"
    "    popq %r14\n"
    "    popq %r13\n"
    "    popq %r12\n"
    "    popq %rbx\n"
    "    popq %rbp\n"
    "    ret\n"
    ".size NativeCompatGuestExit, . - NativeCompatGuestExit\n"

    // Reached from gate64 with r10 = state. The guest stack holds the lcall
    // return eip and cs, the thunk return address, the gate, the caller
    // return address, then the arguments.
    ".balign 16\n"
    ".hidden NativeCompatImportLanding\n"
    ".globl NativeCompatImportLanding\n"
    ".type NativeCompatImportLanding, @function\n"
    "NativeCompatImportLanding:\n"
    "    cld\n"
    "    movl %esp, %esp\n"
    "    movq %rsp, %r11\n"
    "    movq 0(%r10), %rsp\n"
    "    pushq %rbx\n"
    "    pushq %rbp\n"
    "    pushq %rsi\n"
    "    pushq %rdi\n"
    "    pushq %r10\n"
    "    pushq %r11\n"
    "    movq %r10, %rdi\n"
    "    call NativeCompatRestoreHostFs\n"
    "    movq 8(%rsp), %rdi\n"
    "    movq 0(%rsp), %rsi\n"
    "    call NativeCompatImportDispatch\n"
    "    movq %rax, %r8\n"
    "    popq %r11\n"
    "    popq %r10\n"
    "    popq %rdi\n"
    "    popq %rsi\n"
    "    popq %rbp\n"
    "    popq %rbx\n"
    "    movl $0x2B, %eax\n"
    "    movw %ax, %ds\n"
    "    movw %ax, %es\n"
    "    movzwl 16(%r10), %eax\n"
    "    movw %ax, %fs\n"
    "    movq %r11, %rsp\n"
    "    movl %r8d, %eax\n"
    "    shrq $32, %r8\n"
    "    movl %r8d, %edx\n"
    "    lretl\n"
    ".size NativeCompatImportLanding, . - NativeCompatImportLanding\n"

    // sa_sigaction entry. The kernel does not touch FS, so a signal raised in
    // guest code arrives with the guest TEB as the FS base; restore the host
    // base before any C code reads TLS or the stack-protector canary. When
    // the C handler returns 1 the guest resumes, and since rt_sigreturn does
    // not restore FS either, reload the guest selector before returning to
    // __restore_rt, which issues only the syscall.
    ".balign 16\n"
    ".hidden NativeCompatSignalEntry\n"
    ".globl NativeCompatSignalEntry\n"
    ".type NativeCompatSignalEntry, @function\n"
    "NativeCompatSignalEntry:\n"
    "    pushq %rbx\n"
    "    movq g_native_compat_active_state(%rip), %rax\n"
    "    testq %rax, %rax\n"
    "    jz 1f\n"
    "    pushq %rdi\n"
    "    pushq %rsi\n"
    "    pushq %rdx\n"
    "    subq $8, %rsp\n"
    "    movq %rax, %rdi\n"
    "    call NativeCompatRestoreHostFs\n"
    "    addq $8, %rsp\n"
    "    popq %rdx\n"
    "    popq %rsi\n"
    "    popq %rdi\n"
    "1:\n"
    "    call NativeCompatSignalHandler\n"
    "    testl %eax, %eax\n"
    "    jz 2f\n"
    "    movq g_native_compat_active_state(%rip), %rcx\n"
    "    movzwl 16(%rcx), %ecx\n"
    "    movw %cx, %fs\n"
    "2:\n"
    "    popq %rbx\n"
    "    ret\n"
    ".size NativeCompatSignalEntry, . - NativeCompatSignalEntry\n"

    ".popsection\n");
