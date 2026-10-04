#define NOMINMAX
#include <windows.h>

#include <intrin.h>

#include "../../native/native_process_bootstrap.h"
#include "../../native/native_guest_seh.h"
#include "../../native/native_guest_threads.h"
#include "../../native/native_host_services.h"
#include "../../native/native_import_bridge.h"
#include "../../native/native_instruction_trace.h"
#include "../../native/native_legacy_io.h"
#include "native_guest_transition.h"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <mutex>

// The asm here moves SEH chains between fs:0 and the shadow TEB rather than
// registering handlers of its own, so C4733 (fs:0 written without a safe
// handler) does not apply.
#pragma warning(disable : 4733)

// The Windows x86 backend of the in-process runner (task 448, design
// 20261004-448). The guest runs on the calling host thread's own stack with
// that thread's real TEB in FS; the HLE sees a shadow TEB per guest thread;
// faults arrive through a vectored exception handler.
namespace re2dj::platform::native
{
namespace
{

// x86 TEB offsets.
constexpr std::uint32_t kTebExceptionList = 0x00;
constexpr std::uint32_t kTebStackBase = 0x04;
constexpr std::uint32_t kTebStackLimit = 0x08;
constexpr std::uint32_t kTebSelf = 0x18;
constexpr std::uint32_t kTebPeb = 0x30;
constexpr std::uint32_t kTebDeallocationStack = 0xE0C;
constexpr std::uint32_t kPebImageBase = 0x08;

constexpr std::uint32_t kThreadStackReserve = 1024 * 1024;
// The stack a guest exception is delivered on, one per host thread: the
// vectored handler runs right below the guest's stack pointer, where
// delivery writes the exception record, so it cannot deliver there itself.
constexpr std::size_t kDeliveryStackSize = 32 * 1024;

// The guest's registers and fault cause, handed from the vectored handler
// to GuestFaultTrampoline. Offsets are fixed for the asm.
struct PendingGuestFault
{
    std::uint32_t eax = 0;
    std::uint32_t ebx = 0;
    std::uint32_t ecx = 0;
    std::uint32_t edx = 0;
    std::uint32_t esi = 0;
    std::uint32_t edi = 0;
    std::uint32_t ebp = 0;
    std::uint32_t esp = 0;
    std::uint32_t eip = 0;
    std::uint32_t eflags = 0;
    NativeTrapRegisters trap;
    NativeGuestTrapCause cause;
    std::uint32_t status_code = 0;
};
static_assert(offsetof(PendingGuestFault, eax) == 0);
static_assert(offsetof(PendingGuestFault, ebx) == 4);
static_assert(offsetof(PendingGuestFault, ecx) == 8);
static_assert(offsetof(PendingGuestFault, edx) == 12);
static_assert(offsetof(PendingGuestFault, esi) == 16);
static_assert(offsetof(PendingGuestFault, edi) == 20);
static_assert(offsetof(PendingGuestFault, ebp) == 24);
static_assert(offsetof(PendingGuestFault, esp) == 28);
static_assert(offsetof(PendingGuestFault, eip) == 32);
static_assert(offsetof(PendingGuestFault, eflags) == 36);

// Per guest thread: the run in progress, where it escapes to, and how it
// ended. Each guest thread runs on a host thread of its own.
thread_local NativeProcessBootstrap::Impl* g_current_bootstrap = nullptr;
thread_local bool g_guest_active = false;
thread_local GuestEscapeFrame g_escape;
thread_local bool g_exit_requested = false;
thread_local std::uint32_t g_exit_code = 0;
thread_local NativeGuestFault g_fault;
thread_local PendingGuestFault g_pending;
alignas(16) thread_local std::uint8_t g_delivery_stack[kDeliveryStackSize];
thread_local std::uintptr_t g_delivery_stack_top = 0;

NativeFaultKind KindOf(DWORD code)
{
    switch (code)
    {
    case STATUS_ACCESS_VIOLATION:
    case STATUS_GUARD_PAGE_VIOLATION:
    case STATUS_IN_PAGE_ERROR:
        return NativeFaultKind::kAccessViolation;
    case STATUS_ILLEGAL_INSTRUCTION:
        return NativeFaultKind::kIllegalInstruction;
    case STATUS_PRIVILEGED_INSTRUCTION:
        return NativeFaultKind::kPrivilegedInstruction;
    case STATUS_BREAKPOINT:
        return NativeFaultKind::kBreakpoint;
    case STATUS_SINGLE_STEP:
        return NativeFaultKind::kSingleStep;
    case STATUS_INTEGER_DIVIDE_BY_ZERO:
    case STATUS_INTEGER_OVERFLOW:
        return NativeFaultKind::kDivide;
    default:
        return NativeFaultKind::kOther;
    }
}

// The x86 trap behind an exception code, as the shared SEH model takes it;
// false for one no trap stands for.
bool CauseOf(const EXCEPTION_RECORD& record, NativeGuestTrapCause* cause)
{
    *cause = {};
    switch (record.ExceptionCode)
    {
    case STATUS_SINGLE_STEP:
        cause->trap_number = kTrapDebug;
        return true;
    case STATUS_BREAKPOINT:
        cause->trap_number = kTrapBreakpoint;
        return true;
    case STATUS_PRIVILEGED_INSTRUCTION:
        cause->trap_number = kTrapGeneralProtection;
        return true;
    case STATUS_ILLEGAL_INSTRUCTION:
        cause->trap_number = kTrapInvalidOpcode;
        return true;
    case STATUS_INTEGER_DIVIDE_BY_ZERO:
        cause->trap_number = kTrapDivideError;
        return true;
    case STATUS_INTEGER_OVERFLOW:
        cause->trap_number = kTrapOverflow;
        return true;
    case STATUS_ACCESS_VIOLATION:
    {
        cause->trap_number = kTrapPageFault;
        const ULONG_PTR access = record.NumberParameters >= 1 ? record.ExceptionInformation[0] : 0;
        // A user-mode page fault: bit 2 user, bit 1 write, bit 4 fetch.
        cause->error_code = 0x04U | (access == 1 ? 0x02U : 0U) | (access == 8 ? 0x10U : 0U);
        cause->fault_address =
            record.NumberParameters >= 2 ? static_cast<std::uint32_t>(record.ExceptionInformation[1]) : 0;
        return true;
    }
    default:
        return false;
    }
}

NativeTrapRegisters ReadTrapRegisters(const CONTEXT& context)
{
    NativeTrapRegisters trap;
    trap.eip = context.Eip;
    trap.esp = context.Esp;
    trap.eax = context.Eax;
    trap.ebx = context.Ebx;
    trap.ecx = context.Ecx;
    trap.edx = context.Edx;
    trap.esi = context.Esi;
    trap.edi = context.Edi;
    trap.ebp = context.Ebp;
    trap.eflags = context.EFlags;
    trap.cs = context.SegCs;
    trap.ss = context.SegSs;
    trap.ds = context.SegDs;
    trap.es = context.SegEs;
    trap.fs = context.SegFs;
    trap.gs = context.SegGs;
    return trap;
}

void WriteTrapRegisters(const NativeTrapRegisters& trap, CONTEXT* context)
{
    context->Eip = trap.eip;
    context->Esp = trap.esp;
    context->Eax = trap.eax;
    context->Ebx = trap.ebx;
    context->Ecx = trap.ecx;
    context->Edx = trap.edx;
    context->Esi = trap.esi;
    context->Edi = trap.edi;
    context->Ebp = trap.ebp;
    context->EFlags = trap.eflags;
}

void RecordFault(std::uint32_t status_code,
                 NativeFaultKind kind,
                 const NativeTrapRegisters& registers,
                 const NativeGuestTrapCause& cause)
{
    g_fault = {};
    g_fault.status_code = status_code;
    g_fault.kind = kind;
    g_fault.instruction_pointer = registers.eip;
    g_fault.stack_pointer = registers.esp;
    g_fault.fault_address = cause.fault_address;
    g_fault.cpu_error_code = cause.error_code;
    g_fault.eax = registers.eax;
    g_fault.ebx = registers.ebx;
    g_fault.ecx = registers.ecx;
    g_fault.edx = registers.edx;
    g_fault.esi = registers.esi;
    g_fault.edi = registers.edi;
    g_fault.ebp = registers.ebp;
    g_fault.eflags = registers.eflags;
}

void WriteU32(std::uint8_t* bytes, std::size_t offset, std::uint32_t value)
{
    std::memcpy(bytes + offset, &value, sizeof(value));
}

void FillShadowTeb(std::uint8_t* teb, std::uint32_t self, std::uint32_t peb, std::uint32_t stack_base,
                   std::uint32_t stack_limit)
{
    WriteU32(teb, kTebExceptionList, 0xFFFFFFFFU);
    WriteU32(teb, kTebStackBase, stack_base);
    WriteU32(teb, kTebStackLimit, stack_limit);
    WriteU32(teb, kTebSelf, self);
    WriteU32(teb, kTebPeb, peb);
}

// The calling thread's whole stack: its base, and the bottom of the reserve
// (StackLimit moves up as pages commit).
void ReadThreadStack(std::uint32_t* base, std::uint32_t* limit)
{
    *base = __readfsdword(kTebStackBase);
    *limit = __readfsdword(kTebDeallocationStack);
}

void ResumeGuestFault(PendingGuestFault* pending);
LONG CALLBACK GuestExceptionHandler(EXCEPTION_POINTERS* pointers);

// Entered from the vectored handler on Execute's delivery stack with ECX =
// &g_pending: delivers the exception to the guest's SEH chain with host
// fs:0 = -1, then loads the registers delivery chose and jumps to the guest.
extern "C" __declspec(naked) void GuestFaultTrampoline()
{
    __asm
    {
        push dword ptr fs:[0]
        mov dword ptr fs:[0], 0FFFFFFFFh
        push ecx
        push ecx
        call ResumeGuestFault
        add esp, 4
        pop ecx
        pop dword ptr fs:[0]
        mov esp, [ecx + 28]
        push dword ptr [ecx + 32]
        push dword ptr [ecx + 36]
        push dword ptr [ecx + 8]
        mov eax, [ecx]
        mov ebx, [ecx + 4]
        mov edx, [ecx + 12]
        mov esi, [ecx + 16]
        mov edi, [ecx + 20]
        mov ebp, [ecx + 24]
        pop ecx
        popfd
        ret
    }
}

std::once_flag g_handler_once;

}  // namespace

struct NativeProcessBootstrap::Impl
{
    void* shadow_mapping = nullptr;
    std::uint32_t shadow_size = 0;
    std::uint32_t teb = 0;
    std::uint32_t peb = 0;
    std::uint32_t stack_base = 0;
    std::uint32_t stack_limit = 0;
    bool initialized = false;
    std::uint32_t image_base = 0;
    NativeGuestExceptionDispatcher exceptions;
    // The exception last delivered, reported if it goes unhandled.
    std::uint32_t delivered_status = 0;
    NativeGuestTrapCause delivered_cause;
    bool process_exited = false;
    std::uint32_t exit_code = 0;
    // The main guest thread: this host thread's stack, the shadow TEB above.
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
        }
        ReleaseNativeGuestExceptionDispatcher(&exceptions);
        HostUnmap(shadow_mapping, shadow_size);
    }

    bool Initialize(std::uint32_t image_base_value, std::string* error)
    {
        image_base = image_base_value;
        const std::uint32_t page_size = HostPageSize();
        if (initialized || page_size == 0)
        {
            *error = "invalid native process bootstrap state";
            return false;
        }
        shadow_size = page_size * 2;
        shadow_mapping = HostMapAnywhere(shadow_size, HostProtection::kReadWrite);
        if (shadow_mapping == nullptr)
        {
            *error = "cannot allocate the guest's shadow TEB and PEB";
            return false;
        }
        auto* shadow = static_cast<std::uint8_t*>(shadow_mapping);
        teb = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(shadow));
        peb = teb + page_size;
        ReadThreadStack(&stack_base, &stack_limit);
        FillShadowTeb(shadow, teb, peb, stack_base, stack_limit);
        WriteU32(shadow + page_size, kPebImageBase, image_base);
        std::call_once(g_handler_once, [] { AddVectoredExceptionHandler(1, &GuestExceptionHandler); });
        if (!CreateNativeGuestExceptionDispatcher(&exceptions, error))
        {
            return false;
        }
        main_thread.teb = teb;
        main_thread.fs_teb = __readfsdword(kTebSelf);
        main_thread.stack_limit = stack_limit;
        main_thread.stack_base = stack_base;
        BeginNativeGuestThreads(&main_thread);
        initialized = true;
        return true;
    }

    template <typename Function>
    static void __cdecl Body(void* function)
    {
        (*static_cast<Function*>(function))();
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
        g_fault = {};
        g_exit_requested = false;
        g_current_bootstrap = this;
        ResetNativeImportGateNesting();
        g_delivery_stack_top = reinterpret_cast<std::uintptr_t>(g_delivery_stack + sizeof(g_delivery_stack) - 16);
        g_guest_active = true;
        const std::uint32_t escaped = EnterGuestRun(&g_escape, &Body<Function>, &function);
        g_guest_active = false;
        g_current_bootstrap = nullptr;
        if (escaped == 0)
        {
            error->clear();
            return true;
        }
        if (g_exit_requested)
        {
            // The guest ended its process from an import; that is a normal
            // completion, not a fault.
            g_exit_requested = false;
            process_exited = true;
            exit_code = g_exit_code;
            error->clear();
            return true;
        }
        *fault = g_fault;
        error->clear();
        return false;
    }
};

namespace
{

// Whether the guest's SEH chain may see this exception: not the host's own
// stop stub or traps, and not the dispatcher's stop after every handler
// declined.
bool Deliverable(const NativeProcessBootstrap::Impl* bootstrap, const NativeTrapRegisters& registers)
{
    return bootstrap != nullptr && registers.eip != bootstrap->exceptions.stop && !IsNativeHostTrap(registers.eip) &&
           !IsNativeHostTrap(registers.eip - 1);
}

// On the delivery stack: builds the exception record and context below the
// guest's stack pointer and points the registers at the guest dispatcher, or
// ends the run with the fault when the chain cannot take it.
void ResumeGuestFault(PendingGuestFault* pending)
{
    NativeProcessBootstrap::Impl* bootstrap = g_current_bootstrap;
    NativeTrapRegisters registers = pending->trap;
    Win32ExceptionRecord32 record;
    Win32Context32 guest_context;
    const NativeGuestThread* thread = CurrentNativeGuestThread();
    const std::uint32_t stack_limit =
        thread != nullptr ? thread->stack_limit : (bootstrap != nullptr ? bootstrap->stack_limit : 0);
    const std::uint32_t stack_base =
        thread != nullptr ? thread->stack_base : (bootstrap != nullptr ? bootstrap->stack_base : 0);
    if (bootstrap == nullptr || !DescribeNativeGuestException(pending->cause, registers, &record, &guest_context) ||
        !DeliverNativeGuestException(&bootstrap->exceptions, stack_limit, stack_base, record, guest_context,
                                     &registers))
    {
        RecordFault(pending->status_code, KindOf(pending->status_code), pending->trap, pending->cause);
        EscapeGuestRun(&g_escape);
    }
    bootstrap->delivered_status = pending->status_code;
    bootstrap->delivered_cause = pending->cause;
    pending->eax = registers.eax;
    pending->ebx = registers.ebx;
    pending->ecx = registers.ecx;
    pending->edx = registers.edx;
    pending->esi = registers.esi;
    pending->edi = registers.edi;
    pending->ebp = registers.ebp;
    pending->esp = registers.esp;
    pending->eip = registers.eip;
    pending->eflags = registers.eflags;
}

LONG CALLBACK GuestExceptionHandler(EXCEPTION_POINTERS* pointers)
{
    // Host code's exceptions, and every other host thread's, take Windows'
    // usual path.
    if (!g_guest_active || NativeHostCodeRunning() || pointers == nullptr)
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    const EXCEPTION_RECORD& exception = *pointers->ExceptionRecord;
    CONTEXT* context = pointers->ContextRecord;
    const std::uint32_t code = static_cast<std::uint32_t>(exception.ExceptionCode);
    NativeGuestTrapCause cause;
    const bool cpu_trap = CauseOf(exception, &cause);
    NativeTrapRegisters trap = ReadTrapRegisters(*context);
    // The shared code expects the stop after INT3, as Linux reports it;
    // Windows reports the byte itself.
    if (code == STATUS_BREAKPOINT)
    {
        trap.eip += 1;
    }
    const bool debug_trap = code == STATUS_SINGLE_STEP || code == STATUS_BREAKPOINT;
    if ((debug_trap && HandleNativeInstructionTraceTrap(&trap)) ||
        (code == STATUS_PRIVILEGED_INSTRUCTION && HandleNativeLegacyIoTrap(&trap)))
    {
        WriteTrapRegisters(trap, context);
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    NativeProcessBootstrap::Impl* bootstrap = g_current_bootstrap;
    if (cpu_trap && Deliverable(bootstrap, trap))
    {
        // Delivery writes below the guest's stack pointer, where this handler
        // is running; it happens on the delivery stack once this returns.
        g_pending = {};
        g_pending.trap = trap;
        g_pending.cause = cause;
        g_pending.status_code = code;
        context->Esp = static_cast<DWORD>(g_delivery_stack_top);
        context->Eip = static_cast<DWORD>(reinterpret_cast<std::uintptr_t>(&GuestFaultTrampoline));
        context->Ecx = static_cast<DWORD>(reinterpret_cast<std::uintptr_t>(&g_pending));
        context->EFlags &= ~0x100U;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    // Nothing takes it: report the fault, or, when the guest dispatcher
    // stopped after every handler declined, the exception it carried.
    Win32ExceptionRecord32 record;
    Win32Context32 guest_context;
    if (bootstrap != nullptr && ReadNativeGuestUnhandledException(bootstrap->exceptions, trap, &record, &guest_context))
    {
        NativeTrapRegisters original = trap;
        original.eip = record.exception_address;
        original.esp = guest_context.esp;
        original.eax = guest_context.eax;
        original.ebx = guest_context.ebx;
        original.ecx = guest_context.ecx;
        original.edx = guest_context.edx;
        original.esi = guest_context.esi;
        original.edi = guest_context.edi;
        original.ebp = guest_context.ebp;
        original.eflags = guest_context.eflags;
        RecordFault(bootstrap->delivered_status, KindOf(bootstrap->delivered_status), original,
                    bootstrap->delivered_cause);
    }
    else
    {
        RecordFault(code, KindOf(code), trap, cause);
    }
    context->Eip = static_cast<DWORD>(reinterpret_cast<std::uintptr_t>(&EscapeGuestRun));
    context->Ecx = static_cast<DWORD>(reinterpret_cast<std::uintptr_t>(&g_escape));
    context->EFlags &= ~0x100U;
    return EXCEPTION_CONTINUE_EXECUTION;
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
    return impl_->Execute([&]() { CallGuestTls(callback, image_base); }, fault, error);
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
    if (!impl_->Execute([&]() { *result = CallGuestEntry(entry); }, fault, error))
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

// A guest thread other than the main one: its host thread and shadow TEB,
// which outlive it only when it ended the process.
struct SecondaryThread
{
    NativeGuestThread thread;
    NativeGuestThreadStart start;
    NativeProcessBootstrap::Impl* bootstrap = nullptr;
    NativeImportGateHandler handler = nullptr;
    void* handler_context = nullptr;
    void* shadow_mapping = nullptr;
    std::uint32_t shadow_size = 0;
    // Set once the host thread has recorded its stack in the shadow TEB.
    HANDLE ready = nullptr;

    void Release()
    {
        HostUnmap(shadow_mapping, shadow_size);
        shadow_mapping = nullptr;
        if (ready != nullptr)
        {
            CloseHandle(ready);
            ready = nullptr;
        }
    }
};

DWORD WINAPI RunSecondaryThread(void* parameter)
{
    auto* secondary = static_cast<SecondaryThread*>(parameter);
    NativeGuestThread& thread = secondary->thread;
    ReadThreadStack(&thread.stack_base, &thread.stack_limit);
    thread.fs_teb = __readfsdword(kTebSelf);
    auto* shadow = static_cast<std::uint8_t*>(secondary->shadow_mapping);
    WriteU32(shadow, kTebStackBase, thread.stack_base);
    WriteU32(shadow, kTebStackLimit, thread.stack_limit);
    SetEvent(secondary->ready);
    BindNativeGuestThread(&thread);
    AcquireNativeGuestLock();
    ConfigureNativeImportGateHandler(secondary->handler, secondary->handler_context);
    ConfigureNativeImportGateStackRange(thread.stack_limit, thread.stack_base);
    NativeGuestTermination termination;
    std::uint32_t exit_code = 0;
    std::string error;
    const bool returned = secondary->bootstrap->Execute(
        [&]() { exit_code = CallGuestThread(secondary->start.start, secondary->start.parameter); },
        &termination.fault, &error);
    if (returned && !secondary->bootstrap->process_exited)
    {
        if (secondary->start.on_exit != nullptr)
        {
            secondary->start.on_exit(secondary->start.exit_context, secondary->start.token, exit_code);
        }
        ClearNativeImportGateHandler();
        RemoveNativeGuestThreadAndRelease(&thread);
        secondary->Release();
        delete secondary;
        return 0;
    }
    if (returned)
    {
        termination.process_exited = true;
        termination.exit_code = secondary->bootstrap->exit_code;
    }
    TerminateNativeGuestProcess(termination);
    return 0;
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
    auto* secondary = new SecondaryThread;
    secondary->start = start;
    secondary->bootstrap = bootstrap;
    secondary->shadow_size = HostPageSize();
    secondary->shadow_mapping = HostMapAnywhere(secondary->shadow_size, HostProtection::kReadWrite);
    secondary->ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (secondary->shadow_mapping == nullptr || secondary->ready == nullptr)
    {
        secondary->Release();
        delete secondary;
        if (error != nullptr) *error = "cannot allocate a guest thread's shadow TEB";
        return false;
    }
    NativeGuestThread& thread = secondary->thread;
    thread.teb = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(secondary->shadow_mapping));
    thread.width_state = secondary;
    FillShadowTeb(static_cast<std::uint8_t*>(secondary->shadow_mapping), thread.teb, bootstrap->peb, 0, 0);
    CurrentNativeImportGateHandler(&secondary->handler, &secondary->handler_context);
    AddNativeGuestThread(&thread);
    HANDLE host_thread = CreateThread(nullptr, kThreadStackReserve, &RunSecondaryThread, secondary,
                                      STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
    if (host_thread == nullptr)
    {
        // The thread never runs; nothing of it can be reclaimed safely now
        // that the scheduler knows it, so it stays registered and idle.
        if (error != nullptr) *error = "cannot start a guest thread's host thread";
        return false;
    }
    CloseHandle(host_thread);
    // The stack bounds come from the new thread itself; wait for them so the
    // thread is complete when the caller sees it.
    WaitForSingleObject(secondary->ready, INFINITE);
    *teb = thread.teb;
    if (error != nullptr) error->clear();
    return true;
}

// Nothing of the transition is kept outside the host thread itself: each
// guest thread has its own TEB and stack.
void SaveNativeGuestTransition(NativeGuestThread*) {}
void RestoreNativeGuestTransition(NativeGuestThread*) {}

void AbandonNativeGuestRun(const NativeGuestTermination& termination)
{
    if (g_current_bootstrap == nullptr || !g_guest_active)
    {
        std::abort();
    }
    if (termination.process_exited)
    {
        g_exit_code = termination.exit_code;
        g_exit_requested = true;
    }
    else
    {
        g_fault = termination.fault;
    }
    EscapeGuestRun(&g_escape);
}

void ExitNativeGuestProcess(std::uint32_t exit_code)
{
    // Called from the import bridge, on the guest thread's own stack.
    if (g_current_bootstrap == nullptr || !g_guest_active)
    {
        std::abort();
    }
    g_exit_code = exit_code;
    g_exit_requested = true;
    EscapeGuestRun(&g_escape);
}

}  // namespace re2dj::platform::native
