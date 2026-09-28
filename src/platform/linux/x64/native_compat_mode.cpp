#include "native_compat_mode.h"

#include "../native_guest_seh.h"
#include "../native_guest_threads.h"
#include "../native_import_bridge.h"
#include "../native_instruction_trace.h"
#include "../native_legacy_io.h"
#include "../native_process_bootstrap.h"

#include <asm/ldt.h>
#include <asm/prctl.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/auxv.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <ucontext.h>
#include <unistd.h>

#include <array>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <thread>

#ifndef HWCAP2_FSGSBASE
#define HWCAP2_FSGSBASE (1 << 1)
#endif

extern "C"
{
re2dj::platform::linux::NativeCompatTransitionState* g_native_compat_active_state = nullptr;
}

namespace re2dj::platform::linux
{
namespace
{

constexpr std::uint32_t kGuestStackSize = 1024 * 1024;
constexpr std::size_t kSignalStackSize = 64 * 1024;
constexpr std::array<int, 5> kGuestSignals = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGTRAP};
constexpr std::uint32_t kTrialResult = 0x2301C0DEU;
constexpr std::size_t kMaximumArguments = 64;
constexpr std::size_t kLdtProbeEntries = 16;

static_assert(offsetof(ucontext_t, uc_mcontext.gregs) + REG_CSGSFS * sizeof(greg_t) == 184,
              "NativeCompatSignalEntry reads the interrupted CS at this offset");

// Each guest thread runs on a host thread of its own with a runtime of its
// own, so the run state is per thread. Only host code reads it, after the
// transition has restored the host FS base.
thread_local NativeCompatModeRuntime::Impl* g_active_runtime = nullptr;

thread_local sigjmp_buf g_guest_jump;
thread_local volatile std::uint64_t g_fault_signal = 0;
thread_local volatile std::uint64_t g_fault_eip = 0;
thread_local volatile std::uint64_t g_fault_esp = 0;
thread_local volatile std::uint64_t g_fault_address = 0;
thread_local volatile std::uint64_t g_fault_signal_code = 0;
thread_local volatile std::uint64_t g_fault_cpu_error_code = 0;
thread_local volatile std::uint64_t g_fault_eax = 0;
thread_local volatile std::uint64_t g_fault_ebx = 0;
thread_local volatile std::uint64_t g_fault_ecx = 0;
thread_local volatile std::uint64_t g_fault_edx = 0;
thread_local volatile std::uint64_t g_fault_esi = 0;
thread_local volatile std::uint64_t g_fault_edi = 0;
thread_local volatile std::uint64_t g_fault_ebp = 0;
thread_local volatile std::uint64_t g_fault_eflags = 0;
// Set by ExitNativeGuestProcess just before it jumps back to Run.
thread_local volatile std::uint32_t g_exit_requested = 0;
thread_local volatile std::uint32_t g_exit_code = 0;

std::uint32_t PageSize()
{
    const long value = sysconf(_SC_PAGESIZE);
    return value <= 0 ? 4096U : static_cast<std::uint32_t>(value);
}

void* LowPointer(std::uint32_t address)
{
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(address));
}

void WriteGuestU32(std::uint32_t address, std::uint32_t value)
{
    std::memcpy(LowPointer(address), &value, sizeof(value));
}

std::uint32_t ReadGuestU32(std::uint32_t address)
{
    std::uint32_t value = 0;
    std::memcpy(&value, LowPointer(address), sizeof(value));
    return value;
}

std::size_t BlobOffset(const unsigned char* label)
{
    return static_cast<std::size_t>(label - native_compat_blob_start);
}

std::uint16_t CurrentCodeSelector()
{
    std::uint16_t selector = 0;
    __asm__ volatile("movw %%cs, %0" : "=r"(selector));
    return selector;
}

bool CpuHasFsGsBase()
{
    return (getauxval(AT_HWCAP2) & HWCAP2_FSGSBASE) != 0;
}

// The transition code and state pages shared by every runtime in the process.
struct NativeCompatTransitionPages
{
    NativeLowMemory code;
    NativeLowMemory data;
    bool ready = false;
};

NativeCompatTransitionPages g_transition;

NativeCompatTransitionState* TransitionState()
{
    return static_cast<NativeCompatTransitionState*>(g_transition.data.memory);
}

std::uint32_t TransitionCodeAddress(const unsigned char* label)
{
    return g_transition.code.address + static_cast<std::uint32_t>(BlobOffset(label));
}

bool EnsureTransitionPages(std::string* error)
{
    if (g_transition.ready)
    {
        return true;
    }
    const std::uint32_t page = PageSize();
    const std::size_t blob_size = BlobOffset(native_compat_blob_end);
    if (blob_size > page)
    {
        *error = "compatibility-mode transition blob exceeds one page";
        return false;
    }
    if (!MapNativeLowMemory(page, PROT_READ | PROT_WRITE, &g_transition.data, error) ||
        !MapNativeLowMemory(page, PROT_READ | PROT_WRITE, &g_transition.code, error))
    {
        ReleaseNativeLowMemory(&g_transition.code);
        ReleaseNativeLowMemory(&g_transition.data);
        return false;
    }
    NativeCompatTransitionState* state = TransitionState();
    *state = {};

    auto* code = static_cast<std::uint8_t*>(g_transition.code.memory);
    std::memcpy(code, native_compat_blob_start, blob_size);
    const std::uint32_t exit64 = TransitionCodeAddress(native_compat_exit64);
    const std::uint32_t gate64 = TransitionCodeAddress(native_compat_gate64);
    const auto state_address = reinterpret_cast<std::uint64_t>(state);
    const auto exit_target = reinterpret_cast<std::uint64_t>(&NativeCompatGuestExit);
    const auto landing_target = reinterpret_cast<std::uint64_t>(&NativeCompatImportLanding);
    std::memcpy(code + BlobOffset(native_compat_exit32_target), &exit64, sizeof(exit64));
    std::memcpy(code + BlobOffset(native_compat_gate32_target), &gate64, sizeof(gate64));
    std::memcpy(code + BlobOffset(native_compat_exit64_state), &state_address,
                sizeof(state_address));
    std::memcpy(code + BlobOffset(native_compat_exit64_target), &exit_target,
                sizeof(exit_target));
    std::memcpy(code + BlobOffset(native_compat_gate64_state), &state_address,
                sizeof(state_address));
    std::memcpy(code + BlobOffset(native_compat_gate64_target), &landing_target,
                sizeof(landing_target));
    if (mprotect(g_transition.code.memory, g_transition.code.size, PROT_READ | PROT_EXEC) != 0)
    {
        ReleaseNativeLowMemory(&g_transition.code);
        ReleaseNativeLowMemory(&g_transition.data);
        *error = "cannot protect the compatibility-mode transition page";
        return false;
    }
    __builtin___clear_cache(static_cast<char*>(g_transition.code.memory),
                            static_cast<char*>(g_transition.code.memory) + blob_size);
    g_transition.ready = true;
    return true;
}

}  // namespace

std::uint64_t ReadNativeHostFsBase(bool use_fsgsbase)
{
    std::uint64_t base = 0;
    if (use_fsgsbase)
    {
        __asm__ volatile("rdfsbase %0" : "=r"(base));
        return base;
    }
    syscall(SYS_arch_prctl, ARCH_GET_FS, &base);
    return base;
}

std::uint32_t NativeCompatImportBridgeAddress()
{
    std::string error;
    return EnsureTransitionPages(&error) ? TransitionCodeAddress(native_compat_gate32) : 0;
}

std::uint32_t NativeCompatImportCleanupAddress()
{
    std::string error;
    return EnsureTransitionPages(&error)
        ? g_transition.data.address +
              static_cast<std::uint32_t>(offsetof(NativeCompatTransitionState, cleanup_bytes))
        : 0;
}

struct NativeCompatModeRuntime::Impl
{
    NativeLowMemory stack;
    NativeLowMemory environment;
    std::uint32_t stack_base = 0;
    std::uint32_t stack_limit = 0;
    std::uint32_t teb = 0;
    std::uint32_t peb = 0;
    std::uint32_t image_base = 0;
    NativeGuestExceptionDispatcher exceptions;
    // The signal of the exception last delivered, reported if it goes unhandled.
    int delivered_signal = 0;
    std::uint32_t delivered_signal_code = 0;
    std::uint32_t delivered_cpu_error = 0;
    std::uint32_t delivered_fault_address = 0;
    int ldt_entry = -1;
    std::uint16_t fs_selector = 0;
    bool use_fsgsbase = false;
    void* signal_stack = nullptr;
    stack_t previous_signal_stack = {};
    std::array<struct sigaction, kGuestSignals.size()> previous_actions = {};
    int installed_action_count = 0;
    bool initialized = false;

    NativeImportGateHandler handler = nullptr;
    void* handler_context = nullptr;
    // The guest stack pointer at the innermost import being handled, 0 when
    // none is; guest calls go below it.
    std::uint32_t import_stack_pointer = 0;
    NativeCompatEnterFunction pending_enter = nullptr;
    std::uint32_t pending_entry = 0;
    std::uint32_t pending_stack_pointer = 0;

    // This runtime's guest thread. A secondary runtime runs one thread other
    // than the main one: it shares the process's PEB and signal handlers,
    // and keeps its part of the shared transition state while another thread
    // holds the guest lock.
    NativeGuestThread thread;
    bool threads_begun = false;
    bool secondary = false;
    std::uint64_t saved_host_stack_pointer = 0;
    std::uint64_t saved_host_fs_base = 0;

    ~Impl()
    {
        if (threads_begun)
        {
            EndNativeGuestThreads();
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
        if (ldt_entry >= 0)
        {
            user_desc descriptor = {};
            descriptor.entry_number = static_cast<unsigned int>(ldt_entry);
            descriptor.read_exec_only = 1;
            descriptor.seg_not_present = 1;
            syscall(SYS_modify_ldt, 0x11, &descriptor, sizeof(descriptor));
        }
        ReleaseNativeGuestExceptionDispatcher(&exceptions);
        ReleaseNativeLowMemory(&environment);
        ReleaseNativeLowMemory(&stack);
    }

    bool AllocateGuestStack(std::string* error)
    {
        const std::uint32_t page = PageSize();
        if (!MapNativeLowMemory(page + kGuestStackSize, PROT_NONE, &stack, error))
        {
            return false;
        }
        auto* bytes = static_cast<std::uint8_t*>(stack.memory);
        if (mprotect(bytes + page, kGuestStackSize, PROT_READ | PROT_WRITE) != 0)
        {
            *error = "cannot commit guest stack";
            return false;
        }
        stack_limit = stack.address + page;
        stack_base = stack_limit + kGuestStackSize;
        return true;
    }

    // A secondary thread's TEB, pointing at the main thread's PEB.
    bool AllocateThreadEnvironment(std::uint32_t process_peb, std::string* error)
    {
        if (!MapNativeLowMemory(PageSize(), PROT_READ | PROT_WRITE, &environment, error))
        {
            return false;
        }
        teb = environment.address;
        peb = process_peb;
        WriteGuestU32(teb + 0x00, 0xFFFFFFFFU);
        WriteGuestU32(teb + 0x04, stack_base);
        WriteGuestU32(teb + 0x08, stack_limit);
        WriteGuestU32(teb + 0x18, teb);
        WriteGuestU32(teb + 0x30, peb);
        return true;
    }

    // A runtime for a thread other than the main one, prepared by the lock
    // holder: its own stack, TEB, FS descriptor, and exception dispatcher.
    // Its host thread installs the alternate signal stack itself.
    bool InitializeThread(const Impl& process, std::string* error)
    {
        secondary = true;
        image_base = process.image_base;
        use_fsgsbase = process.use_fsgsbase;
        if (!AllocateGuestStack(error) || !AllocateThreadEnvironment(process.peb, error) ||
            !InstallFsDescriptor(error) || !CreateNativeGuestExceptionDispatcher(&exceptions, error))
        {
            return false;
        }
        thread.teb = teb;
        thread.stack_limit = stack_limit;
        thread.stack_base = stack_base;
        thread.width_state = this;
        initialized = true;
        return true;
    }

    bool InstallSignalStack(std::string* error)
    {
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
            munmap(signal_stack, kSignalStackSize);
            signal_stack = nullptr;
            *error = "cannot install alternate signal stack";
            return false;
        }
        return true;
    }

    bool AllocateEnvironment(std::uint32_t image_base, std::string* error)
    {
        const std::uint32_t page = PageSize();
        if (!MapNativeLowMemory(page * 2, PROT_READ | PROT_WRITE, &environment, error))
        {
            return false;
        }
        teb = environment.address;
        peb = teb + page;
        WriteGuestU32(teb + 0x00, 0xFFFFFFFFU);
        WriteGuestU32(teb + 0x04, stack_base);
        WriteGuestU32(teb + 0x08, stack_limit);
        WriteGuestU32(teb + 0x18, teb);
        WriteGuestU32(teb + 0x30, peb);
        WriteGuestU32(peb + 0x08, image_base);
        return true;
    }

    bool InstallFsDescriptor(std::string* error)
    {
        std::array<std::uint64_t, kLdtProbeEntries> existing = {};
        const long bytes = syscall(SYS_modify_ldt, 0, existing.data(), sizeof(existing));
        if (bytes < 0)
        {
            *error = "Linux x64 compatibility mode unavailable: modify_ldt is not permitted";
            return false;
        }
        const std::size_t count = static_cast<std::size_t>(bytes) / sizeof(std::uint64_t);
        std::size_t index = count;
        for (std::size_t candidate = 0; candidate < count; ++candidate)
        {
            if (existing[candidate] == 0)
            {
                index = candidate;
                break;
            }
        }
        if (index >= kLdtProbeEntries)
        {
            *error = "no free LDT entry for the guest FS descriptor";
            return false;
        }
        user_desc descriptor = {};
        descriptor.entry_number = static_cast<unsigned int>(index);
        descriptor.base_addr = teb;
        descriptor.limit = 0xFFFFF;
        descriptor.seg_32bit = 1;
        descriptor.limit_in_pages = 1;
        descriptor.useable = 1;
        if (syscall(SYS_modify_ldt, 0x11, &descriptor, sizeof(descriptor)) != 0)
        {
            *error = "Linux x64 compatibility mode unavailable: cannot install the guest FS "
                     "descriptor";
            return false;
        }
        ldt_entry = static_cast<int>(index);
        fs_selector = static_cast<std::uint16_t>((index << 3) | 7);
        return true;
    }

    bool InstallSignalHandlers(std::string* error)
    {
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
            munmap(signal_stack, kSignalStackSize);
            signal_stack = nullptr;
            *error = "cannot install alternate signal stack";
            return false;
        }
        struct sigaction action = {};
        action.sa_sigaction = &NativeCompatSignalEntry;
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
        return true;
    }

    bool Initialize(std::uint32_t image_base,
                    const NativeCompatModeOptions& options,
                    std::string* error)
    {
        if (initialized || stack.memory != nullptr)
        {
            *error = "compatibility-mode runtime is already initialized";
            return false;
        }
        if (CurrentCodeSelector() != kCompatUser64CodeSelector)
        {
            *error = "Linux x64 compatibility mode unavailable: unexpected 64-bit code selector";
            return false;
        }
        this->image_base = image_base;
        use_fsgsbase = !options.force_arch_prctl && CpuHasFsGsBase();
        if (!EnsureTransitionPages(error) || !AllocateGuestStack(error) ||
            !AllocateEnvironment(image_base, error) || !InstallFsDescriptor(error) ||
            !InstallSignalHandlers(error) || !CreateNativeGuestExceptionDispatcher(&exceptions, error))
        {
            return false;
        }

        initialized = true;
        NativeCompatModeCall trial;
        trial.entry = TransitionCodeAddress(native_compat_trial32);
        trial.handler = &RejectTrialImport;
        NativeCompatModeRunResult result;
        NativeGuestFault fault;
        if (!Run(trial, &result, &fault, error))
        {
            initialized = false;
            if (error->empty())
            {
                *error = "Linux x64 compatibility mode unavailable: trial transition raised "
                         "signal " + std::to_string(fault.status_code);
            }
            return false;
        }
        if (result.eax != kTrialResult)
        {
            initialized = false;
            *error = "Linux x64 compatibility mode unavailable: trial transition returned the "
                     "wrong value";
            return false;
        }
        thread.teb = teb;
        thread.stack_limit = stack_limit;
        thread.stack_base = stack_base;
        thread.width_state = this;
        BeginNativeGuestThreads(&thread);
        threads_begun = true;
        error->clear();
        return true;
    }

    // The trial stub makes no imports; naming a handler keeps a configured
    // process-wide handler from being consulted during initialization.
    static bool RejectTrialImport(const NativeImportGateEvent&, NativeImportGateResult*, void*)
    {
        return false;
    }

    __attribute__((noinline)) std::uint64_t EnterPending()
    {
        return pending_enter(TransitionState(), pending_entry, pending_stack_pointer);
    }

    bool Run(const NativeCompatModeCall& call,
             NativeCompatModeRunResult* result,
             NativeGuestFault* fault,
             std::string* error)
    {
        if (result == nullptr || fault == nullptr || error == nullptr)
        {
            if (error != nullptr) *error = "invalid compatibility-mode run arguments";
            return false;
        }
        if (!initialized || g_active_runtime != nullptr || call.entry == 0 ||
            call.arguments.size() > kMaximumArguments)
        {
            *error = "invalid compatibility-mode run state";
            return false;
        }
        *result = {};
        *fault = {};

        std::uint32_t stack_pointer = stack_base & ~0xFU;
        for (std::size_t index = call.arguments.size(); index > 0; --index)
        {
            stack_pointer -= 4;
            WriteGuestU32(stack_pointer, call.arguments[index - 1]);
        }
        stack_pointer -= 4;
        WriteGuestU32(stack_pointer, TransitionCodeAddress(native_compat_exit32));

        NativeCompatTransitionState* state = TransitionState();
        state->guest_fs_selector = fs_selector;
        state->use_fsgsbase = use_fsgsbase ? 1 : 0;
        state->host_fs_base = ReadNativeHostFsBase(use_fsgsbase);
        state->cleanup_bytes = 0;
        if (call.handler != nullptr)
        {
            handler = call.handler;
            handler_context = call.handler_context;
        }
        else
        {
            const NativeImportGateConfiguration configured = ConfiguredNativeImportGate();
            handler = configured.handler;
            handler_context = configured.context;
        }
        pending_enter = call.enter != nullptr ? call.enter : &NativeCompatEnterGuest;
        pending_entry = call.entry;
        pending_stack_pointer = stack_pointer;
        result->entry_stack_pointer = stack_pointer;

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
        import_stack_pointer = 0;
        g_active_runtime = this;
        if (sigsetjmp(g_guest_jump, 1) == 0)
        {
            // Left set: a run on another guest thread may still be under way,
            // and the signal entry checks the interrupted CS itself.
            g_native_compat_active_state = state;
            const std::uint64_t value = EnterPending();
            g_active_runtime = nullptr;
            handler = nullptr;
            handler_context = nullptr;
            result->eax = static_cast<std::uint32_t>(value);
            result->edx = static_cast<std::uint32_t>(value >> 32);
            error->clear();
            return true;
        }
        g_active_runtime = nullptr;
        handler = nullptr;
        handler_context = nullptr;
        if (g_exit_requested != 0)
        {
            // The guest ended its process from an import; that is a normal
            // completion, not a fault.
            g_exit_requested = 0;
            result->process_exited = true;
            result->exit_code = g_exit_code;
            error->clear();
            return true;
        }
        fault->status_code = static_cast<std::uint32_t>(g_fault_signal);
        fault->instruction_pointer = static_cast<std::uint32_t>(g_fault_eip);
        fault->stack_pointer = static_cast<std::uint32_t>(g_fault_esp);
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

NativeCompatModeRuntime::NativeCompatModeRuntime() : impl_(new Impl) {}
NativeCompatModeRuntime::~NativeCompatModeRuntime() { delete impl_; }

bool NativeCompatModeRuntime::Initialize(std::uint32_t image_base,
                                         const NativeCompatModeOptions& options,
                                         std::string* error)
{
    return error != nullptr && impl_->Initialize(image_base, options, error);
}

bool NativeCompatModeRuntime::Run(const NativeCompatModeCall& call,
                                  NativeCompatModeRunResult* result,
                                  NativeGuestFault* fault,
                                  std::string* error)
{
    return impl_->Run(call, result, fault, error);
}

std::uint32_t NativeCompatModeRuntime::ImportBridgeAddress() const
{
    return impl_->initialized ? TransitionCodeAddress(native_compat_gate32) : 0;
}

std::uint32_t NativeCompatModeRuntime::ImportCleanupAddress() const
{
    return impl_->initialized ? NativeCompatImportCleanupAddress() : 0;
}

std::uint32_t NativeCompatModeRuntime::Teb() const { return impl_->teb; }
std::uint32_t NativeCompatModeRuntime::Peb() const { return impl_->peb; }
std::uint32_t NativeCompatModeRuntime::GuestStackBase() const { return impl_->stack_base; }
std::uint32_t NativeCompatModeRuntime::GuestStackLimit() const { return impl_->stack_limit; }
std::uint16_t NativeCompatModeRuntime::GuestFsSelector() const { return impl_->fs_selector; }
bool NativeCompatModeRuntime::UsesFsGsBase() const { return impl_->use_fsgsbase; }

std::uint32_t NativeCompatModeRuntime::SehDispatchCount() const
{
    return ExceptionCounters().resumed;
}

std::uint32_t NativeCompatModeRuntime::LastSehHandler() const
{
    return ExceptionCounters().last_handler;
}

std::uint32_t NativeCompatModeRuntime::LastSehResumedEip() const
{
    return ExceptionCounters().last_resumed_eip;
}

NativeGuestExceptionCounters NativeCompatModeRuntime::ExceptionCounters() const
{
    return ReadNativeGuestExceptionCounters(impl_->exceptions);
}

namespace
{

struct CompatThreadStart
{
    NativeCompatModeRuntime::Impl* runtime = nullptr;
    NativeGuestThreadStart start;
    NativeImportGateHandler handler = nullptr;
    void* handler_context = nullptr;
};

// The host thread of a secondary guest thread: its alternate signal stack,
// then the ThreadProc once the guest lock is ours.
void RunCompatThread(CompatThreadStart* start)
{
    NativeCompatModeRuntime::Impl* runtime = start->runtime;
    std::string error;
    const bool ready = runtime->InstallSignalStack(&error);
    BindNativeGuestThread(&runtime->thread);
    AcquireNativeGuestLock();
    NativeGuestTermination termination;
    if (!ready)
    {
        termination.fault.status_code = SIGSEGV;
        termination.fault.instruction_pointer = start->start.start;
        TerminateNativeGuestProcess(termination);
        return;
    }
    NativeCompatModeCall call;
    call.entry = start->start.start;
    call.arguments = {start->start.parameter};
    call.handler = start->handler;
    call.handler_context = start->handler_context;
    NativeCompatModeRunResult result;
    const bool returned = runtime->Run(call, &result, &termination.fault, &error);
    if (returned && !result.process_exited)
    {
        if (start->start.on_exit != nullptr)
        {
            start->start.on_exit(start->start.exit_context, start->start.token, result.eax);
        }
        RemoveNativeGuestThreadAndRelease(&runtime->thread);
        delete runtime;
        delete start;
        return;
    }
    if (returned)
    {
        termination.process_exited = true;
        termination.exit_code = result.exit_code;
    }
    TerminateNativeGuestProcess(termination);
}

}  // namespace

bool StartNativeGuestThread(const NativeGuestThreadStart& start, std::uint32_t* teb, std::string* error)
{
    NativeCompatModeRuntime::Impl* creator = g_active_runtime;
    if (creator == nullptr || teb == nullptr || start.start == 0)
    {
        if (error != nullptr) *error = "no guest process is running on this thread";
        return false;
    }
    auto* runtime = new NativeCompatModeRuntime::Impl;
    std::string init_error;
    if (!runtime->InitializeThread(*creator, &init_error))
    {
        delete runtime;
        if (error != nullptr) *error = "cannot prepare a guest thread: " + init_error;
        return false;
    }
    auto* thread_start = new CompatThreadStart;
    thread_start->runtime = runtime;
    thread_start->start = start;
    thread_start->handler = creator->handler;
    thread_start->handler_context = creator->handler_context;
    AddNativeGuestThread(&runtime->thread);
    std::thread(&RunCompatThread, thread_start).detach();
    *teb = runtime->teb;
    if (error != nullptr) error->clear();
    return true;
}

void SaveNativeGuestTransition(NativeGuestThread* thread)
{
    auto* runtime = static_cast<NativeCompatModeRuntime::Impl*>(thread->width_state);
    const NativeCompatTransitionState* state = TransitionState();
    if (runtime == nullptr || state == nullptr)
    {
        return;
    }
    runtime->saved_host_stack_pointer = state->host_stack_pointer;
    runtime->saved_host_fs_base = state->host_fs_base;
}

void RestoreNativeGuestTransition(NativeGuestThread* thread)
{
    auto* runtime = static_cast<NativeCompatModeRuntime::Impl*>(thread->width_state);
    NativeCompatTransitionState* state = TransitionState();
    if (runtime == nullptr || state == nullptr)
    {
        return;
    }
    state->host_stack_pointer = runtime->saved_host_stack_pointer;
    state->host_fs_base = runtime->saved_host_fs_base;
    state->guest_fs_selector = runtime->fs_selector;
}

void AbandonNativeGuestRun(const NativeGuestTermination& termination)
{
    if (g_active_runtime == nullptr)
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
        g_fault_signal = fault.status_code;
        g_fault_eip = fault.instruction_pointer;
        g_fault_esp = fault.stack_pointer;
        g_fault_address = fault.fault_address;
        g_fault_signal_code = fault.signal_code;
        g_fault_cpu_error_code = fault.cpu_error_code;
        g_fault_eax = fault.eax;
        g_fault_ebx = fault.ebx;
        g_fault_ecx = fault.ecx;
        g_fault_edx = fault.edx;
        g_fault_esi = fault.esi;
        g_fault_edi = fault.edi;
        g_fault_ebp = fault.ebp;
        g_fault_eflags = fault.eflags;
    }
    siglongjmp(g_guest_jump, 1);
}

void CurrentNativeImportGateHandler(NativeImportGateHandler* handler, void** context)
{
    const NativeCompatModeRuntime::Impl* runtime = g_active_runtime;
    if (runtime != nullptr)
    {
        *handler = runtime->handler;
        *context = runtime->handler_context;
        return;
    }
    const NativeImportGateConfiguration configured = ConfiguredNativeImportGate();
    *handler = configured.handler;
    *context = configured.context;
}

void ExitNativeGuestProcess(std::uint32_t exit_code)
{
    // Called from the import dispatch, which runs on the host stack after the
    // landing restored the host FS base, so the jump only abandons the
    // landing's asm frames.
    if (g_active_runtime == nullptr)
    {
        std::abort();
    }
    g_exit_code = exit_code;
    g_exit_requested = 1;
    siglongjmp(g_guest_jump, 1);
}

namespace
{

NativeTrapRegisters ReadTrapRegisters(const ucontext_t* context,
                                      const NativeCompatModeRuntime::Impl& runtime)
{
    const greg_t* registers = context->uc_mcontext.gregs;
    NativeTrapRegisters trap;
    trap.eip = static_cast<std::uint32_t>(registers[REG_RIP]);
    trap.esp = static_cast<std::uint32_t>(registers[REG_RSP]);
    trap.eax = static_cast<std::uint32_t>(registers[REG_RAX]);
    trap.ebx = static_cast<std::uint32_t>(registers[REG_RBX]);
    trap.ecx = static_cast<std::uint32_t>(registers[REG_RCX]);
    trap.edx = static_cast<std::uint32_t>(registers[REG_RDX]);
    trap.esi = static_cast<std::uint32_t>(registers[REG_RSI]);
    trap.edi = static_cast<std::uint32_t>(registers[REG_RDI]);
    trap.ebp = static_cast<std::uint32_t>(registers[REG_RBP]);
    trap.eflags = static_cast<std::uint32_t>(registers[REG_EFL]);
    // The segments the guest saw: the transition loads them before every
    // entry, and an x86-64 ucontext does not carry DS, ES, or FS.
    trap.cs = kCompatUser32CodeSelector;
    trap.ss = kCompatUserDataSelector;
    trap.ds = kCompatUserDataSelector;
    trap.es = kCompatUserDataSelector;
    trap.fs = runtime.fs_selector;
    trap.gs = 0;
    return trap;
}

void WriteTrapRegisters(const NativeTrapRegisters& trap, ucontext_t* context)
{
    greg_t* registers = context->uc_mcontext.gregs;
    registers[REG_RIP] = static_cast<greg_t>(trap.eip);
    registers[REG_RSP] = static_cast<greg_t>(trap.esp);
    registers[REG_RAX] = static_cast<greg_t>(trap.eax);
    registers[REG_RBX] = static_cast<greg_t>(trap.ebx);
    registers[REG_RCX] = static_cast<greg_t>(trap.ecx);
    registers[REG_RDX] = static_cast<greg_t>(trap.edx);
    registers[REG_RSI] = static_cast<greg_t>(trap.esi);
    registers[REG_RDI] = static_cast<greg_t>(trap.edi);
    registers[REG_RBP] = static_cast<greg_t>(trap.ebp);
    registers[REG_EFL] = static_cast<greg_t>(trap.eflags);
}

// Delivers a guest fault to the guest's SEH chain through the dispatcher.
// Host code runs in 64-bit mode, so only compatibility-mode faults get here.
bool TryDeliverGuestException(NativeCompatModeRuntime::Impl* runtime,
                              int signal_number,
                              const siginfo_t* signal_info,
                              const ucontext_t* context,
                              NativeTrapRegisters* registers)
{
    if (registers->eip == runtime->exceptions.stop || IsNativeHostTrap(registers->eip) ||
        IsNativeHostTrap(registers->eip - 1))
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
    if (!DescribeNativeGuestException(cause, *registers, &record, &guest_context) ||
        !DeliverNativeGuestException(&runtime->exceptions,
                                     runtime->stack_limit,
                                     runtime->stack_base,
                                     record,
                                     guest_context,
                                     registers))
    {
        return false;
    }
    runtime->delivered_signal = signal_number;
    runtime->delivered_signal_code = signal_info == nullptr ? 0 : static_cast<std::uint32_t>(signal_info->si_code);
    runtime->delivered_cpu_error = cause.error_code;
    runtime->delivered_fault_address = cause.fault_address;
    return true;
}

}  // namespace

// A nested transition like TryDispatchGuestSeh's, from the host stack of the
// import being handled: the data and arguments go on the guest stack below
// that import's landing frame.
bool CallNativeGuestStdcall(std::uint32_t function,
                            std::span<const std::uint32_t> arguments,
                            std::span<std::uint8_t> data,
                            int data_argument,
                            std::uint32_t* eax,
                            std::string* error)
{
    constexpr std::uint32_t kGap = 64;
    constexpr std::uint32_t kReserve = 16 * 1024;
    NativeCompatModeRuntime::Impl* runtime = g_active_runtime;
    if (runtime == nullptr || runtime->import_stack_pointer == 0 || function == 0)
    {
        *error = "no guest import is being handled";
        return false;
    }
    if (arguments.size() > 16 || data.size() > kNativeGuestCallMaximumData ||
        (!data.empty() && (data_argument < 0 || static_cast<std::size_t>(data_argument) >= arguments.size())))
    {
        *error = "guest call shape is invalid";
        return false;
    }
    const std::uint32_t top = runtime->import_stack_pointer;
    const std::uint32_t needed = kGap + kNativeGuestCallMaximumData + 16 * 4 + 32 + kReserve;
    if (top > runtime->stack_base || top < runtime->stack_limit || top - runtime->stack_limit < needed)
    {
        *error = "guest stack has no room for a guest call";
        return false;
    }
    const std::uint32_t data_address =
        (top - kGap - static_cast<std::uint32_t>(data.size())) & ~0xFU;
    const auto count = static_cast<std::uint32_t>(arguments.size());
    // The arguments start 16-byte aligned, with the return address below.
    const std::uint32_t arguments_address = (data_address - count * 4) & ~0xFU;
    const std::uint32_t call_stack_pointer = arguments_address - 4;
    if (!data.empty())
    {
        std::memcpy(LowPointer(data_address), data.data(), data.size());
    }
    WriteGuestU32(call_stack_pointer, TransitionCodeAddress(native_compat_exit32));
    for (std::uint32_t index = 0; index < count; ++index)
    {
        const std::uint32_t value =
            !data.empty() && index == static_cast<std::uint32_t>(data_argument) ? data_address
                                                                                : arguments[index];
        WriteGuestU32(arguments_address + index * 4, value);
    }

    NativeCompatTransitionState* state = TransitionState();
    const std::uint64_t saved_host_stack_pointer = state->host_stack_pointer;
    const std::uint64_t result = NativeCompatEnterGuest(state, function, call_stack_pointer);
    state->host_stack_pointer = saved_host_stack_pointer;
    *eax = static_cast<std::uint32_t>(result);
    if (!data.empty())
    {
        std::memcpy(data.data(), LowPointer(data_address), data.size());
    }
    error->clear();
    return true;
}

}  // namespace re2dj::platform::linux

namespace linux_platform = re2dj::platform::linux;

extern "C" std::uint64_t NativeCompatImportDispatch(
    linux_platform::NativeCompatTransitionState* state,
    std::uint32_t guest_stack_pointer)
{
    // Guest stack at the landing: lcall eip, lcall cs, thunk return, gate,
    // caller return, arguments.
    linux_platform::NativeCompatModeRuntime::Impl* runtime = linux_platform::g_active_runtime;
    linux_platform::NativeImportGateEvent event;
    event.gate_address = linux_platform::ReadGuestU32(guest_stack_pointer + 12);
    event.stack_pointer = guest_stack_pointer + 16;
    event.instruction_pointer = linux_platform::ReadGuestU32(event.stack_pointer);
    linux_platform::NativeImportGateResult result;
    if (runtime == nullptr || runtime->handler == nullptr)
    {
        state->cleanup_bytes = 0;
        linux_platform::ResumeNativeInstructionTrace(event.instruction_pointer);
        return 0;
    }
    // Another guest thread waiting for the lock runs first.
    linux_platform::YieldNativeGuestThread();
    event.guest_stack_base = runtime->stack_base;
    event.guest_stack_limit = runtime->stack_limit;
    const std::uint32_t outer_import_stack_pointer = runtime->import_stack_pointer;
    runtime->import_stack_pointer = guest_stack_pointer;
    const bool handled = runtime->handler(event, &result, runtime->handler_context);
    runtime->import_stack_pointer = outer_import_stack_pointer;
    if (!handled)
    {
        state->cleanup_bytes = 0;
        linux_platform::ResumeNativeInstructionTrace(event.instruction_pointer);
        return 0;
    }
    if (result.exit_process)
    {
        linux_platform::ExitNativeGuestProcess(result.exit_code);
    }
    state->cleanup_bytes = result.stack_bytes_to_pop;
    linux_platform::ResumeNativeInstructionTrace(event.instruction_pointer);
    return (static_cast<std::uint64_t>(result.edx) << 32) | result.eax;
}

extern "C" int NativeCompatSignalHandler(int signal_number,
                                         siginfo_t* signal_info,
                                         void* context_pointer)
{
    auto* context = static_cast<ucontext_t*>(context_pointer);
    const auto code_selector =
        static_cast<std::uint16_t>(context->uc_mcontext.gregs[REG_CSGSFS] & 0xFFFF);
    linux_platform::NativeCompatModeRuntime::Impl* runtime = linux_platform::g_active_runtime;
    if (runtime == nullptr || code_selector != linux_platform::kCompatUser32CodeSelector)
    {
        // A host fault: let the default action run once this handler returns.
        signal(signal_number, SIG_DFL);
        raise(signal_number);
        return 0;
    }
    linux_platform::NativeTrapRegisters trap = linux_platform::ReadTrapRegisters(context, *runtime);
    if ((signal_number == SIGTRAP && linux_platform::HandleNativeInstructionTraceTrap(&trap)) ||
        (signal_number == SIGSEGV && linux_platform::HandleNativeLegacyIoTrap(&trap)) ||
        linux_platform::TryDeliverGuestException(runtime, signal_number, signal_info, context, &trap))
    {
        linux_platform::WriteTrapRegisters(trap, context);
        return 1;
    }
    linux_platform::Win32ExceptionRecord32 unhandled_record;
    linux_platform::Win32Context32 unhandled_context;
    if (linux_platform::ReadNativeGuestUnhandledException(
            runtime->exceptions, trap, &unhandled_record, &unhandled_context))
    {
        // No handler continued: report the fault the exception came from.
        linux_platform::g_fault_signal = static_cast<std::uint64_t>(runtime->delivered_signal);
        linux_platform::g_fault_eip = unhandled_record.exception_address;
        linux_platform::g_fault_esp = unhandled_context.esp;
        linux_platform::g_fault_address = runtime->delivered_fault_address;
        linux_platform::g_fault_signal_code = runtime->delivered_signal_code;
        linux_platform::g_fault_cpu_error_code = runtime->delivered_cpu_error;
        linux_platform::g_fault_eax = unhandled_context.eax;
        linux_platform::g_fault_ebx = unhandled_context.ebx;
        linux_platform::g_fault_ecx = unhandled_context.ecx;
        linux_platform::g_fault_edx = unhandled_context.edx;
        linux_platform::g_fault_esi = unhandled_context.esi;
        linux_platform::g_fault_edi = unhandled_context.edi;
        linux_platform::g_fault_ebp = unhandled_context.ebp;
        linux_platform::g_fault_eflags = unhandled_context.eflags;
        siglongjmp(linux_platform::g_guest_jump, 1);
    }
    const greg_t* registers = context->uc_mcontext.gregs;
    linux_platform::g_fault_signal = static_cast<std::uint64_t>(signal_number);
    linux_platform::g_fault_eip = static_cast<std::uint64_t>(registers[REG_RIP]);
    linux_platform::g_fault_esp = static_cast<std::uint64_t>(registers[REG_RSP]);
    linux_platform::g_fault_address = signal_info == nullptr
        ? 0 : reinterpret_cast<std::uint64_t>(signal_info->si_addr);
    linux_platform::g_fault_signal_code =
        signal_info == nullptr ? 0 : static_cast<std::uint64_t>(signal_info->si_code);
    linux_platform::g_fault_cpu_error_code = static_cast<std::uint64_t>(registers[REG_ERR]);
    linux_platform::g_fault_eax = static_cast<std::uint64_t>(registers[REG_RAX]);
    linux_platform::g_fault_ebx = static_cast<std::uint64_t>(registers[REG_RBX]);
    linux_platform::g_fault_ecx = static_cast<std::uint64_t>(registers[REG_RCX]);
    linux_platform::g_fault_edx = static_cast<std::uint64_t>(registers[REG_RDX]);
    linux_platform::g_fault_esi = static_cast<std::uint64_t>(registers[REG_RSI]);
    linux_platform::g_fault_edi = static_cast<std::uint64_t>(registers[REG_RDI]);
    linux_platform::g_fault_ebp = static_cast<std::uint64_t>(registers[REG_RBP]);
    linux_platform::g_fault_eflags = static_cast<std::uint64_t>(registers[REG_EFL]);
    siglongjmp(linux_platform::g_guest_jump, 1);
}
