#include <errno.h>
#include <signal.h>
#include <sys/mman.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

#include "native_compat_mode.h"

namespace native_platform = re2dj::platform::native;

extern "C"
{
std::uint32_t g_probe_callee_saved_mismatches = 0;
std::uint64_t ProbeEnterWithCalleeSavedCheck(native_platform::NativeCompatTransitionState* state,
                                             std::uint32_t entry,
                                             std::uint32_t guest_stack_pointer);
}

// Loads distinct values into every host callee-saved register, enters the
// guest, and counts the registers that did not survive.
__asm__(
    ".pushsection .text\n"
    ".balign 16\n"
    ".globl ProbeEnterWithCalleeSavedCheck\n"
    ".hidden ProbeEnterWithCalleeSavedCheck\n"
    ".type ProbeEnterWithCalleeSavedCheck, @function\n"
    "ProbeEnterWithCalleeSavedCheck:\n"
    "    pushq %rbp\n"
    "    pushq %rbx\n"
    "    pushq %r12\n"
    "    pushq %r13\n"
    "    pushq %r14\n"
    "    pushq %r15\n"
    "    subq $8, %rsp\n"
    "    movabsq $0x1111111111111111, %rbx\n"
    "    movabsq $0x2222222222222222, %rbp\n"
    "    movabsq $0x3333333333333333, %r12\n"
    "    movabsq $0x4444444444444444, %r13\n"
    "    movabsq $0x5555555555555555, %r14\n"
    "    movabsq $0x6666666666666666, %r15\n"
    "    call NativeCompatEnterGuest\n"
    "    xorl %ecx, %ecx\n"
    "    movabsq $0x1111111111111111, %r11\n"
    "    cmpq %r11, %rbx\n"
    "    setne %dl\n"
    "    movzbl %dl, %edx\n"
    "    addl %edx, %ecx\n"
    "    movabsq $0x2222222222222222, %r11\n"
    "    cmpq %r11, %rbp\n"
    "    setne %dl\n"
    "    movzbl %dl, %edx\n"
    "    addl %edx, %ecx\n"
    "    movabsq $0x3333333333333333, %r11\n"
    "    cmpq %r11, %r12\n"
    "    setne %dl\n"
    "    movzbl %dl, %edx\n"
    "    addl %edx, %ecx\n"
    "    movabsq $0x4444444444444444, %r11\n"
    "    cmpq %r11, %r13\n"
    "    setne %dl\n"
    "    movzbl %dl, %edx\n"
    "    addl %edx, %ecx\n"
    "    movabsq $0x5555555555555555, %r11\n"
    "    cmpq %r11, %r14\n"
    "    setne %dl\n"
    "    movzbl %dl, %edx\n"
    "    addl %edx, %ecx\n"
    "    movabsq $0x6666666666666666, %r11\n"
    "    cmpq %r11, %r15\n"
    "    setne %dl\n"
    "    movzbl %dl, %edx\n"
    "    addl %edx, %ecx\n"
    "    movl %ecx, g_probe_callee_saved_mismatches(%rip)\n"
    "    addq $8, %rsp\n"
    "    popq %r15\n"
    "    popq %r14\n"
    "    popq %r13\n"
    "    popq %r12\n"
    "    popq %rbx\n"
    "    popq %rbp\n"
    "    ret\n"
    ".size ProbeEnterWithCalleeSavedCheck, . - ProbeEnterWithCalleeSavedCheck\n"
    ".popsection\n");

namespace
{

thread_local std::uint32_t g_host_marker = 0x5A5AA5A5U;

constexpr std::uint32_t kReturnConstant = 0x000;
constexpr std::uint32_t kReadFs = 0x010;
constexpr std::uint32_t kImportThunk = 0x020;
constexpr std::uint32_t kCallImports = 0x040;
constexpr std::uint32_t kInvalidOpcode = 0x080;
constexpr std::uint32_t kBreakpoint = 0x0A0;
constexpr std::uint32_t kClobberRegisters = 0x0B0;
constexpr std::uint32_t kSehGuest = 0x0D0;
constexpr std::uint32_t kSehHandler = 0x100;
constexpr std::uint32_t kSyntheticGate = 0x00001234U;

class CodeWriter
{
public:
    CodeWriter(std::uint8_t* base, std::uint32_t base_address, std::uint32_t offset)
        : base_(base), base_address_(base_address), offset_(offset)
    {
    }

    CodeWriter& Bytes(std::initializer_list<std::uint8_t> bytes)
    {
        for (std::uint8_t byte : bytes)
        {
            base_[offset_++] = byte;
        }
        return *this;
    }

    CodeWriter& U32(std::uint32_t value)
    {
        std::memcpy(base_ + offset_, &value, sizeof(value));
        offset_ += sizeof(value);
        return *this;
    }

    // E8 rel32 to an absolute guest address.
    CodeWriter& Call(std::uint32_t target)
    {
        Bytes({0xE8});
        return U32(target - (base_address_ + offset_ + 4));
    }

private:
    std::uint8_t* base_;
    std::uint32_t base_address_;
    std::uint32_t offset_;
};

struct ImportContext
{
    std::uint32_t code = 0;
    std::uint32_t calls = 0;
    bool event_valid = true;
};

bool CompleteSyntheticImport(const native_platform::NativeImportGateEvent& event,
                             native_platform::NativeImportGateResult* result,
                             void* opaque)
{
    auto* context = static_cast<ImportContext*>(opaque);
    const std::uint32_t expected_return =
        context->code + kCallImports + (context->calls == 0 ? 9U : 15U);
    std::uint32_t argument = 0;
    std::memcpy(&argument,
                reinterpret_cast<const void*>(
                    static_cast<std::uintptr_t>(event.stack_pointer + 4)),
                sizeof(argument));
    if (event.gate_address != kSyntheticGate || event.instruction_pointer != expected_return ||
        event.stack_pointer < event.guest_stack_limit ||
        event.stack_pointer >= event.guest_stack_base)
    {
        context->event_valid = false;
    }
    result->stack_bytes_to_pop = sizeof(argument);
    if (context->calls == 0 && argument == 41)
    {
        result->eax = 42;
        ++context->calls;
        return true;
    }
    if (context->calls == 1 && argument == 42)
    {
        result->eax = 43;
        result->edx = 1;
        ++context->calls;
        return true;
    }
    context->event_valid = false;
    return false;
}

class Probe
{
public:
    explicit Probe(const char* label) : label_(label) {}

    void Check(bool condition, const char* name)
    {
        std::printf("[%s] %-44s %s\n", label_, name, condition ? "ok" : "FAILED");
        if (!condition)
        {
            ++failures_;
        }
    }

    int failures() const { return failures_; }

private:
    const char* label_;
    int failures_ = 0;
};

bool ReturnsConstant(native_platform::NativeCompatModeRuntime* runtime, std::uint32_t code)
{
    native_platform::NativeCompatModeCall call;
    call.entry = code + kReturnConstant;
    native_platform::NativeCompatModeRunResult result;
    native_platform::NativeGuestFault fault;
    std::string error;
    return runtime->Run(call, &result, &fault, &error) && result.eax == 0x12345678U;
}

int RunProbe(bool force_arch_prctl)
{
    native_platform::NativeCompatModeRuntime runtime;
    native_platform::NativeCompatModeOptions options;
    options.force_arch_prctl = force_arch_prctl;
    std::string error;
    if (!runtime.Initialize(0x00400000U, options, &error))
    {
        std::printf("compatibility-mode runtime initialization failed: %s\n", error.c_str());
        return 1;
    }
    const char* label = runtime.UsesFsGsBase() ? "fsgsbase" : "arch_prctl";
    Probe probe(label);
    std::printf("[%s] teb=0x%08x stack=0x%08x-0x%08x fs=0x%04x bridge=0x%08x\n", label,
                runtime.Teb(), runtime.GuestStackLimit(), runtime.GuestStackBase(),
                runtime.GuestFsSelector(), runtime.ImportBridgeAddress());
    if (force_arch_prctl)
    {
        probe.Check(!runtime.UsesFsGsBase(), "forced arch_prctl path is selected");
    }

    native_platform::NativeLowMemory code_page;
    native_platform::NativeLowMemory scratch_page;
    if (!native_platform::MapNativeLowMemory(4096, native_platform::HostProtection::kReadWrite, &code_page, &error) ||
        !native_platform::MapNativeLowMemory(4096, native_platform::HostProtection::kReadWrite, &scratch_page, &error))
    {
        std::printf("cannot map probe pages: %s\n", error.c_str());
        return 1;
    }
    auto* bytes = static_cast<std::uint8_t*>(code_page.memory);
    const std::uint32_t code = code_page.address;
    std::memset(bytes, 0xCC, code_page.size);

    // mov eax, 0x12345678; ret
    CodeWriter(bytes, code, kReturnConstant).Bytes({0xB8}).U32(0x12345678U).Bytes({0xC3});
    // mov eax, fs:[0x18]; mov edx, fs:[0]; ret
    CodeWriter(bytes, code, kReadFs)
        .Bytes({0x64, 0xA1}).U32(0x18)
        .Bytes({0x64, 0x8B, 0x15}).U32(0)
        .Bytes({0xC3});
    // The i386 import-thunk shape: push gate; call bridge; pop ecx;
    // add esp, [cleanup]; jmp ecx
    CodeWriter(bytes, code, kImportThunk)
        .Bytes({0x68}).U32(kSyntheticGate)
        .Call(runtime.ImportBridgeAddress())
        .Bytes({0x59, 0x03, 0x25}).U32(runtime.ImportCleanupAddress())
        .Bytes({0xFF, 0xE1});
    // mov edi, esp; push 41; call thunk; push eax; call thunk;
    // sub edi, esp; mov [scratch], edi; ret
    CodeWriter(bytes, code, kCallImports)
        .Bytes({0x89, 0xE7, 0x6A, 0x29})
        .Call(code + kImportThunk)
        .Bytes({0x50})
        .Call(code + kImportThunk)
        .Bytes({0x29, 0xE7, 0x89, 0x3D}).U32(scratch_page.address)
        .Bytes({0xC3});
    // mov eax, 0x11111111; mov ebx, 0x22222222; ud2
    CodeWriter(bytes, code, kInvalidOpcode)
        .Bytes({0xB8}).U32(0x11111111U)
        .Bytes({0xBB}).U32(0x22222222U)
        .Bytes({0x0F, 0x0B});
    // int3; ret
    CodeWriter(bytes, code, kBreakpoint).Bytes({0xCC, 0xC3});
    // Overwrite every register the host treats as callee-saved, then return.
    CodeWriter(bytes, code, kClobberRegisters)
        .Bytes({0xBB}).U32(0xDEADBEEFU)
        .Bytes({0xBD}).U32(0xDEADBEEFU)
        .Bytes({0xBE}).U32(0xDEADBEEFU)
        .Bytes({0xBF}).U32(0xDEADBEEFU)
        .Bytes({0xB8}).U32(0x0000600DU)
        .Bytes({0xC3});
    // push handler; push fs:[0]; mov fs:[0], esp; mov eax, 0x11111111; int3;
    // then, once the handler resumes past the int3: mov ecx, [esp];
    // mov fs:[0], ecx; add esp, 8; ret
    CodeWriter(bytes, code, kSehGuest)
        .Bytes({0x68}).U32(code + kSehHandler)
        .Bytes({0x64, 0xFF, 0x35}).U32(0)
        .Bytes({0x64, 0x89, 0x25}).U32(0)
        .Bytes({0xB8}).U32(0x11111111U)
        .Bytes({0xCC})
        .Bytes({0x8B, 0x0C, 0x24})
        .Bytes({0x64, 0x89, 0x0D}).U32(0)
        .Bytes({0x83, 0xC4, 0x08, 0xC3});
    // The handler: mov eax, [esp+12] (CONTEXT); inc dword [eax+0xB8] (Eip);
    // mov dword [eax+0xB0], 0x5EC0 (Eax); xor eax, eax
    // (ExceptionContinueExecution); ret
    CodeWriter(bytes, code, kSehHandler)
        .Bytes({0x8B, 0x44, 0x24, 0x0C})
        .Bytes({0xFF, 0x80}).U32(0xB8)
        .Bytes({0xC7, 0x80}).U32(0xB0).U32(0x5EC0)
        .Bytes({0x31, 0xC0, 0xC3});
    if (mprotect(code_page.memory, code_page.size, PROT_READ | PROT_EXEC) != 0)
    {
        std::printf("cannot protect probe code\n");
        return 1;
    }

    native_platform::NativeCompatModeCall call;
    native_platform::NativeCompatModeRunResult result;
    native_platform::NativeGuestFault fault;

    // 1. Return value and host state.
    const std::uint64_t fs_before = native_platform::ReadNativeHostFsBase(runtime.UsesFsGsBase());
    g_host_marker = 0x5A5AA5A5U;
    errno = 1234;
    call.entry = code + kReturnConstant;
    bool ran = runtime.Run(call, &result, &fault, &error);
    const int errno_after = errno;
    probe.Check(ran && result.eax == 0x12345678U, "1 guest return value");
    probe.Check(g_host_marker == 0x5A5AA5A5U && errno_after == 1234,
                "1 host thread_local and errno survive");
    probe.Check(native_platform::ReadNativeHostFsBase(runtime.UsesFsGsBase()) == fs_before,
                "1 host FS base restored");

    // 2. Guest FS points at the TEB.
    call.entry = code + kReadFs;
    ran = runtime.Run(call, &result, &fault, &error);
    probe.Check(ran && result.eax == runtime.Teb(), "2 fs:[0x18] is the TEB self pointer");
    probe.Check(ran && result.edx == 0xFFFFFFFFU, "2 fs:[0] is the empty SEH chain");

    // 3. Two stdcall imports through the handler.
    ImportContext imports;
    imports.code = code;
    std::memset(scratch_page.memory, 0xFF, 4);
    call.entry = code + kCallImports;
    call.handler = &CompleteSyntheticImport;
    call.handler_context = &imports;
    ran = runtime.Run(call, &result, &fault, &error);
    std::uint32_t stack_delta = 0xFFFFFFFFU;
    std::memcpy(&stack_delta, scratch_page.memory, sizeof(stack_delta));
    probe.Check(ran && imports.calls == 2 && result.eax == 43 && result.edx == 1,
                "3 imports return edx:eax through the gate");
    probe.Check(imports.event_valid, "3 import events carry gate, eip, and esp");
    probe.Check(ran && stack_delta == 0, "3 guest esp balances after stdcall cleanup");
    call.handler = nullptr;
    call.handler_context = nullptr;

    // 4. A guest SIGILL is captured and the runtime stays usable.
    call.entry = code + kInvalidOpcode;
    ran = runtime.Run(call, &result, &fault, &error);
    probe.Check(!ran && error.empty() && fault.status_code == SIGILL &&
                    fault.instruction_pointer == code + kInvalidOpcode + 10,
                "4 ud2 reports SIGILL at the guest eip");
    probe.Check(fault.stack_pointer == result.entry_stack_pointer &&
                    fault.eax == 0x11111111U && fault.ebx == 0x22222222U,
                "4 fault carries guest esp and registers");
    probe.Check(native_platform::ReadNativeHostFsBase(runtime.UsesFsGsBase()) == fs_before &&
                    g_host_marker == 0x5A5AA5A5U,
                "4 host FS base restored after the fault");
    probe.Check(ReturnsConstant(&runtime, code), "4 runtime runs again after the fault");

    // 5. An int3 no SEH handler takes is reported as SIGTRAP at the
    // breakpoint byte, the exception address Windows gives it.
    call.entry = code + kBreakpoint;
    ran = runtime.Run(call, &result, &fault, &error);
    probe.Check(!ran && fault.status_code == SIGTRAP &&
                    fault.instruction_pointer == code + kBreakpoint,
                "5 int3 reports SIGTRAP at the breakpoint");
    probe.Check(ReturnsConstant(&runtime, code), "5 runtime runs again after the trap");

    // 6. Host callee-saved registers survive a guest that clobbers them.
    g_probe_callee_saved_mismatches = 0xFFFFFFFFU;
    call.entry = code + kClobberRegisters;
    call.enter = &ProbeEnterWithCalleeSavedCheck;
    ran = runtime.Run(call, &result, &fault, &error);
    probe.Check(ran && result.eax == 0x600DU && g_probe_callee_saved_mismatches == 0,
                "6 host callee-saved registers preserved");
    call.enter = nullptr;

    // 7. A guest INT3 reaches the guest's own SEH handler, which edits
    // CONTEXT; the guest resumes with the edits and unwinds its frame.
    call.entry = code + kSehGuest;
    ran = runtime.Run(call, &result, &fault, &error);
    std::uint32_t exception_list = 0;
    std::memcpy(&exception_list,
                reinterpret_cast<const void*>(static_cast<std::uintptr_t>(runtime.Teb())),
                sizeof(exception_list));
    probe.Check(ran && result.eax == 0x5EC0U && runtime.SehDispatchCount() == 1,
                "7 SEH handler edits CONTEXT and guest resumes");
    probe.Check(runtime.LastSehHandler() == code + kSehHandler &&
                    runtime.LastSehResumedEip() == code + kSehGuest + 25,
                "7 SEH handler and resume address recorded");
    probe.Check(exception_list == 0xFFFFFFFFU &&
                    native_platform::ReadNativeHostFsBase(runtime.UsesFsGsBase()) == fs_before &&
                    g_host_marker == 0x5A5AA5A5U,
                "7 SEH chain unwound and host state intact");
    probe.Check(ReturnsConstant(&runtime, code), "7 runtime runs again after SEH");

    native_platform::ReleaseNativeLowMemory(&scratch_page);
    native_platform::ReleaseNativeLowMemory(&code_page);
    return probe.failures();
}

}  // namespace

int main()
{
    int failures = RunProbe(false);
    failures += RunProbe(true);
    if (failures != 0)
    {
        std::printf("compatibility-mode probe failed: %d check(s)\n", failures);
        return 1;
    }
    std::printf("compatibility-mode probe passed\n");
    return 0;
}
