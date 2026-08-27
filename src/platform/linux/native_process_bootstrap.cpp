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

sigjmp_buf g_guest_jump;
volatile sig_atomic_t g_guest_active = 0;
volatile sig_atomic_t g_fault_signal = 0;
volatile sig_atomic_t g_fault_eip = 0;
volatile sig_atomic_t g_fault_esp = 0;

void WriteU32(std::uint8_t* bytes, std::size_t offset, std::uint32_t value)
{
    std::memcpy(bytes + offset, &value, sizeof(value));
}

void GuestSignalHandler(int signal_number, siginfo_t*, void* context_pointer)
{
    if (g_guest_active == 0)
    {
        _exit(128 + signal_number);
    }
    const auto* context = static_cast<const ucontext_t*>(context_pointer);
    g_fault_signal = signal_number;
    g_fault_eip = static_cast<std::uint32_t>(context->uc_mcontext.gregs[REG_EIP]);
    g_fault_esp = static_cast<std::uint32_t>(context->uc_mcontext.gregs[REG_ESP]);
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
        error->clear();
        return false;
    }
};

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
    return impl_->Execute(
        [&]() { *result = CallGuestEntry(entry, impl_->stack_base); }, fault, error);
}

}  // namespace re2dj::platform::linux
