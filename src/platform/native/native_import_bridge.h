#ifndef RE2DJ_PLATFORM_NATIVE_NATIVE_IMPORT_BRIDGE_H_
#define RE2DJ_PLATFORM_NATIVE_NATIVE_IMPORT_BRIDGE_H_

#include <cstdint>
#include <span>
#include <string>

#include "native_import_gate.h"

namespace re2dj::platform::native
{

// The host side of guest import thunks. x86/native_import_bridge.cpp calls
// the handler directly from i386 code; x64/native_import_bridge.cpp reaches
// it through the compatibility-mode transition page.
bool ConfigureNativeImportGateHandler(NativeImportGateHandler handler, void* context);
void ConfigureNativeImportGateStackRange(std::uint32_t stack_limit,
                                         std::uint32_t stack_base);
void ClearNativeImportGateHandler();
// The handler imports on this thread reach, which a guest thread it starts
// inherits.
void CurrentNativeImportGateHandler(NativeImportGateHandler* handler, void** context);

// Whether host code is running for an import right now, as opposed to guest
// code (a guest call from an import runs guest code again). On an i386 host
// both run in the same mode on the guest stack, so this is what tells a guest
// fault, delivered to the guest's SEH, from a host one.
bool NativeHostCodeRunning();
// Forgets imports a guest run left unfinished, such as one that ended the
// process by jumping out; called as each guest run starts.
void ResetNativeImportGateNesting();

// Guest-addressable (below 4 GiB) addresses baked into import thunks: the
// one-argument stdcall bridge and the cleanup byte count it leaves behind.
// Zero means the bridge is unavailable.
std::uintptr_t NativeImportGateBridgeAddress();
std::uintptr_t NativeImportGateCleanupAddress();

// The most bytes of data CallNativeGuestStdcall places on the guest stack.
inline constexpr std::size_t kNativeGuestCallMaximumData = 512;

// Calls a guest stdcall function from inside an import handler, the way
// Windows calls a window procedure: on the guest stack below the innermost
// import's frame, with the guest's FS. When data is not empty it is placed on
// the guest stack first, the argument at data_argument becomes its address,
// and the guest's changes to it are copied back. Fails without calling when
// no import is being handled or the guest stack has no room.
bool CallNativeGuestStdcall(std::uint32_t function,
                            std::span<const std::uint32_t> arguments,
                            std::span<std::uint8_t> data,
                            int data_argument,
                            std::uint32_t* eax,
                            std::string* error);

}  // namespace re2dj::platform::native

#endif  // RE2DJ_PLATFORM_NATIVE_NATIVE_IMPORT_BRIDGE_H_
