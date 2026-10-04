#include "../../native/native_process_bootstrap.h"
#include "../native_signal_fault.h"
#include "../../native/native_guest_seh.h"
#include "../../native/native_guest_threads.h"
#include "../../native/native_import_bridge.h"
#include "../../native/native_instruction_trace.h"
#include "../../native/native_legacy_io.h"

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
#include <thread>

namespace re2dj::platform::native
{
namespace
{

// Each guest thread runs on a host thread of its own, so the run state the
// signal handler and ExitNativeGuestProcess reach is per thread. glibc i386
// keeps TLS in GS, which the trampolines restore before any C code runs.
thread_local NativeProcessBootstrap::Impl* g_current_bootstrap = nullptr;

bool TryDeliverGuestException(int signal_number,
                              const siginfo_t* signal_info,
                              const ucontext_t* context,
                              NativeTrapRegisters* registers);
void ReportUnhandledGuestException(const NativeTrapRegisters& registers);

constexpr std::uint32_t kGuestStackSize = 1024 * 1024;
constexpr std::uint32_t kSignalStackSize = 64 * 1024;
constexpr std::array<int, 5> kGuestSignals = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGTRAP};

thread_local sigjmp_buf g_guest_jump;
thread_local volatile sig_atomic_t g_guest_active = 0;
// Set by ExitNativeGuestProcess just before it jumps back to Execute.
thread_local volatile sig_atomic_t g_exit_requested = 0;
thread_local volatile std::uint32_t g_exit_code = 0;
thread_local volatile sig_atomic_t g_fault_signal = 0;
thread_local volatile sig_atomic_t g_fault_eip = 0;
thread_local volatile sig_atomic_t g_fault_esp = 0;
thread_local volatile sig_atomic_t g_fault_address = 0;
thread_local volatile sig_atomic_t g_fault_signal_code = 0;
thread_local volatile sig_atomic_t g_fault_cpu_error_code = 0;
thread_local volatile sig_atomic_t g_fault_eax = 0;
thread_local volatile sig_atomic_t g_fault_ebx = 0;
thread_local volatile sig_atomic_t g_fault_ecx = 0;
thread_local volatile sig_atomic_t g_fault_edx = 0;
thread_local volatile sig_atomic_t g_fault_esi = 0;
thread_local volatile sig_atomic_t g_fault_edi = 0;
thread_local volatile sig_atomic_t g_fault_ebp = 0;
thread_local volatile sig_atomic_t g_fault_eflags = 0;

std::uint16_t QueryCurrentGs()
{
    std::uint16_t selector = 0;
    __asm__ volatile("movw %%gs, %0" : "=rm"(selector));
    return selector;
}

extern "C"
{
std::uint16_t g_native_host_gs_selector = QueryCurrentGs();
}

void WriteU32(std::uint8_t* bytes, std::size_t offset, std::uint32_t value)
{
    std::memcpy(bytes + offset, &value, sizeof(value));
}

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

extern "C" void GuestSignalHandler(int signal_number, siginfo_t* signal_info, void* context_pointer)
{
    if (g_guest_active == 0)
    {
        _exit(128 + signal_number);
    }
    auto* context = static_cast<ucontext_t*>(context_pointer);
    NativeTrapRegisters trap = ReadTrapRegisters(context);
    if ((signal_number == SIGTRAP && HandleNativeInstructionTraceTrap(&trap)) ||
        (signal_number == SIGSEGV && HandleNativeLegacyIoTrap(&trap)) ||
        TryDeliverGuestException(signal_number, signal_info, context, &trap))
    {
        WriteTrapRegisters(trap, context);
        return;
    }
    // A guest exception no handler continued: report the fault it came from.
    ReportUnhandledGuestException(trap);
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

extern "C" __attribute__((naked)) void GuestSignalTrampoline(
    int, siginfo_t*, void*)
{
    __asm__ volatile(
        "pushl %eax\n"
        "call 1f\n"
        "1:\n"
        "popl %eax\n"
        "movw (g_native_host_gs_selector - 1b)(%eax), %ax\n"
        "testw %ax, %ax\n"
        "jz 2f\n"
        "movw %ax, %gs\n"
        "2:\n"
        "popl %eax\n"
        "jmp GuestSignalHandler\n");
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
        "pushl %eax\n"
        "call 1f\n"
        "1:\n"
        "popl %eax\n"
        "movw (g_native_host_gs_selector - 1b)(%eax), %ax\n"
        "testw %ax, %ax\n"
        "jz 2f\n"
        "movw %ax, %gs\n"
        "2:\n"
        "popl %eax\n"
        "movl %ebp, %esp\n"
        "popl %ebp\n"
        "ret\n");
}

// Calls a ThreadProc(parameter) on a thread's own guest stack.
extern "C" __attribute__((naked)) std::uint32_t CallGuestThread(
    std::uint32_t, std::uint32_t, std::uint32_t)
{
    __asm__ volatile(
        "pushl %ebp\n"
        "movl %esp, %ebp\n"
        "movl 12(%ebp), %esp\n"
        "andl $-16, %esp\n"
        "pushl 16(%ebp)\n"
        "call *8(%ebp)\n"
        "pushl %eax\n"
        "call 1f\n"
        "1:\n"
        "popl %eax\n"
        "movw (g_native_host_gs_selector - 1b)(%eax), %ax\n"
        "testw %ax, %ax\n"
        "jz 2f\n"
        "movw %ax, %gs\n"
        "2:\n"
        "popl %eax\n"
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
        "call 1f\n"
        "1:\n"
        "popl %ecx\n"
        "movw (g_native_host_gs_selector - 1b)(%ecx), %cx\n"
        "testw %cx, %cx\n"
        "jz 2f\n"
        "movw %cx, %gs\n"
        "2:\n"
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
    NativeGuestExceptionDispatcher exceptions;
    // The signal of the exception last delivered, reported if it goes unhandled.
    int delivered_signal = 0;
    std::uint32_t delivered_signal_code = 0;
    std::uint32_t delivered_cpu_error = 0;
    std::uint32_t delivered_fault_address = 0;
    bool process_exited = false;
    std::uint32_t exit_code = 0;
    // The main guest thread, whose stack and TEB are the ones above.
    NativeGuestThread main_thread;

    bool IsGuestStackRange(std::uint32_t address, std::uint32_t size) const
    {
        return (address >= stack_limit && address <= stack_base && size <= stack_base - address) ||
               NativeGuestThreadMemoryContains(address, size);
    }

    ~Impl()
    {
        if (initialized)
        {
            EndNativeGuestThreads();
            __asm__ volatile("movw %0, %%fs" : : "rm"(previous_fs));
        }
        for (int index = installed_action_count - 1; index >= 0; --index)
        {
            sigaction(kGuestSignals[static_cast<std::size_t>(index)],
                      &previous_actions[static_cast<std::size_t>(index)], nullptr);
        }
        ReleaseNativeGuestExceptionDispatcher(&exceptions);
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
        action.sa_sigaction = reinterpret_cast<void (*)(int, siginfo_t*, void*)>(&GuestSignalTrampoline);
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
        g_native_host_gs_selector = QueryCurrentGs();
        if (!CreateNativeGuestExceptionDispatcher(&exceptions, error))
        {
            return false;
        }
        main_thread.teb = teb;
        main_thread.stack_limit = stack_limit;
        main_thread.stack_base = stack_base;
        BeginNativeGuestThreads(&main_thread);
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
        ResetNativeImportGateNesting();
        // The calling host thread's own FS, restored whichever way the run ends.
        std::uint16_t host_fs = 0;
        __asm__ volatile("movw %%fs, %0" : "=rm"(host_fs));
        static thread_local std::uint16_t saved_host_fs = 0;
        saved_host_fs = host_fs;
        if (sigsetjmp(g_guest_jump, 1) == 0)
        {
            g_guest_active = 1;
            __asm__ volatile("movw %0, %%fs" : : "rm"(fs_selector));
            function();
            __asm__ volatile("movw %0, %%fs" : : "rm"(saved_host_fs));
            __asm__ volatile("movw %0, %%gs" : : "rm"(g_native_host_gs_selector));
            g_guest_active = 0;
            g_current_bootstrap = nullptr;
            error->clear();
            return true;
        }
        __asm__ volatile("movw %0, %%fs" : : "rm"(saved_host_fs));
        __asm__ volatile("movw %0, %%gs" : : "rm"(g_native_host_gs_selector));
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
        fault->kind = NativeFaultKindFromSignal(static_cast<int>(g_fault_signal));
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

// Delivers a guest fault to the guest's SEH chain through the dispatcher.
// Host code running for an import shares the guest's mode and stack here, so
// its faults, and the host stop stub's INT3s, are never delivered.
bool TryDeliverGuestException(int signal_number,
                              const siginfo_t* signal_info,
                              const ucontext_t* context,
                              NativeTrapRegisters* registers)
{
    NativeProcessBootstrap::Impl* bootstrap = g_current_bootstrap;
    if (bootstrap == nullptr || NativeHostCodeRunning() || registers->eip == bootstrap->exceptions.stop ||
        IsNativeHostTrap(registers->eip) || IsNativeHostTrap(registers->eip - 1))
    {
        return false;
    }
    NativeGuestTrapCause cause;
    cause.trap_number = static_cast<std::uint32_t>(context->uc_mcontext.gregs[REG_TRAPNO]);
    cause.error_code = static_cast<std::uint32_t>(context->uc_mcontext.gregs[REG_ERR]);
    cause.fault_address = signal_info == nullptr ? 0 : static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(signal_info->si_addr));
    Win32ExceptionRecord32 record;
    Win32Context32 guest_context;
    const NativeGuestThread* thread = CurrentNativeGuestThread();
    const std::uint32_t stack_limit = thread != nullptr ? thread->stack_limit : bootstrap->stack_limit;
    const std::uint32_t stack_base = thread != nullptr ? thread->stack_base : bootstrap->stack_base;
    if (!DescribeNativeGuestException(cause, *registers, &record, &guest_context) ||
        !DeliverNativeGuestException(&bootstrap->exceptions,
                                     stack_limit,
                                     stack_base,
                                     record,
                                     guest_context,
                                     registers))
    {
        return false;
    }
    bootstrap->delivered_signal = signal_number;
    bootstrap->delivered_signal_code = signal_info == nullptr ? 0 : static_cast<std::uint32_t>(signal_info->si_code);
    bootstrap->delivered_cpu_error = cause.error_code;
    bootstrap->delivered_fault_address = cause.fault_address;
    return true;
}

// Replaces the dispatcher's stop with the fault the unhandled exception came
// from, so it is reported as that fault.
void ReportUnhandledGuestException(const NativeTrapRegisters& registers)
{
    NativeProcessBootstrap::Impl* bootstrap = g_current_bootstrap;
    Win32ExceptionRecord32 record;
    Win32Context32 context;
    if (bootstrap == nullptr ||
        !ReadNativeGuestUnhandledException(bootstrap->exceptions, registers, &record, &context))
    {
        return;
    }
    g_fault_signal = bootstrap->delivered_signal;
    g_fault_eip = static_cast<sig_atomic_t>(record.exception_address);
    g_fault_esp = static_cast<sig_atomic_t>(context.esp);
    g_fault_address = static_cast<sig_atomic_t>(bootstrap->delivered_fault_address);
    g_fault_signal_code = static_cast<sig_atomic_t>(bootstrap->delivered_signal_code);
    g_fault_cpu_error_code = static_cast<sig_atomic_t>(bootstrap->delivered_cpu_error);
    g_fault_eax = static_cast<sig_atomic_t>(context.eax);
    g_fault_ebx = static_cast<sig_atomic_t>(context.ebx);
    g_fault_ecx = static_cast<sig_atomic_t>(context.ecx);
    g_fault_edx = static_cast<sig_atomic_t>(context.edx);
    g_fault_esi = static_cast<sig_atomic_t>(context.esi);
    g_fault_edi = static_cast<sig_atomic_t>(context.edi);
    g_fault_ebp = static_cast<sig_atomic_t>(context.ebp);
    g_fault_eflags = static_cast<sig_atomic_t>(context.eflags);
    siglongjmp(g_guest_jump, 1);
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
    return ExceptionCounters().resumed;
}

std::uint32_t NativeProcessBootstrap::LastSehHandler() const
{
    return ExceptionCounters().last_handler;
}

std::uint32_t NativeProcessBootstrap::LastSehResumedEip() const
{
    return ExceptionCounters().last_resumed_eip;
}

NativeGuestExceptionCounters NativeProcessBootstrap::ExceptionCounters() const
{
    return impl_ == nullptr ? NativeGuestExceptionCounters{} : ReadNativeGuestExceptionCounters(impl_->exceptions);
}

bool NativeProcessBootstrap::GuestProcessExited() const
{
    return impl_ != nullptr && impl_->process_exited;
}

std::uint32_t NativeProcessBootstrap::GuestExitCode() const
{
    return impl_ == nullptr ? 0 : impl_->exit_code;
}

namespace
{

constexpr std::uint32_t kThreadStackSize = 1024 * 1024;

// A guest thread other than the main one: its guest stack, TEB page, and
// alternate signal stack, which outlive it only when it ended the process.
struct SecondaryThread
{
    NativeGuestThread thread;
    NativeGuestThreadStart start;
    NativeProcessBootstrap::Impl* bootstrap = nullptr;
    NativeImportGateHandler handler = nullptr;
    void* handler_context = nullptr;
    void* stack_mapping = nullptr;
    std::uint32_t stack_mapping_size = 0;
    void* teb_mapping = nullptr;
    std::uint32_t teb_mapping_size = 0;
    void* signal_stack = nullptr;

    void Release()
    {
        if (signal_stack != nullptr) munmap(signal_stack, kSignalStackSize);
        if (teb_mapping != nullptr) munmap(teb_mapping, teb_mapping_size);
        if (stack_mapping != nullptr) munmap(stack_mapping, stack_mapping_size);
        signal_stack = nullptr;
        teb_mapping = nullptr;
        stack_mapping = nullptr;
    }
};

// The host thread of a secondary guest thread: its signal stack and guest FS
// descriptor (the main thread's TLS entry, based at this thread's TEB), then
// the ThreadProc once the guest lock is ours.
void RunSecondaryThread(SecondaryThread* secondary)
{
    NativeGuestTermination termination;
    stack_t alternate = {};
    alternate.ss_sp = secondary->signal_stack;
    alternate.ss_size = kSignalStackSize;
    user_desc descriptor = {};
    descriptor.entry_number = static_cast<unsigned int>(secondary->bootstrap->tls_entry);
    descriptor.base_addr = secondary->thread.teb;
    descriptor.limit = 0xFFFFF;
    descriptor.seg_32bit = 1;
    descriptor.limit_in_pages = 1;
    descriptor.useable = 1;
    const bool ready = sigaltstack(&alternate, nullptr) == 0 &&
                       syscall(SYS_set_thread_area, &descriptor) == 0;
    BindNativeGuestThread(&secondary->thread);
    AcquireNativeGuestLock();
    if (!ready)
    {
        // Nothing of the guest ran; report it as a fault at the ThreadProc.
        termination.fault.status_code = SIGSEGV;
        termination.fault.kind = NativeFaultKind::kAccessViolation;
        termination.fault.instruction_pointer = secondary->start.start;
        TerminateNativeGuestProcess(termination);
        return;
    }
    ConfigureNativeImportGateHandler(secondary->handler, secondary->handler_context);
    ConfigureNativeImportGateStackRange(secondary->thread.stack_limit, secondary->thread.stack_base);
    std::uint32_t exit_code = 0;
    std::string error;
    const bool returned = secondary->bootstrap->Execute(
        [&]() { exit_code = CallGuestThread(secondary->start.start, secondary->thread.stack_base,
                                            secondary->start.parameter); },
        &termination.fault, &error);
    if (returned && !secondary->bootstrap->process_exited)
    {
        if (secondary->start.on_exit != nullptr)
        {
            secondary->start.on_exit(secondary->start.exit_context, secondary->start.token, exit_code);
        }
        ClearNativeImportGateHandler();
        RemoveNativeGuestThreadAndRelease(&secondary->thread);
        stack_t disabled = {};
        disabled.ss_flags = SS_DISABLE;
        sigaltstack(&disabled, nullptr);
        secondary->Release();
        delete secondary;
        return;
    }
    if (returned)
    {
        termination.process_exited = true;
        termination.exit_code = secondary->bootstrap->exit_code;
    }
    TerminateNativeGuestProcess(termination);
}

}  // namespace

bool StartNativeGuestThread(const NativeGuestThreadStart& start, std::uint32_t* teb, std::string* error)
{
    NativeProcessBootstrap::Impl* bootstrap = g_current_bootstrap;
    if (bootstrap == nullptr || teb == nullptr || start.start == 0)
    {
        if (error != nullptr) *error = "no guest process is running on this thread";
        return false;
    }
    const long page_size_value = sysconf(_SC_PAGESIZE);
    const auto page_size = static_cast<std::uint32_t>(page_size_value <= 0 ? 4096 : page_size_value);
    auto* secondary = new SecondaryThread;
    secondary->start = start;
    secondary->bootstrap = bootstrap;
    secondary->stack_mapping_size = page_size + kThreadStackSize;
    secondary->teb_mapping_size = page_size;
    secondary->stack_mapping = mmap(nullptr, secondary->stack_mapping_size, PROT_NONE,
                                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    secondary->teb_mapping = mmap(nullptr, secondary->teb_mapping_size, PROT_READ | PROT_WRITE,
                                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    secondary->signal_stack = mmap(nullptr, kSignalStackSize, PROT_READ | PROT_WRITE,
                                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (secondary->stack_mapping == MAP_FAILED) secondary->stack_mapping = nullptr;
    if (secondary->teb_mapping == MAP_FAILED) secondary->teb_mapping = nullptr;
    if (secondary->signal_stack == MAP_FAILED) secondary->signal_stack = nullptr;
    auto* stack_bytes = static_cast<std::uint8_t*>(secondary->stack_mapping);
    if (secondary->stack_mapping == nullptr || secondary->teb_mapping == nullptr ||
        secondary->signal_stack == nullptr ||
        mprotect(stack_bytes + page_size, kThreadStackSize, PROT_READ | PROT_WRITE) != 0)
    {
        secondary->Release();
        delete secondary;
        if (error != nullptr) *error = "cannot allocate a guest thread's stack and TEB";
        return false;
    }
    NativeGuestThread& thread = secondary->thread;
    thread.stack_limit = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(stack_bytes + page_size));
    thread.stack_base = thread.stack_limit + kThreadStackSize;
    thread.teb = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(secondary->teb_mapping));
    thread.width_state = secondary;
    auto* environment = static_cast<std::uint8_t*>(secondary->teb_mapping);
    WriteU32(environment, 0x00, 0xFFFFFFFFU);
    WriteU32(environment, 0x04, thread.stack_base);
    WriteU32(environment, 0x08, thread.stack_limit);
    WriteU32(environment, 0x18, thread.teb);
    WriteU32(environment, 0x30, bootstrap->peb);
    CurrentNativeImportGateHandler(&secondary->handler, &secondary->handler_context);
    AddNativeGuestThread(&thread);
    std::thread(&RunSecondaryThread, secondary).detach();
    *teb = thread.teb;
    if (error != nullptr) error->clear();
    return true;
}

// Nothing of the i386 transition is kept outside the thread itself.
void SaveNativeGuestTransition(NativeGuestThread*) {}
void RestoreNativeGuestTransition(NativeGuestThread*) {}

void AbandonNativeGuestRun(const NativeGuestTermination& termination)
{
    if (g_current_bootstrap == nullptr || g_guest_active == 0)
    {
        std::abort();
    }
    if (termination.process_exited)
    {
        g_exit_code = termination.exit_code;
        g_exit_requested = 1;
    }
    else
    {
        const NativeGuestFault& fault = termination.fault;
        g_fault_signal = static_cast<sig_atomic_t>(fault.status_code);
        g_fault_eip = static_cast<sig_atomic_t>(fault.instruction_pointer);
        g_fault_esp = static_cast<sig_atomic_t>(fault.stack_pointer);
        g_fault_address = static_cast<sig_atomic_t>(fault.fault_address);
        g_fault_signal_code = static_cast<sig_atomic_t>(fault.signal_code);
        g_fault_cpu_error_code = static_cast<sig_atomic_t>(fault.cpu_error_code);
        g_fault_eax = static_cast<sig_atomic_t>(fault.eax);
        g_fault_ebx = static_cast<sig_atomic_t>(fault.ebx);
        g_fault_ecx = static_cast<sig_atomic_t>(fault.ecx);
        g_fault_edx = static_cast<sig_atomic_t>(fault.edx);
        g_fault_esi = static_cast<sig_atomic_t>(fault.esi);
        g_fault_edi = static_cast<sig_atomic_t>(fault.edi);
        g_fault_ebp = static_cast<sig_atomic_t>(fault.ebp);
        g_fault_eflags = static_cast<sig_atomic_t>(fault.eflags);
    }
    siglongjmp(g_guest_jump, 1);
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

}  // namespace re2dj::platform::native
