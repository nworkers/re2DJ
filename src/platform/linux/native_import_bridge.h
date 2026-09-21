#ifndef RE2DJ_PLATFORM_LINUX_NATIVE_IMPORT_BRIDGE_H_
#define RE2DJ_PLATFORM_LINUX_NATIVE_IMPORT_BRIDGE_H_

#include <cstdint>

namespace re2dj::platform::linux
{

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
};

using NativeImportGateHandler = bool (*)(const NativeImportGateEvent& event,
                                         NativeImportGateResult* result,
                                         void* context);

bool ConfigureNativeImportGateHandler(NativeImportGateHandler handler, void* context);
void ConfigureNativeImportGateStackRange(std::uint32_t stack_limit,
                                         std::uint32_t stack_base);
void ClearNativeImportGateHandler();

std::uintptr_t NativeImportGateBridgeAddress();
std::uintptr_t NativeImportGateCleanupAddress();

}  // namespace re2dj::platform::linux

#endif  // RE2DJ_PLATFORM_LINUX_NATIVE_IMPORT_BRIDGE_H_
