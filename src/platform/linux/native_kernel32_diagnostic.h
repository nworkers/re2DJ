#ifndef RE2DJ_PLATFORM_LINUX_NATIVE_KERNEL32_DIAGNOSTIC_H_
#define RE2DJ_PLATFORM_LINUX_NATIVE_KERNEL32_DIAGNOSTIC_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "native_guest_module_set.h"
#include "native_import_bridge.h"
#include "native_import_thunks.h"
#include "re2dj/hle/import_dispatcher.h"
#include "re2dj/platform/linux/original_runner.h"
#include "re2dj/runtime/pe_loader.h"

namespace re2dj::platform::linux
{

class NativePeSession;

// Shared state for the Linux i386 in-process diagnostics that run the original
// image against the kernel32 and user32 facades. It owns facade registration,
// static IAT rebinding, the bounded guest-memory services the kernel32 handlers need, and
// the resolver-identity observation. Diagnostics keep only their own boundary.
class NativeKernel32Diagnostic final : public hle::ImportCallServices
{
public:
    NativeKernel32Diagnostic(std::uint32_t image_base, std::uint32_t image_size);
    ~NativeKernel32Diagnostic() override;

    NativeKernel32Diagnostic(const NativeKernel32Diagnostic&) = delete;
    NativeKernel32Diagnostic& operator=(const NativeKernel32Diagnostic&) = delete;

    // NativePeSessionSetup callback; the context must be a NativeKernel32Diagnostic.
    static bool Setup(NativePeSession* session, void* context, std::string* error);

    // Records the guest stack range for this event and dispatches it through the
    // facade. A gate outside the facade is recorded as the first unhandled
    // import and reported as unhandled so the bridge returns zero.
    bool Dispatch(const NativeImportGateEvent& event, NativeImportGateResult* output);

    // True when the event is a call through the named kernel32 facade export.
    bool IsExportCall(const NativeImportGateEvent& event, std::string_view export_name) const;

    // Reads a by-name GetProcAddress request from the call's second argument.
    // Returns false for an ordinal request or an unreadable name.
    bool ReadRequestedExportName(const NativeImportGateEvent& event, std::string* name);

    // Reads the ANSI string whose address is the call's argument at index.
    bool ReadArgumentString(const NativeImportGateEvent& event,
                            std::uint32_t index,
                            std::string* value);

    // The facade export this event calls, or null for a gate outside the facade.
    const hle::modules::RegisteredGuestExport* FindFacadeExport(
        const NativeImportGateEvent& event) const;

    // "module!name" for any gate the session or facade knows.
    std::string GateName(const NativeImportGateEvent& event) const;

    // Allocates a read/execute page of INT3 used to stop without writing guest code.
    bool PrepareStopStub(std::string* error);

    // Points the caller's return slot at the stop stub, so the thunk's
    // "pop ecx; ...; jmp ecx" lands on INT3 once cleanup is done. The original
    // return address is kept as stopped_return_address().
    bool RedirectReturnToStop(const NativeImportGateEvent& event);

    std::uint32_t stop_stub() const { return stop_stub_; }
    std::uint32_t stopped_return_address() const { return stopped_return_address_; }

    bool ImageContains(std::uint32_t address, std::size_t size) const;

    std::uint32_t image_base() const { return image_base_; }
    std::uint32_t image_size() const { return image_size_; }
    bool prepared() const { return registry_create_file_.value() != 0; }
    runtime::GuestAddress guest_get_version() const { return get_version_address_; }

    void CopyTo(OriginalRunResult* result) const;

    bool ReadGuestString(runtime::GuestAddress address,
                         std::string* value,
                         std::string* error) const override;
    runtime::GuestAddress FindGuestModule(std::string_view name) const override;
    runtime::GuestAddress FindGuestExport(runtime::GuestAddress module,
                                          std::string_view name) const override;
    runtime::GuestAddress FindGuestExport(runtime::GuestAddress module,
                                          std::uint16_t ordinal) const override;

private:
    const runtime::ImportGate* FindSessionGate(std::uint32_t address) const;
    void RecordUnresolvedLookup(std::string request) const;

    std::uint32_t image_base_ = 0;
    std::uint32_t image_size_ = 0;
    mutable std::uint32_t stack_base_ = 0;
    mutable std::uint32_t stack_limit_ = 0;
    NativeGuestModuleSet modules_;
    std::vector<NativeGuestImportRebinding> rebindings_;
    // Snapshot of the session's gates so an unhandled import can be named.
    std::vector<runtime::ImportGate> session_gates_;
    // Registry values captured while preparing the facade.
    runtime::GuestAddress registry_kernel32_base_;
    runtime::GuestAddress registry_get_version_;
    runtime::GuestAddress registry_create_file_;
    // Read back from the rebound static IAT slot while it is still mapped.
    runtime::GuestAddress static_create_file_slot_;
    // Values the guest actually received from the resolver at run time.
    mutable runtime::GuestAddress kernel32_base_;
    mutable runtime::GuestAddress get_version_address_;
    mutable runtime::GuestAddress create_file_address_;
    mutable std::string unhandled_dynamic_request_;
    std::string unhandled_import_;
    std::uint32_t stop_stub_ = 0;
    std::uint32_t stopped_return_address_ = 0;
    std::string dispatch_error_;
};

}  // namespace re2dj::platform::linux

#endif  // RE2DJ_PLATFORM_LINUX_NATIVE_KERNEL32_DIAGNOSTIC_H_
