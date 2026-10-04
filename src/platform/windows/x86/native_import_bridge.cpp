#include "../../native/native_import_bridge.h"
#include "../../native/native_guest_threads.h"
#include "../../native/native_instruction_trace.h"
#include "../../native/native_process_bootstrap.h"
#include "native_guest_transition.h"

#include <intrin.h>

#include <cstddef>
#include <cstring>

// The asm here moves SEH chains between fs:0 and the shadow TEB rather than
// registering handlers of its own, so C4733 (fs:0 written without a safe
// handler) does not apply.
#pragma warning(disable : 4733)

namespace
{

thread_local re2dj::platform::native::NativeImportGateHandler import_gate_handler = nullptr;
thread_local void* import_gate_context = nullptr;
// Every thunk reads this one word right after the bridge returns, so it is
// shared by the guest threads; only the one holding the guest lock runs.
std::uint32_t import_gate_cleanup_bytes = 0;
thread_local std::uint32_t import_gate_stack_base = 0;
thread_local std::uint32_t import_gate_stack_limit = 0;
// Imports being handled on this thread; guest calls need at least one.
thread_local std::uint32_t import_gate_depth = 0;
// Whether host code runs for an import now; cleared around guest calls.
thread_local bool host_code_running = false;

// Guest stack kept free below a guest call's arguments for the callee.
constexpr std::uint32_t kGuestCallReserve = 16 * 1024;

// The shadow TEB's ExceptionList word of the calling guest thread, where the
// guest's SEH chain waits while host code owns fs:0 (design 448).
std::uint32_t* ShadowExceptionList()
{
    const re2dj::platform::native::NativeGuestThread* thread = re2dj::platform::native::CurrentNativeGuestThread();
    if (thread == nullptr || thread->teb == 0)
    {
        return nullptr;
    }
    return reinterpret_cast<std::uint32_t*>(static_cast<std::uintptr_t>(thread->teb));
}

}  // namespace

// Runs with fs:0 = -1; exception_list is the guest chain the bridge took
// off fs:0, which goes back on when the bridge returns.
extern "C" std::uint64_t __cdecl NativeImportGateBridgeImpl(std::uint32_t gate_address,
                                                            std::uint32_t* return_slot,
                                                            std::uint32_t* exception_list)
{
    std::uint32_t* shadow = ShadowExceptionList();
    if (shadow != nullptr)
    {
        *shadow = *exception_list;
    }
    std::uint32_t return_address = 0;
    std::memcpy(&return_address, return_slot, sizeof(return_address));

    re2dj::platform::native::NativeImportGateEvent event;
    event.gate_address = gate_address;
    event.instruction_pointer = return_address;
    event.stack_pointer = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(return_slot));
    event.guest_stack_base = import_gate_stack_base;
    event.guest_stack_limit = import_gate_stack_limit;
    re2dj::platform::native::NativeImportGateResult result;
    ++import_gate_depth;
    const bool outer_host_code_running = host_code_running;
    host_code_running = true;
    // Another guest thread waiting for the lock runs first.
    re2dj::platform::native::YieldNativeGuestThread();
    const bool handled = import_gate_handler != nullptr && import_gate_handler(event, &result, import_gate_context);
    host_code_running = outer_host_code_running;
    --import_gate_depth;
    // The handler may have changed the chain (RtlUnwind), and a thread
    // switch inside it may have moved this thread's shadow; read it again.
    shadow = ShadowExceptionList();
    if (shadow != nullptr)
    {
        *exception_list = *shadow;
    }
    if (!handled)
    {
        import_gate_cleanup_bytes = 0;
        re2dj::platform::native::ResumeNativeInstructionTrace(return_address);
        return 0;
    }
    if (result.exit_process)
    {
        re2dj::platform::native::ExitNativeGuestProcess(result.exit_code);
    }
    import_gate_cleanup_bytes = result.stack_bytes_to_pop;
    re2dj::platform::native::ResumeNativeInstructionTrace(return_address);
    return (static_cast<std::uint64_t>(result.edx) << 32) | result.eax;
}

namespace
{

// stdcall(gate): the thunk pushes the gate and calls here; [ebp + 12] is the
// guest's own return address. The guest chain moves from fs:0 to a stack
// slot for the call and back.
extern "C" __declspec(naked) std::uint64_t __stdcall NativeImportGateBridge(std::uint32_t)
{
    __asm
    {
        push ebp
        mov ebp, esp
        push dword ptr fs:[0]
        mov dword ptr fs:[0], 0FFFFFFFFh
        lea eax, [ebp - 4]
        push eax
        lea eax, [ebp + 12]
        push eax
        push dword ptr [ebp + 8]
        call NativeImportGateBridgeImpl
        add esp, 12
        mov ecx, [ebp - 4]
        mov fs:[0], ecx
        mov esp, ebp
        pop ebp
        ret 4
    }
}

}  // namespace

bool re2dj::platform::native::CallNativeGuestStdcall(std::uint32_t function,
                                                    std::span<const std::uint32_t> arguments,
                                                    std::span<std::uint8_t> data,
                                                    int data_argument,
                                                    std::uint32_t* eax,
                                                    std::string* error)
{
    // The handler runs on the guest stack (the thread's own), so the data and
    // the call go on the current stack.
    alignas(16) std::uint8_t data_area[kNativeGuestCallMaximumData];
    std::uint32_t words[16] = {};
    const auto stack_pointer = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(_AddressOfReturnAddress()));
    std::uint32_t* shadow = ShadowExceptionList();
    if (import_gate_depth == 0 || function == 0 || shadow == nullptr)
    {
        *error = "no guest import is being handled";
        return false;
    }
    if (arguments.size() > std::size(words) || data.size() > sizeof(data_area) ||
        (!data.empty() && (data_argument < 0 || static_cast<std::size_t>(data_argument) >= arguments.size())))
    {
        *error = "guest call shape is invalid";
        return false;
    }
    if (stack_pointer < import_gate_stack_limit ||
        stack_pointer - import_gate_stack_limit < kGuestCallReserve + sizeof(data_area))
    {
        *error = "guest stack has no room for a guest call";
        return false;
    }
    std::memcpy(words, arguments.data(), arguments.size_bytes());
    if (!data.empty())
    {
        std::memcpy(data_area, data.data(), data.size());
        words[data_argument] = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(data_area));
    }
    host_code_running = false;
    *eax = CallGuestStdcallWords(function, words, static_cast<std::uint32_t>(arguments.size()), shadow);
    host_code_running = true;
    if (!data.empty())
    {
        std::memcpy(data.data(), data_area, data.size());
    }
    error->clear();
    return true;
}

bool re2dj::platform::native::ConfigureNativeImportGateHandler(NativeImportGateHandler handler, void* context)
{
    if (handler == nullptr)
    {
        return false;
    }
    import_gate_handler = handler;
    import_gate_context = context;
    import_gate_cleanup_bytes = 0;
    return true;
}

void re2dj::platform::native::ConfigureNativeImportGateStackRange(std::uint32_t stack_limit, std::uint32_t stack_base)
{
    import_gate_stack_limit = stack_limit;
    import_gate_stack_base = stack_base;
}

void re2dj::platform::native::CurrentNativeImportGateHandler(NativeImportGateHandler* handler, void** context)
{
    *handler = import_gate_handler;
    *context = import_gate_context;
}

void re2dj::platform::native::ClearNativeImportGateHandler()
{
    import_gate_handler = nullptr;
    import_gate_context = nullptr;
    import_gate_cleanup_bytes = 0;
    import_gate_stack_base = 0;
    import_gate_stack_limit = 0;
    import_gate_depth = 0;
    host_code_running = false;
}

bool re2dj::platform::native::NativeHostCodeRunning()
{
    return host_code_running;
}

void re2dj::platform::native::ResetNativeImportGateNesting()
{
    import_gate_depth = 0;
    host_code_running = false;
}

std::uintptr_t re2dj::platform::native::NativeImportGateBridgeAddress()
{
    return reinterpret_cast<std::uintptr_t>(&NativeImportGateBridge);
}

std::uintptr_t re2dj::platform::native::NativeImportGateCleanupAddress()
{
    return reinterpret_cast<std::uintptr_t>(&import_gate_cleanup_bytes);
}
