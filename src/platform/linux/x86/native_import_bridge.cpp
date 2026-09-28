#include "../native_import_bridge.h"
#include "../native_guest_threads.h"
#include "../native_instruction_trace.h"

#include "../native_process_bootstrap.h"

#include <cstddef>
#include <cstring>

namespace
{

thread_local re2dj::platform::linux::NativeImportGateHandler import_gate_handler = nullptr;
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

extern "C" std::uint16_t g_native_host_gs_selector;

extern "C" std::uint64_t NativeImportGateBridgeImpl(
    std::uint32_t gate_address,
    std::uint32_t* return_slot)
{
    std::uint32_t return_address = 0;
    std::memcpy(&return_address, return_slot, sizeof(return_address));

    re2dj::platform::linux::NativeImportGateEvent event;
    event.gate_address = gate_address;
    event.instruction_pointer = return_address;
    event.stack_pointer = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(return_slot));
    event.guest_stack_base = import_gate_stack_base;
    event.guest_stack_limit = import_gate_stack_limit;
    re2dj::platform::linux::NativeImportGateResult result;
    ++import_gate_depth;
    const bool outer_host_code_running = host_code_running;
    host_code_running = true;
    // Another guest thread waiting for the lock runs first.
    re2dj::platform::linux::YieldNativeGuestThread();
    const bool handled =
        import_gate_handler != nullptr && import_gate_handler(event, &result, import_gate_context);
    host_code_running = outer_host_code_running;
    --import_gate_depth;
    if (!handled)
    {
        import_gate_cleanup_bytes = 0;
        re2dj::platform::linux::ResumeNativeInstructionTrace(return_address);
        return 0;
    }
    if (result.exit_process)
    {
        re2dj::platform::linux::ExitNativeGuestProcess(result.exit_code);
    }
    import_gate_cleanup_bytes = result.stack_bytes_to_pop;
    re2dj::platform::linux::ResumeNativeInstructionTrace(return_address);
    return (static_cast<std::uint64_t>(result.edx) << 32) | result.eax;
}

extern "C" __attribute__((naked)) std::uint64_t NativeImportGateBridge(
    std::uint32_t)
{
    __asm__ volatile(
        "pushl %ebp\n"
        "movl %esp, %ebp\n"
        "pushl %ebx\n"
        "xorl %ebx, %ebx\n"
        "movw %gs, %bx\n"
        "pushl %ebx\n"
        "call 1f\n"
        "1:\n"
        "popl %ebx\n"
        "movw (g_native_host_gs_selector - 1b)(%ebx), %bx\n"
        "testw %bx, %bx\n"
        "jz 2f\n"
        "movw %bx, %gs\n"
        "2:\n"
        "leal 12(%ebp), %eax\n"
        "pushl %eax\n"
        "pushl 8(%ebp)\n"
        "call NativeImportGateBridgeImpl\n"
        "addl $8, %esp\n"
        "popl %ebx\n"
        "movw %bx, %gs\n"
        "popl %ebx\n"
        "movl %ebp, %esp\n"
        "popl %ebp\n"
        "ret $4\n");
}

// Copies count arguments below a 16-byte aligned stack pointer and calls the
// stdcall function; the caller's registers and stack are restored from ebp
// whatever the callee pops.
extern "C" __attribute__((naked)) std::uint32_t CallGuestStdcallWords(
    std::uint32_t, const std::uint32_t*, std::uint32_t)
{
    __asm__ volatile(
        "pushl %ebp\n"
        "movl %esp, %ebp\n"
        "pushl %ebx\n"
        "pushl %esi\n"
        "pushl %edi\n"
        "movl 16(%ebp), %ecx\n"
        "movl 12(%ebp), %esi\n"
        "leal 0(,%ecx,4), %eax\n"
        "subl %eax, %esp\n"
        "andl $-16, %esp\n"
        "movl %esp, %edi\n"
        "cld\n"
        "rep movsl\n"
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
        "leal -12(%ebp), %esp\n"
        "popl %edi\n"
        "popl %esi\n"
        "popl %ebx\n"
        "popl %ebp\n"
        "ret\n");
}

}  // namespace

bool re2dj::platform::linux::CallNativeGuestStdcall(std::uint32_t function,
                                                    std::span<const std::uint32_t> arguments,
                                                    std::span<std::uint8_t> data,
                                                    int data_argument,
                                                    std::uint32_t* eax,
                                                    std::string* error)
{
    // The handler already runs on the guest stack with the guest's FS, so the
    // data and the call go on the current stack.
    alignas(16) std::uint8_t data_area[kNativeGuestCallMaximumData];
    std::uint32_t words[16] = {};
    const auto stack_pointer = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
    if (import_gate_depth == 0 || function == 0)
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
    *eax = CallGuestStdcallWords(function, words, static_cast<std::uint32_t>(arguments.size()));
    host_code_running = true;
    if (!data.empty())
    {
        std::memcpy(data.data(), data_area, data.size());
    }
    error->clear();
    return true;
}

bool re2dj::platform::linux::ConfigureNativeImportGateHandler(NativeImportGateHandler handler,
                                                              void* context)
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

void re2dj::platform::linux::ConfigureNativeImportGateStackRange(
    std::uint32_t stack_limit,
    std::uint32_t stack_base)
{
    import_gate_stack_limit = stack_limit;
    import_gate_stack_base = stack_base;
}

void re2dj::platform::linux::CurrentNativeImportGateHandler(NativeImportGateHandler* handler,
                                                            void** context)
{
    *handler = import_gate_handler;
    *context = import_gate_context;
}

void re2dj::platform::linux::ClearNativeImportGateHandler()
{
    import_gate_handler = nullptr;
    import_gate_context = nullptr;
    import_gate_cleanup_bytes = 0;
    import_gate_stack_base = 0;
    import_gate_stack_limit = 0;
    import_gate_depth = 0;
    host_code_running = false;
}

bool re2dj::platform::linux::NativeHostCodeRunning()
{
    return host_code_running;
}

void re2dj::platform::linux::ResetNativeImportGateNesting()
{
    import_gate_depth = 0;
    host_code_running = false;
}

std::uintptr_t re2dj::platform::linux::NativeImportGateBridgeAddress()
{
    return reinterpret_cast<std::uintptr_t>(&NativeImportGateBridge);
}

std::uintptr_t re2dj::platform::linux::NativeImportGateCleanupAddress()
{
    return reinterpret_cast<std::uintptr_t>(&import_gate_cleanup_bytes);
}
