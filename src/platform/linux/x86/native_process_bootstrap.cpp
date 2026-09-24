#include "../native_process_bootstrap.h"
#include "../native_guest_seh.h"
#include "../native_instruction_trace.h"

#include <asm/ldt.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <ucontext.h>
#include <unistd.h>

#include <array>
#include <cstddef>
#include <cstdlib>
#include <cstring>

namespace re2dj::platform::linux
{
namespace
{

NativeProcessBootstrap::Impl* g_current_bootstrap = nullptr;

bool TryDispatchGuestSeh(NativeTrapRegisters* registers);

constexpr std::uint32_t kGuestStackSize = 1024 * 1024;
constexpr std::uint32_t kSignalStackSize = 64 * 1024;
constexpr std::array<int, 5> kGuestSignals = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGTRAP};

sigjmp_buf g_guest_jump;
volatile sig_atomic_t g_guest_active = 0;
// Set by ExitNativeGuestProcess just before it jumps back to Execute.
volatile sig_atomic_t g_exit_requested = 0;
volatile std::uint32_t g_exit_code = 0;
volatile sig_atomic_t g_fault_signal = 0;
volatile sig_atomic_t g_fault_eip = 0;
volatile sig_atomic_t g_fault_esp = 0;
volatile sig_atomic_t g_fault_address = 0;
volatile sig_atomic_t g_fault_signal_code = 0;
volatile sig_atomic_t g_fault_cpu_error_code = 0;
volatile sig_atomic_t g_fault_eax = 0;
volatile sig_atomic_t g_fault_ebx = 0;
volatile sig_atomic_t g_fault_ecx = 0;
volatile sig_atomic_t g_fault_edx = 0;
volatile sig_atomic_t g_fault_esi = 0;
volatile sig_atomic_t g_fault_edi = 0;
volatile sig_atomic_t g_fault_ebp = 0;
volatile sig_atomic_t g_fault_eflags = 0;

void WriteU32(std::uint8_t* bytes, std::size_t offset, std::uint32_t value)
{
    std::memcpy(bytes + offset, &value, sizeof(value));
}

using Win32ExceptionHandlerFunction = std::uint32_t(__attribute__((cdecl)) *)(
    const Win32ExceptionRecord32* record,
    const Win32ExceptionRegistrationRecord32* frame,
    Win32Context32* context,
    void* dispatcher_context);

NativeTrapRegisters ReadTrapRegisters(const ucontext_t* context)
{
    const greg_t* registers = context->uc_mcontext.gregs;
    NativeTrapRegisters trap;
    trap.eip = static_cast<std::uint32_t>(registers[REG_EIP]);
    trap.esp = static_cast<std::uint32_t>(registers[REG_ESP]);
    trap.eax = static_cast<std::uint32_t>(registers[REG_EAX]);
    trap.ebx = static_cast<std::uint32_t>(registers[REG_EBX]);
    trap.ecx = static_cast<std::uint32_t>(registers[REG_ECX]);
    trap.edx = static_cast<std::uint32_t>(registers[REG_EDX]);
    trap.esi = static_cast<std::uint32_t>(registers[REG_ESI]);
    trap.edi = static_cast<std::uint32_t>(registers[REG_EDI]);
    trap.ebp = static_cast<std::uint32_t>(registers[REG_EBP]);
    trap.eflags = static_cast<std::uint32_t>(registers[REG_EFL]);
    trap.cs = static_cast<std::uint32_t>(registers[REG_CS]);
    trap.ss = static_cast<std::uint32_t>(registers[REG_SS]);
    trap.ds = static_cast<std::uint32_t>(registers[REG_DS]);
    trap.es = static_cast<std::uint32_t>(registers[REG_ES]);
    trap.fs = static_cast<std::uint32_t>(registers[REG_FS]);
    trap.gs = static_cast<std::uint32_t>(registers[REG_GS]);
    return trap;
}

void WriteTrapRegisters(const NativeTrapRegisters& trap, ucontext_t* context)
{
    greg_t* registers = context->uc_mcontext.gregs;
    registers[REG_EIP] = static_cast<greg_t>(trap.eip);
    registers[REG_ESP] = static_cast<greg_t>(trap.esp);
    registers[REG_EAX] = static_cast<greg_t>(trap.eax);
    registers[REG_EBX] = static_cast<greg_t>(trap.ebx);
    registers[REG_ECX] = static_cast<greg_t>(trap.ecx);
    registers[REG_EDX] = static_cast<greg_t>(trap.edx);
    registers[REG_ESI] = static_cast<greg_t>(trap.esi);
    registers[REG_EDI] = static_cast<greg_t>(trap.edi);
    registers[REG_EBP] = static_cast<greg_t>(trap.ebp);
    registers[REG_EFL] = static_cast<greg_t>(trap.eflags);
}

void GuestSignalHandler(int signal_number, siginfo_t* signal_info, void* context_pointer)
{
    if (g_guest_active == 0)
    {
        _exit(128 + signal_number);
    }
    auto* context = static_cast<ucontext_t*>(context_pointer);
    if (signal_number == SIGTRAP)
    {
        NativeTrapRegisters trap = ReadTrapRegisters(context);
        if (HandleNativeInstructionTraceTrap(&trap) || TryDispatchGuestSeh(&trap))
        {
            WriteTrapRegisters(trap, context);
            return;
        }
    }
    g_fault_signal = signal_number;
    g_fault_eip = static_cast<std::uint32_t>(context->uc_mcontext.gregs[REG_EIP]);
    g_fault_esp = static_cast<std::uint32_t>(context->uc_mcontext.gregs[REG_ESP]);
    g_fault_address = signal_info == nullptr ? 0 : static_cast<sig_atomic_t>(
        reinterpret_cast<std::uintptr_t>(signal_info->si_addr));
    g_fault_signal_code = signal_info == nullptr ? 0 : signal_info->si_code;
    g_fault_cpu_error_code = static_cast<sig_atomic_t>(context->uc_mcontext.gregs[REG_ERR]);
    g_fault_eax = static_cast<sig_atomic_t>(context->uc_mcontext.gregs[REG_EAX]);
    g_fault_ebx = static_cast<sig_atomic_t>(context->uc_mcontext.gregs[REG_EBX]);
    g_fault_ecx = static_cast<sig_atomic_t>(context->uc_mcontext.gregs[REG_ECX]);
    g_fault_edx = static_cast<sig_atomic_t>(context->uc_mcontext.gregs[REG_EDX]);
    g_fault_esi = static_cast<sig_atomic_t>(context->uc_mcontext.gregs[REG_ESI]);
    g_fault_edi = static_cast<sig_atomic_t>(context->uc_mcontext.gregs[REG_EDI]);
    g_fault_ebp = static_cast<sig_atomic_t>(context->uc_mcontext.gregs[REG_EBP]);
    g_fault_eflags = static_cast<sig_atomic_t>(context->uc_mcontext.gregs[REG_EFL]);
    siglongjmp(g_guest_jump, 1);
}

extern "C" __attribute__((naked)) std::uint32_t CallGuestEntry(
    std::uint32_t, std::uint32_t)
{
    __asm__ volatile(
        "pushl %ebp\n"
        "movl %esp, %ebp\n"
        "movl 12(%ebp), %esp\n"
        "andl $-16, %esp\n"
        "call *8(%ebp)\n"
        "movl %ebp, %esp\n"
        "popl %ebp\n"
        "ret\n");
}

extern "C" __attribute__((naked)) void CallGuestTls(
    std::uint32_t, std::uint32_t, std::uint32_t)
{
    __asm__ volatile(
        "pushl %ebp\n"
        "movl %esp, %ebp\n"
        "movl 12(%ebp), %esp\n"
        "andl $-16, %esp\n"
        "pushl $0\n"
        "pushl $1\n"
        "pushl 16(%ebp)\n"
        "call *8(%ebp)\n"
        "movl %ebp, %esp\n"
        "popl %ebp\n"
        "ret\n");
}

}  // namespace

struct NativeProcessBootstrap::Impl
{
    void* stack_mapping = nullptr;
    std::uint32_t stack_mapping_size = 0;
    std::uint32_t stack_base = 0;
    std::uint32_t stack_limit = 0;
    void* environment_mapping = nullptr;
    std::uint32_t environment_size = 0;
    std::uint32_t teb = 0;
    std::uint32_t peb = 0;
    void* signal_stack = nullptr;
    stack_t previous_signal_stack = {};
    std::array<struct sigaction, kGuestSignals.size()> previous_actions = {};
    int installed_action_count = 0;
    int tls_entry = -1;
    std::uint16_t fs_selector = 0;
    std::uint16_t previous_fs = 0;
    bool initialized = false;
    std::uint32_t image_base = 0;
    std::uint32_t seh_dispatch_count = 0;
    std::uint32_t last_seh_handler = 0;
    std::uint32_t last_seh_resumed_eip = 0;
    bool process_exited = false;
    std::uint32_t exit_code = 0;

    bool IsGuestStackRange(std::uint32_t address, std::uint32_t size) const
    {
        return address >= stack_limit && address <= stack_base && size <= stack_base - address;
    }

    ~Impl()
    {
        if (initialized)
        {
            __asm__ volatile("movw %0, %%fs" : : "rm"(previous_fs));
        }
        for (int index = installed_action_count - 1; index >= 0; --index)
        {
            sigaction(kGuestSignals[static_cast<std::size_t>(index)],
                      &previous_actions[static_cast<std::size_t>(index)], nullptr);
        }
        if (signal_stack != nullptr)
        {
            sigaltstack(&previous_signal_stack, nullptr);
            munmap(signal_stack, kSignalStackSize);
        }
        if (environment_mapping != nullptr)
        {
            munmap(environment_mapping, environment_size);
        }
        if (stack_mapping != nullptr)
        {
            munmap(stack_mapping, stack_mapping_size);
        }
        if (tls_entry >= 0)
        {
            // Only the kernel's "empty" pattern (read_exec_only and
            // seg_not_present set, all else zero) clears the slot for reuse;
            // any other not-present descriptor keeps one of the three TLS
            // GDT entries occupied.
            user_desc descriptor = {};
            descriptor.entry_number = tls_entry;
            descriptor.read_exec_only = 1;
            descriptor.seg_not_present = 1;
            syscall(SYS_set_thread_area, &descriptor);
        }
    }

    bool Initialize(std::uint32_t image_base, std::string* error)
    {
        this->image_base = image_base;
        const long page_size_value = sysconf(_SC_PAGESIZE);
        if (initialized || page_size_value <= 0)
        {
            *error = "invalid native process bootstrap state";
            return false;
        }
        const auto page_size = static_cast<std::uint32_t>(page_size_value);
        stack_mapping_size = page_size + kGuestStackSize;
        stack_mapping = mmap(nullptr, stack_mapping_size, PROT_NONE,
                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (stack_mapping == MAP_FAILED)
        {
            stack_mapping = nullptr;
            *error = "cannot allocate guarded guest stack";
            return false;
        }
        auto* stack_bytes = static_cast<std::uint8_t*>(stack_mapping);
        if (mprotect(stack_bytes + page_size, kGuestStackSize,
                     PROT_READ | PROT_WRITE) != 0)
        {
            *error = "cannot commit guest stack";
            return false;
        }
        stack_limit = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(stack_bytes + page_size));
        stack_base = stack_limit + kGuestStackSize;

        environment_size = page_size * 2;
        environment_mapping = mmap(nullptr, environment_size, PROT_READ | PROT_WRITE,
                                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (environment_mapping == MAP_FAILED)
        {
            environment_mapping = nullptr;
            *error = "cannot allocate guest TEB and PEB";
            return false;
        }
        auto* environment = static_cast<std::uint8_t*>(environment_mapping);
        teb = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(environment));
        peb = teb + page_size;
        WriteU32(environment, 0x00, 0xFFFFFFFFU);
        WriteU32(environment, 0x04, stack_base);
        WriteU32(environment, 0x08, stack_limit);
        WriteU32(environment, 0x18, teb);
        WriteU32(environment, 0x30, peb);
        WriteU32(environment + page_size, 0x08, image_base);

        user_desc descriptor = {};
        descriptor.entry_number = -1;
        descriptor.base_addr = teb;
        descriptor.limit = 0xFFFFF;
        descriptor.seg_32bit = 1;
        descriptor.limit_in_pages = 1;
        descriptor.useable = 1;
        if (syscall(SYS_set_thread_area, &descriptor) != 0)
        {
            *error = "cannot allocate guest FS descriptor";
            return false;
        }
        tls_entry = descriptor.entry_number;
        fs_selector = static_cast<std::uint16_t>((tls_entry << 3) | 3);
        __asm__ volatile("movw %%fs, %0" : "=rm"(previous_fs));

        signal_stack = mmap(nullptr, kSignalStackSize, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (signal_stack == MAP_FAILED)
        {
            signal_stack = nullptr;
            *error = "cannot allocate alternate signal stack";
            return false;
        }
        stack_t alternate = {};
        alternate.ss_sp = signal_stack;
        alternate.ss_size = kSignalStackSize;
        if (sigaltstack(&alternate, &previous_signal_stack) != 0)
        {
            *error = "cannot install alternate signal stack";
            return false;
        }
        struct sigaction action = {};
        action.sa_sigaction = &GuestSignalHandler;
        action.sa_flags = SA_SIGINFO | SA_ONSTACK;
        sigemptyset(&action.sa_mask);
        for (std::size_t index = 0; index < kGuestSignals.size(); ++index)
        {
            if (sigaction(kGuestSignals[index], &action, &previous_actions[index]) != 0)
            {
                *error = "cannot install guest fault handler";
                return false;
            }
            ++installed_action_count;
        }
        initialized = true;
        return true;
    }

    template <typename Function>
    bool Execute(Function function, NativeGuestFault* fault, std::string* error)
    {
        if (!initialized || process_exited || fault == nullptr || error == nullptr)
        {
            if (error != nullptr) *error = "invalid guest execution arguments";
            return false;
        }
        *fault = {};
        g_fault_signal = 0;
        g_fault_eip = 0;
        g_fault_esp = 0;
        g_fault_address = 0;
        g_fault_signal_code = 0;
        g_fault_cpu_error_code = 0;
        g_fault_eax = 0;
        g_fault_ebx = 0;
        g_fault_ecx = 0;
        g_fault_edx = 0;
        g_fault_esi = 0;
        g_fault_edi = 0;
        g_fault_ebp = 0;
        g_fault_eflags = 0;
        g_exit_requested = 0;
        g_current_bootstrap = this;
        if (sigsetjmp(g_guest_jump, 1) == 0)
        {
            g_guest_active = 1;
            __asm__ volatile("movw %0, %%fs" : : "rm"(fs_selector));
            function();
            __asm__ volatile("movw %0, %%fs" : : "rm"(previous_fs));
            g_guest_active = 0;
            g_current_bootstrap = nullptr;
            error->clear();
            return true;
        }
        __asm__ volatile("movw %0, %%fs" : : "rm"(previous_fs));
        g_guest_active = 0;
        g_current_bootstrap = nullptr;
        if (g_exit_requested != 0)
        {
            // The guest ended its process from an import; that is a normal
            // completion, not a fault.
            g_exit_requested = 0;
            process_exited = true;
            exit_code = g_exit_code;
            error->clear();
            return true;
        }
        fault->status_code = static_cast<std::uint32_t>(g_fault_signal);
        fault->instruction_pointer = g_fault_eip;
        fault->stack_pointer = g_fault_esp;
        fault->fault_address = static_cast<std::uint32_t>(g_fault_address);
        fault->signal_code = static_cast<std::uint32_t>(g_fault_signal_code);
        fault->cpu_error_code = static_cast<std::uint32_t>(g_fault_cpu_error_code);
        fault->eax = static_cast<std::uint32_t>(g_fault_eax);
        fault->ebx = static_cast<std::uint32_t>(g_fault_ebx);
        fault->ecx = static_cast<std::uint32_t>(g_fault_ecx);
        fault->edx = static_cast<std::uint32_t>(g_fault_edx);
        fault->esi = static_cast<std::uint32_t>(g_fault_esi);
        fault->edi = static_cast<std::uint32_t>(g_fault_edi);
        fault->ebp = static_cast<std::uint32_t>(g_fault_ebp);
        fault->eflags = static_cast<std::uint32_t>(g_fault_eflags);
        error->clear();
        return false;
    }
};

namespace
{

bool TryDispatchGuestSeh(NativeTrapRegisters* registers)
{
    NativeProcessBootstrap::Impl* bootstrap = g_current_bootstrap;
    if (bootstrap == nullptr)
    {
        return false;
    }
    NativeGuestSehDispatch dispatch;
    if (!PrepareNativeGuestBreakpointDispatch(*registers,
                                              bootstrap->teb,
                                              bootstrap->image_base,
                                              bootstrap->stack_limit,
                                              bootstrap->stack_base,
                                              &dispatch))
    {
        return false;
    }

    auto handler_fn = reinterpret_cast<Win32ExceptionHandlerFunction>(
        static_cast<std::uintptr_t>(dispatch.frame.handler));

    std::uint16_t current_fs = 0;
    __asm__ volatile("movw %%fs, %0" : "=rm"(current_fs));
    if (current_fs != bootstrap->fs_selector)
    {
        __asm__ volatile("movw %0, %%fs" : : "rm"(bootstrap->fs_selector));
    }

    const std::uint32_t disposition =
        handler_fn(&dispatch.record,
                   reinterpret_cast<const Win32ExceptionRegistrationRecord32*>(
                       static_cast<std::uintptr_t>(dispatch.frame_address)),
                   &dispatch.context,
                   nullptr);

    if (current_fs != bootstrap->fs_selector)
    {
        __asm__ volatile("movw %0, %%fs" : : "rm"(current_fs));
    }

    if (disposition != kExceptionContinueExecution)
    {
        return false;
    }
    ApplyNativeGuestSehContext(dispatch.context, registers);
    ++bootstrap->seh_dispatch_count;
    bootstrap->last_seh_handler = dispatch.frame.handler;
    bootstrap->last_seh_resumed_eip = dispatch.context.eip;
    return true;
}

}  // namespace

NativeProcessBootstrap::NativeProcessBootstrap() : impl_(new Impl) {}
NativeProcessBootstrap::~NativeProcessBootstrap() { delete impl_; }

bool NativeProcessBootstrap::Initialize(std::uint32_t image_base, std::string* error)
{
    return error != nullptr && impl_->Initialize(image_base, error);
}

bool NativeProcessBootstrap::RunTlsCallback(std::uint32_t callback,
                                            std::uint32_t image_base,
                                            NativeGuestFault* fault,
                                            std::string* error)
{
    return impl_->Execute(
        [&]() { CallGuestTls(callback, impl_->stack_base, image_base); }, fault, error);
}

bool NativeProcessBootstrap::RunEntry(std::uint32_t entry,
                                      std::uint32_t* result,
                                      NativeGuestFault* fault,
                                      std::string* error)
{
    if (result == nullptr)
    {
        if (error != nullptr) *error = "guest entry result is required";
        return false;
    }
    if (!impl_->Execute(
            [&]() { *result = CallGuestEntry(entry, impl_->stack_base); }, fault, error))
    {
        return false;
    }
    if (impl_->process_exited)
    {
        *result = impl_->exit_code;
    }
    return true;
}

std::uint32_t NativeProcessBootstrap::GuestStackBase() const
{
    return impl_ == nullptr ? 0 : impl_->stack_base;
}

std::uint32_t NativeProcessBootstrap::GuestStackLimit() const
{
    return impl_ == nullptr ? 0 : impl_->stack_limit;
}

bool NativeProcessBootstrap::IsGuestStackRange(std::uint32_t address, std::uint32_t size) const
{
    return impl_ != nullptr && address >= impl_->stack_limit && address <= impl_->stack_base &&
           size <= impl_->stack_base - address;
}

std::uint32_t NativeProcessBootstrap::Teb() const
{
    return impl_ == nullptr ? 0 : impl_->teb;
}

std::uint32_t NativeProcessBootstrap::SehDispatchCount() const
{
    return impl_ == nullptr ? 0 : impl_->seh_dispatch_count;
}

std::uint32_t NativeProcessBootstrap::LastSehHandler() const
{
    return impl_ == nullptr ? 0 : impl_->last_seh_handler;
}

std::uint32_t NativeProcessBootstrap::LastSehResumedEip() const
{
    return impl_ == nullptr ? 0 : impl_->last_seh_resumed_eip;
}

bool NativeProcessBootstrap::GuestProcessExited() const
{
    return impl_ != nullptr && impl_->process_exited;
}

std::uint32_t NativeProcessBootstrap::GuestExitCode() const
{
    return impl_ == nullptr ? 0 : impl_->exit_code;
}

void ExitNativeGuestProcess(std::uint32_t exit_code)
{
    // Called from the import bridge, which runs host code on the guest stack
    // with the guest FS; glibc i386 keeps TLS in GS, and Execute restores FS
    // once the jump lands.
    if (g_current_bootstrap == nullptr || g_guest_active == 0)
    {
        std::abort();
    }
    g_exit_code = exit_code;
    g_exit_requested = 1;
    siglongjmp(g_guest_jump, 1);
}

}  // namespace re2dj::platform::linux
