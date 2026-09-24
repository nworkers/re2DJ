#ifndef RE2DJ_PLATFORM_LINUX_X64_NATIVE_COMPAT_MODE_H_
#define RE2DJ_PLATFORM_LINUX_X64_NATIVE_COMPAT_MODE_H_

#include <cstdint>
#include <string>
#include <vector>

#include "../native_guest_fault.h"
#include "../native_import_gate.h"
#include "../native_low_memory.h"
#include "native_compat_mode_transition.h"

namespace re2dj::platform::linux
{

// The process-wide transition pages, created on first use and kept for the
// process lifetime because guest thunks bake their addresses in. Each
// returns 0 when the pages cannot be created.
std::uint32_t NativeCompatImportBridgeAddress();
std::uint32_t NativeCompatImportCleanupAddress();

// The handler installed through ConfigureNativeImportGateHandler, used by a
// run whose call names no handler of its own.
struct NativeImportGateConfiguration
{
    NativeImportGateHandler handler = nullptr;
    void* context = nullptr;
};
NativeImportGateConfiguration ConfiguredNativeImportGate();

struct NativeCompatModeOptions
{
    // Restores the host FS base with arch_prctl even when the CPU and kernel
    // offer wrfsbase, so both restore paths can be exercised.
    bool force_arch_prctl = false;
};

struct NativeCompatModeCall
{
    std::uint32_t entry = 0;
    // Pushed right to left, so arguments[0] sits just above the return address.
    std::vector<std::uint32_t> arguments;
    // Falls back to ConfiguredNativeImportGate() when null.
    NativeImportGateHandler handler = nullptr;
    void* handler_context = nullptr;
    // Test seam replacing NativeCompatEnterGuest; it must forward to it.
    NativeCompatEnterFunction enter = nullptr;
};

struct NativeCompatModeRunResult
{
    std::uint32_t eax = 0;
    std::uint32_t edx = 0;
    std::uint32_t entry_stack_pointer = 0;
    // Set when an import ended the guest process (ExitNativeGuestProcess);
    // eax and edx are then meaningless.
    bool process_exited = false;
    std::uint32_t exit_code = 0;
};

// Runs 32-bit guest code in compatibility mode inside this x86-64 process.
// One runtime per process and thread; runs do not nest.
class NativeCompatModeRuntime
{
public:
    NativeCompatModeRuntime();
    ~NativeCompatModeRuntime();

    NativeCompatModeRuntime(const NativeCompatModeRuntime&) = delete;
    NativeCompatModeRuntime& operator=(const NativeCompatModeRuntime&) = delete;

    // Rejects hosts where compatibility mode is unavailable with an error
    // starting "Linux x64 compatibility mode unavailable".
    bool Initialize(std::uint32_t image_base,
                    const NativeCompatModeOptions& options,
                    std::string* error);

    // Returns true when the guest returned; returns false with fault filled
    // and error empty when the guest raised a signal, or with error set when
    // the call was invalid.
    bool Run(const NativeCompatModeCall& call,
             NativeCompatModeRunResult* result,
             NativeGuestFault* fault,
             std::string* error);

    std::uint32_t ImportBridgeAddress() const;
    std::uint32_t ImportCleanupAddress() const;
    std::uint32_t Teb() const;
    std::uint32_t Peb() const;
    std::uint32_t GuestStackBase() const;
    std::uint32_t GuestStackLimit() const;
    std::uint16_t GuestFsSelector() const;
    bool UsesFsGsBase() const;
    // Guest SEH dispatches that resumed with ExceptionContinueExecution.
    std::uint32_t SehDispatchCount() const;
    std::uint32_t LastSehHandler() const;
    std::uint32_t LastSehResumedEip() const;

    struct Impl;

private:
    Impl* impl_ = nullptr;
};

// The host FS base as the transition code saves and restores it.
std::uint64_t ReadNativeHostFsBase(bool use_fsgsbase);

}  // namespace re2dj::platform::linux

#endif  // RE2DJ_PLATFORM_LINUX_X64_NATIVE_COMPAT_MODE_H_
