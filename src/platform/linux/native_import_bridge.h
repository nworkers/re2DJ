#ifndef RE2DJ_PLATFORM_LINUX_NATIVE_IMPORT_BRIDGE_H_
#define RE2DJ_PLATFORM_LINUX_NATIVE_IMPORT_BRIDGE_H_

#include <cstdint>

#include "native_import_gate.h"

namespace re2dj::platform::linux
{

// The host side of guest import thunks. x86/native_import_bridge.cpp calls
// the handler directly from i386 code; x64/native_import_bridge.cpp reaches
// it through the compatibility-mode transition page.
bool ConfigureNativeImportGateHandler(NativeImportGateHandler handler, void* context);
void ConfigureNativeImportGateStackRange(std::uint32_t stack_limit,
                                         std::uint32_t stack_base);
void ClearNativeImportGateHandler();

// Guest-addressable (below 4 GiB) addresses baked into import thunks: the
// one-argument stdcall bridge and the cleanup byte count it leaves behind.
// Zero means the bridge is unavailable.
std::uintptr_t NativeImportGateBridgeAddress();
std::uintptr_t NativeImportGateCleanupAddress();

}  // namespace re2dj::platform::linux

#endif  // RE2DJ_PLATFORM_LINUX_NATIVE_IMPORT_BRIDGE_H_
