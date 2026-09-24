#include "../native_import_bridge.h"

#include "native_compat_mode.h"

namespace re2dj::platform::linux
{
namespace
{

// Read only by host code after the transition has restored the host FS base,
// so thread-local storage is safe here.
thread_local NativeImportGateHandler import_gate_handler = nullptr;
thread_local void* import_gate_context = nullptr;

}  // namespace

bool ConfigureNativeImportGateHandler(NativeImportGateHandler handler, void* context)
{
    if (handler == nullptr)
    {
        return false;
    }
    import_gate_handler = handler;
    import_gate_context = context;
    return true;
}

void ConfigureNativeImportGateStackRange(std::uint32_t, std::uint32_t)
{
    // The compatibility-mode runtime reports its own guest stack range in
    // every import event, so there is nothing to record.
}

void ClearNativeImportGateHandler()
{
    import_gate_handler = nullptr;
    import_gate_context = nullptr;
}

std::uintptr_t NativeImportGateBridgeAddress()
{
    return NativeCompatImportBridgeAddress();
}

std::uintptr_t NativeImportGateCleanupAddress()
{
    return NativeCompatImportCleanupAddress();
}

NativeImportGateConfiguration ConfiguredNativeImportGate()
{
    return {import_gate_handler, import_gate_context};
}

}  // namespace re2dj::platform::linux
