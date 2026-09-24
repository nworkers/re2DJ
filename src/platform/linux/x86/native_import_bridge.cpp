#include "../native_import_bridge.h"
#include "../native_instruction_trace.h"

#include "../native_process_bootstrap.h"

#include <cstddef>
#include <cstring>

namespace
{

thread_local re2dj::platform::linux::NativeImportGateHandler import_gate_handler = nullptr;
thread_local void* import_gate_context = nullptr;
thread_local std::uint32_t import_gate_cleanup_bytes = 0;
thread_local std::uint32_t import_gate_stack_base = 0;
thread_local std::uint32_t import_gate_stack_limit = 0;

extern "C" __attribute__((noinline, stdcall)) std::uint64_t NativeImportGateBridge(
    std::uint32_t gate_address)
{
    auto* frame = static_cast<std::uint8_t*>(__builtin_frame_address(0));
    std::uint8_t* bridge_return_slot = frame + sizeof(void*);
    std::uint8_t* return_slot = bridge_return_slot + 2 * sizeof(void*);
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
    if (import_gate_handler == nullptr ||
        !import_gate_handler(event, &result, import_gate_context))
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

}  // namespace

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

void re2dj::platform::linux::ClearNativeImportGateHandler()
{
    import_gate_handler = nullptr;
    import_gate_context = nullptr;
    import_gate_cleanup_bytes = 0;
    import_gate_stack_base = 0;
    import_gate_stack_limit = 0;
}

std::uintptr_t re2dj::platform::linux::NativeImportGateBridgeAddress()
{
    return reinterpret_cast<std::uintptr_t>(&NativeImportGateBridge);
}

std::uintptr_t re2dj::platform::linux::NativeImportGateCleanupAddress()
{
    return reinterpret_cast<std::uintptr_t>(&import_gate_cleanup_bytes);
}
