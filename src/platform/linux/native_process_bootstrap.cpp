#include "native_process_bootstrap.h"

#include <asm/ldt.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <ucontext.h>
#include <unistd.h>

#include <array>
#include <cstddef>
#include <cstring>

namespace re2dj::platform::linux
{
namespace
{

constexpr std::uint32_t kGuestStackSize = 1024 * 1024;
constexpr std::uint32_t kSignalStackSize = 64 * 1024;
constexpr std::array<int, 5> kGuestSignals = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGTRAP};
constexpr sig_atomic_t kTrapFlag = 0x100;

struct RawInstructionTraceFrame
{
    volatile sig_atomic_t instruction_pointer = 0;
    volatile sig_atomic_t stack_pointer = 0;
    volatile sig_atomic_t eax = 0;
    volatile sig_atomic_t ebx = 0;
    volatile sig_atomic_t ecx = 0;
    volatile sig_atomic_t edx = 0;
    volatile sig_atomic_t esi = 0;
    volatile sig_atomic_t edi = 0;
    volatile sig_atomic_t ebp = 0;
    volatile sig_atomic_t eflags = 0;
};

sigjmp_buf g_guest_jump;
volatile sig_atomic_t g_guest_active = 0;
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
volatile sig_atomic_t g_trace_armed = 0;
volatile sig_atomic_t g_trace_started = 0;
volatile sig_atomic_t g_trace_paused = 0;
volatile sig_atomic_t g_trace_breakpoint_pending = 0;
volatile sig_atomic_t g_trace_limit_reached = 0;
volatile sig_atomic_t g_trace_breakpoint = 0;
volatile sig_atomic_t g_trace_original_byte = 0;
volatile sig_atomic_t g_trace_image_base = 0;
volatile sig_atomic_t g_trace_image_size = 0;
volatile sig_atomic_t g_trace_frame_count = 0;
std::array<RawInstructionTraceFrame, kNativeInstructionTraceMaximumFrames> g_trace_frames = {};

void WriteU32(std::uint8_t* bytes, std::size_t offset, std::uint32_t value)
{
    std::memcpy(bytes + offset, &value, sizeof(value));
}

void CaptureInstructionTraceFrame(const ucontext_t* context)
{
    const sig_atomic_t frame_index = g_trace_frame_count;
    if (frame_index < 0 || static_cast<std::size_t>(frame_index) >= g_trace_frames.size())
    {
        return;
    }
    RawInstructionTraceFrame& frame = g_trace_frames[static_cast<std::size_t>(frame_index)];
    frame.instruction_pointer = static_cast<sig_atomic_t>(context->uc_mcontext.gregs[REG_EIP]);
    frame.stack_pointer = static_cast<sig_atomic_t>(context->uc_mcontext.gregs[REG_ESP]);
    frame.eax = static_cast<sig_atomic_t>(context->uc_mcontext.gregs[REG_EAX]);
    frame.ebx = static_cast<sig_atomic_t>(context->uc_mcontext.gregs[REG_EBX]);
    frame.ecx = static_cast<sig_atomic_t>(context->uc_mcontext.gregs[REG_ECX]);
    frame.edx = static_cast<sig_atomic_t>(context->uc_mcontext.gregs[REG_EDX]);
    frame.esi = static_cast<sig_atomic_t>(context->uc_mcontext.gregs[REG_ESI]);
    frame.edi = static_cast<sig_atomic_t>(context->uc_mcontext.gregs[REG_EDI]);
    frame.ebp = static_cast<sig_atomic_t>(context->uc_mcontext.gregs[REG_EBP]);
    frame.eflags = static_cast<sig_atomic_t>(context->uc_mcontext.gregs[REG_EFL]);
    g_trace_frame_count = frame_index + 1;
}

bool IsTraceGuestInstructionAddress(sig_atomic_t address)
{
    return address >= g_trace_image_base &&
           static_cast<std::uint32_t>(address - g_trace_image_base) <
               static_cast<std::uint32_t>(g_trace_image_size);
}

void GuestSignalHandler(int signal_number, siginfo_t* signal_info, void* context_pointer)
{
    if (g_guest_active == 0)
    {
        _exit(128 + signal_number);
    }
    auto* context = static_cast<ucontext_t*>(context_pointer);
    if (signal_number == SIGTRAP && g_trace_armed != 0)
    {
        const sig_atomic_t instruction_pointer =
            static_cast<sig_atomic_t>(context->uc_mcontext.gregs[REG_EIP]);
        if (g_trace_breakpoint_pending != 0 &&
            instruction_pointer == g_trace_breakpoint + 1)
        {
            auto* breakpoint = reinterpret_cast<std::uint8_t*>(
                static_cast<std::uintptr_t>(g_trace_breakpoint));
            *breakpoint = static_cast<std::uint8_t>(g_trace_original_byte);
            g_trace_started = 1;
            g_trace_paused = 0;
            g_trace_breakpoint_pending = 0;
            context->uc_mcontext.gregs[REG_EIP] = g_trace_breakpoint;
            context->uc_mcontext.gregs[REG_EFL] |= kTrapFlag;
            CaptureInstructionTraceFrame(context);
            return;
        }
        if (g_trace_started != 0 && g_trace_paused == 0)
        {
            CaptureInstructionTraceFrame(context);
            if (!IsTraceGuestInstructionAddress(instruction_pointer))
            {
                g_trace_paused = 1;
                context->uc_mcontext.gregs[REG_EFL] &= ~kTrapFlag;
                return;
            }
            if (static_cast<std::size_t>(g_trace_frame_count) >= g_trace_frames.size())
            {
                g_trace_limit_reached = 1;
                g_trace_armed = 0;
                context->uc_mcontext.gregs[REG_EFL] &= ~kTrapFlag;
            }
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
            user_desc descriptor = {};
            descriptor.entry_number = tls_entry;
            descriptor.seg_not_present = 1;
            syscall(SYS_set_thread_area, &descriptor);
        }
    }

    bool Initialize(std::uint32_t image_base, std::string* error)
    {
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
        if (!initialized || fault == nullptr || error == nullptr)
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
        if (sigsetjmp(g_guest_jump, 1) == 0)
        {
            g_guest_active = 1;
            __asm__ volatile("movw %0, %%fs" : : "rm"(fs_selector));
            function();
            __asm__ volatile("movw %0, %%fs" : : "rm"(previous_fs));
            g_guest_active = 0;
            error->clear();
            return true;
        }
        __asm__ volatile("movw %0, %%fs" : : "rm"(previous_fs));
        g_guest_active = 0;
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

NativeProcessBootstrap::NativeProcessBootstrap() : impl_(new Impl) {}
NativeProcessBootstrap::~NativeProcessBootstrap() { delete impl_; }

bool ArmNativeInstructionTrace(NativeInstructionTrace* trace,
                               std::uint32_t breakpoint,
                               std::uint32_t image_base,
                               std::uint32_t image_size,
                               std::string* error)
{
    if (trace == nullptr || error == nullptr || breakpoint == 0 || image_base == 0 ||
        image_size == 0 || breakpoint < image_base || breakpoint - image_base >= image_size ||
        g_trace_armed != 0)
    {
        if (error != nullptr)
        {
            *error = "invalid native instruction trace arguments";
        }
        return false;
    }
    *trace = {};
    auto* byte = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(breakpoint));
    const std::uint8_t original_byte = *byte;
    *byte = 0xCC;
    g_trace_breakpoint = static_cast<sig_atomic_t>(breakpoint);
    g_trace_original_byte = static_cast<sig_atomic_t>(original_byte);
    g_trace_started = 0;
    g_trace_paused = 0;
    g_trace_breakpoint_pending = 1;
    g_trace_limit_reached = 0;
    g_trace_image_base = static_cast<sig_atomic_t>(image_base);
    g_trace_image_size = static_cast<sig_atomic_t>(image_size);
    g_trace_frame_count = 0;
    g_trace_armed = 1;
    trace->armed = true;
    trace->breakpoint = breakpoint;
    error->clear();
    return true;
}

bool ResumeNativeInstructionTrace(std::uint32_t return_address)
{
    if (g_trace_armed == 0 || g_trace_started == 0 || g_trace_paused == 0 ||
        g_trace_breakpoint_pending != 0 ||
        !IsTraceGuestInstructionAddress(static_cast<sig_atomic_t>(return_address)))
    {
        return false;
    }
    auto* byte = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(return_address));
    g_trace_breakpoint = static_cast<sig_atomic_t>(return_address);
    g_trace_original_byte = static_cast<sig_atomic_t>(*byte);
    *byte = 0xCC;
    g_trace_breakpoint_pending = 1;
    return true;
}

void FinalizeNativeInstructionTrace(NativeInstructionTrace* trace)
{
    if (trace == nullptr)
    {
        return;
    }
    if (g_trace_armed != 0 && g_trace_breakpoint_pending != 0 && g_trace_breakpoint != 0)
    {
        auto* breakpoint = reinterpret_cast<std::uint8_t*>(
            static_cast<std::uintptr_t>(g_trace_breakpoint));
        *breakpoint = static_cast<std::uint8_t>(g_trace_original_byte);
    }
    trace->armed = trace->armed || g_trace_breakpoint != 0;
    trace->started = g_trace_started != 0;
    trace->limit_reached = g_trace_limit_reached != 0;
    if (trace->breakpoint == 0)
    {
        trace->breakpoint = static_cast<std::uint32_t>(g_trace_breakpoint);
    }
    trace->frame_count = static_cast<std::uint32_t>(g_trace_frame_count);
    if (trace->frame_count > trace->frames.size())
    {
        trace->frame_count = static_cast<std::uint32_t>(trace->frames.size());
    }
    for (std::uint32_t index = 0; index < trace->frame_count; ++index)
    {
        const RawInstructionTraceFrame& source = g_trace_frames[index];
        NativeInstructionTraceFrame& destination = trace->frames[index];
        destination.instruction_pointer = static_cast<std::uint32_t>(source.instruction_pointer);
        destination.stack_pointer = static_cast<std::uint32_t>(source.stack_pointer);
        destination.eax = static_cast<std::uint32_t>(source.eax);
        destination.ebx = static_cast<std::uint32_t>(source.ebx);
        destination.ecx = static_cast<std::uint32_t>(source.ecx);
        destination.edx = static_cast<std::uint32_t>(source.edx);
        destination.esi = static_cast<std::uint32_t>(source.esi);
        destination.edi = static_cast<std::uint32_t>(source.edi);
        destination.ebp = static_cast<std::uint32_t>(source.ebp);
        destination.eflags = static_cast<std::uint32_t>(source.eflags);
    }
    g_trace_armed = 0;
    g_trace_started = 0;
    g_trace_paused = 0;
    g_trace_breakpoint_pending = 0;
    g_trace_limit_reached = 0;
    g_trace_breakpoint = 0;
    g_trace_original_byte = 0;
    g_trace_image_base = 0;
    g_trace_image_size = 0;
    g_trace_frame_count = 0;
}

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
    return impl_->Execute(
        [&]() { *result = CallGuestEntry(entry, impl_->stack_base); }, fault, error);
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

}  // namespace re2dj::platform::linux
