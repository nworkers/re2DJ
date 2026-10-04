#ifndef RE2DJ_PLATFORM_NATIVE_NATIVE_IMPORT_GATE_H_
#define RE2DJ_PLATFORM_NATIVE_NATIVE_IMPORT_GATE_H_

#include <cstdint>

namespace re2dj::platform::native
{

// The contract between a guest import thunk and the host handler that
// completes it. Every field is a 32-bit guest value, so the same handler
// serves the i386 in-process bridge and the x86-64 compatibility-mode landing.
struct NativeImportGateEvent
{
    std::uint32_t gate_address = 0;
    std::uint32_t instruction_pointer = 0;
    std::uint32_t stack_pointer = 0;
    std::uint32_t guest_stack_base = 0;
    std::uint32_t guest_stack_limit = 0;
};

struct NativeImportGateResult
{
    std::uint32_t eax = 0;
    std::uint32_t edx = 0;
    std::uint32_t stack_bytes_to_pop = 0;
    // The bridge does not return to the guest; the run ends with exit_code
    // through ExitNativeGuestProcess.
    bool exit_process = false;
    std::uint32_t exit_code = 0;
};

using NativeImportGateHandler = bool (*)(const NativeImportGateEvent& event,
                                         NativeImportGateResult* result,
                                         void* context);

}  // namespace re2dj::platform::native

#endif  // RE2DJ_PLATFORM_NATIVE_NATIVE_IMPORT_GATE_H_
