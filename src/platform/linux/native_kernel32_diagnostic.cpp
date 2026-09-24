#include "native_kernel32_diagnostic.h"

#include <cstring>
#include <string>
#include <utility>

#include <sys/mman.h>
#include <unistd.h>

#include "native_low_memory.h"
#include "native_pe_session.h"
#include "re2dj/hle/modules/kernel32_module.h"
#include "re2dj/hle/modules/user32_module.h"

namespace re2dj::platform::linux
{
namespace
{

constexpr std::size_t kMaximumGuestStringLength = 260;
constexpr std::string_view kKernel32Module = "kernel32.dll";

bool StackContains(const NativeImportGateEvent& event,
                   std::uint32_t address,
                   std::size_t size)
{
    return address >= event.guest_stack_limit && address <= event.guest_stack_base &&
           size <= event.guest_stack_base - address;
}

std::string FormatGate(const runtime::ImportGate& gate)
{
    return gate.by_ordinal ? gate.module + "!#" + std::to_string(gate.ordinal)
                           : gate.module + "!" + gate.name;
}

}  // namespace

NativeKernel32Diagnostic::NativeKernel32Diagnostic(std::uint32_t image_base,
                                                   std::uint32_t image_size)
    : image_base_(image_base), image_size_(image_size)
{
}

NativeKernel32Diagnostic::~NativeKernel32Diagnostic()
{
    if (stop_stub_ != 0)
    {
        munmap(reinterpret_cast<void*>(static_cast<std::uintptr_t>(stop_stub_)),
               static_cast<std::size_t>(sysconf(_SC_PAGESIZE)));
    }
}

bool NativeKernel32Diagnostic::Setup(NativePeSession* session, void* context, std::string* error)
{
    auto* state = static_cast<NativeKernel32Diagnostic*>(context);
    if (session == nullptr || state == nullptr || error == nullptr ||
        !state->modules_.Add(hle::modules::MakeKernel32ModuleDescriptor(),
                             session->mutable_gates(),
                             NativeImportGateBridgeAddress(),
                             NativeImportGateCleanupAddress(),
                             kDefaultNativeGuestModuleBase,
                             error) ||
        !state->modules_.Add(hle::modules::MakeUser32ModuleDescriptor(),
                             session->mutable_gates(),
                             NativeImportGateBridgeAddress(),
                             NativeImportGateCleanupAddress(),
                             kDefaultNativeGuestModuleBase,
                             error) ||
        !RebindNativeGuestModuleImports(session->mutable_import_thunks(),
                                        state->modules_.registry(),
                                        &state->rebindings_,
                                        error))
    {
        return false;
    }

    const hle::modules::GuestModuleRegistry& registry = state->modules_.registry();
    const hle::modules::RegisteredGuestModule* module = registry.FindModule("kernel32");
    const hle::modules::RegisteredGuestExport* get_version =
        module == nullptr ? nullptr : registry.FindExport(module->base, "GetVersion");
    const hle::modules::RegisteredGuestExport* create_file =
        module == nullptr ? nullptr : registry.FindExport(module->base, "CreateFileA");
    if (module == nullptr || get_version == nullptr || create_file == nullptr ||
        state->modules_.FindGate(kKernel32Module, "GetProcAddress") == nullptr ||
        state->modules_.FindGate(kKernel32Module, "GetVersion") == nullptr ||
        state->modules_.FindGate(kKernel32Module, "CreateFileA") == nullptr)
    {
        *error = "kernel32 facade registration is incomplete";
        return false;
    }
    for (const NativeImportSlotBinding& slot : session->mutable_import_thunks()->slots)
    {
        // Read the slot back while the image is still mapped, so the static
        // path is evidence rather than the value we believe we wrote.
        if (slot.gate.name == "CreateFileA" && slot.slot != nullptr)
        {
            std::uint32_t written = 0;
            std::memcpy(&written, slot.slot, sizeof(written));
            state->static_create_file_slot_ = runtime::GuestAddress(written);
        }
    }
    state->session_gates_ = session->mutable_gates()->gates();
    state->registry_kernel32_base_ = module->base;
    state->registry_get_version_ = get_version->thunk_address;
    state->registry_create_file_ = create_file->thunk_address;
    error->clear();
    return true;
}

bool NativeKernel32Diagnostic::Dispatch(const NativeImportGateEvent& event,
                                        NativeImportGateResult* output)
{
    stack_base_ = event.guest_stack_base;
    stack_limit_ = event.guest_stack_limit;
    if (modules_.FindGate(runtime::GuestAddress(event.gate_address)) == nullptr)
    {
        if (unhandled_import_.empty())
        {
            const runtime::ImportGate* gate = FindSessionGate(event.gate_address);
            unhandled_import_ = gate == nullptr ? "<unknown gate>" : FormatGate(*gate);
        }
        return false;
    }
    return modules_.Dispatch(event, output, &dispatch_error_, this);
}

bool NativeKernel32Diagnostic::IsExportCall(const NativeImportGateEvent& event,
                                            std::string_view export_name) const
{
    const runtime::ImportGate* gate = modules_.FindGate(kKernel32Module, export_name);
    return gate != nullptr && gate->address.value() == event.gate_address;
}

bool NativeKernel32Diagnostic::ReadRequestedExportName(const NativeImportGateEvent& event,
                                                       std::string* name)
{
    if (name == nullptr || !StackContains(event, event.stack_pointer, 3 * sizeof(std::uint32_t)))
    {
        return false;
    }
    stack_base_ = event.guest_stack_base;
    stack_limit_ = event.guest_stack_limit;
    const auto* stack =
        reinterpret_cast<const std::uint32_t*>(static_cast<std::uintptr_t>(event.stack_pointer));
    if ((stack[2] & 0xFFFF0000U) == 0)
    {
        return false;
    }
    std::string error;
    return ReadGuestString(runtime::GuestAddress(stack[2]), name, &error);
}

bool NativeKernel32Diagnostic::ReadArgumentString(const NativeImportGateEvent& event,
                                                  std::uint32_t index,
                                                  std::string* value)
{
    const std::uint32_t offset = (index + 1) * sizeof(std::uint32_t);
    if (value == nullptr ||
        !StackContains(event, event.stack_pointer, offset + sizeof(std::uint32_t)))
    {
        return false;
    }
    stack_base_ = event.guest_stack_base;
    stack_limit_ = event.guest_stack_limit;
    std::uint32_t address = 0;
    std::memcpy(&address,
                reinterpret_cast<const void*>(
                    static_cast<std::uintptr_t>(event.stack_pointer + offset)),
                sizeof(address));
    std::string error;
    return ReadGuestString(runtime::GuestAddress(address), value, &error);
}

const hle::modules::RegisteredGuestExport* NativeKernel32Diagnostic::FindFacadeExport(
    const NativeImportGateEvent& event) const
{
    const runtime::ImportGate* gate = modules_.FindGate(runtime::GuestAddress(event.gate_address));
    return gate == nullptr ? nullptr : modules_.registry().FindExport(*gate);
}

std::string NativeKernel32Diagnostic::GateName(const NativeImportGateEvent& event) const
{
    const runtime::ImportGate* gate = modules_.FindGate(runtime::GuestAddress(event.gate_address));
    if (gate == nullptr)
    {
        gate = FindSessionGate(event.gate_address);
    }
    return gate == nullptr ? "<unknown gate>" : FormatGate(*gate);
}

bool NativeKernel32Diagnostic::PrepareStopStub(std::string* error)
{
    if (stop_stub_ != 0)
    {
        return true;
    }
    const auto page_size = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    // The guest returns into the stub, so it must be guest-addressable.
    NativeLowMemory mapping;
    std::string mapping_error;
    if (!MapNativeLowMemory(static_cast<std::uint32_t>(page_size),
                            PROT_READ | PROT_WRITE,
                            &mapping,
                            &mapping_error))
    {
        if (error != nullptr)
        {
            *error = "cannot allocate the diagnostic stop stub: " + mapping_error;
        }
        return false;
    }
    void* page = mapping.memory;
    std::memset(page, 0xCC, page_size);
    if (mprotect(page, page_size, PROT_READ | PROT_EXEC) != 0)
    {
        munmap(page, page_size);
        if (error != nullptr)
        {
            *error = "cannot make the diagnostic stop stub executable";
        }
        return false;
    }
    stop_stub_ = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(page));
    return true;
}

bool NativeKernel32Diagnostic::RedirectReturnToStop(const NativeImportGateEvent& event)
{
    if (stop_stub_ == 0 || !StackContains(event, event.stack_pointer, sizeof(std::uint32_t)))
    {
        return false;
    }
    auto* slot = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(event.stack_pointer));
    std::memcpy(&stopped_return_address_, slot, sizeof(stopped_return_address_));
    std::memcpy(slot, &stop_stub_, sizeof(stop_stub_));
    return true;
}

bool NativeKernel32Diagnostic::ImageContains(std::uint32_t address, std::size_t size) const
{
    if (address < image_base_)
    {
        return false;
    }
    const std::uint32_t offset = address - image_base_;
    return offset <= image_size_ && size <= image_size_ - offset;
}

void NativeKernel32Diagnostic::CopyTo(OriginalRunResult* result) const
{
    if (result == nullptr)
    {
        return;
    }
    OriginalResolverIdentity& identity = result->resolver_identity;
    identity.prepared = prepared();
    identity.registry_kernel32_base = registry_kernel32_base_;
    identity.registry_get_version = registry_get_version_;
    identity.registry_create_file = registry_create_file_;
    identity.kernel32_base = kernel32_base_;
    identity.get_version_address = get_version_address_;
    identity.create_file_address = create_file_address_;
    identity.static_imports_rebound = !rebindings_.empty();
    identity.static_create_file_slot = static_create_file_slot_;
    result->unhandled_dynamic_request_observed = !unhandled_dynamic_request_.empty();
    result->unhandled_dynamic_request = unhandled_dynamic_request_;
    result->unhandled_import_observed = !unhandled_import_.empty();
    result->unhandled_import = unhandled_import_;
}

bool NativeKernel32Diagnostic::ReadGuestString(runtime::GuestAddress address,
                                               std::string* value,
                                               std::string* error) const
{
    if (value == nullptr || error == nullptr || address.value() == 0)
    {
        if (error != nullptr)
        {
            *error = "invalid guest string request";
        }
        return false;
    }
    value->clear();
    for (std::size_t index = 0; index < kMaximumGuestStringLength; ++index)
    {
        const std::uint32_t current = address.value() + static_cast<std::uint32_t>(index);
        const bool in_image = ImageContains(current, 1);
        const bool in_stack = current >= stack_limit_ && current < stack_base_;
        if (!in_image && !in_stack)
        {
            *error = "guest string lies outside readable diagnostic ranges";
            return false;
        }
        const char value_byte =
            *reinterpret_cast<const char*>(static_cast<std::uintptr_t>(current));
        if (value_byte == '\0')
        {
            error->clear();
            return !value->empty();
        }
        value->push_back(value_byte);
    }
    *error = "guest string exceeds the diagnostic limit";
    return false;
}

runtime::GuestAddress NativeKernel32Diagnostic::FindGuestModule(std::string_view name) const
{
    const hle::modules::RegisteredGuestModule* module = modules_.registry().FindModule(name);
    if (module == nullptr)
    {
        RecordUnresolvedLookup("GetModuleHandleA(" + std::string(name) + ")");
        return {};
    }
    if (module->base == registry_kernel32_base_)
    {
        kernel32_base_ = module->base;
    }
    return module->base;
}

runtime::GuestAddress NativeKernel32Diagnostic::FindGuestExport(runtime::GuestAddress module,
                                                                std::string_view name) const
{
    const hle::modules::RegisteredGuestExport* export_entry =
        modules_.registry().FindExport(module, name);
    if (export_entry == nullptr)
    {
        RecordUnresolvedLookup("GetProcAddress(" + std::string(name) + ")");
        return {};
    }
    if (name == "GetVersion")
    {
        get_version_address_ = export_entry->thunk_address;
    }
    else if (name == "CreateFileA")
    {
        create_file_address_ = export_entry->thunk_address;
    }
    return export_entry->thunk_address;
}

runtime::GuestAddress NativeKernel32Diagnostic::FindGuestExport(runtime::GuestAddress module,
                                                                std::uint16_t ordinal) const
{
    const hle::modules::RegisteredGuestExport* export_entry =
        modules_.registry().FindExport(module, ordinal);
    if (export_entry == nullptr)
    {
        RecordUnresolvedLookup("GetProcAddress(#" + std::to_string(ordinal) + ")");
        return {};
    }
    return export_entry->thunk_address;
}

void NativeKernel32Diagnostic::RecordUnresolvedLookup(std::string request) const
{
    if (unhandled_dynamic_request_.empty())
    {
        unhandled_dynamic_request_ = std::move(request);
    }
}

const runtime::ImportGate* NativeKernel32Diagnostic::FindSessionGate(std::uint32_t address) const
{
    for (const runtime::ImportGate& gate : session_gates_)
    {
        if (gate.address.value() == address)
        {
            return &gate;
        }
    }
    return nullptr;
}

}  // namespace re2dj::platform::linux
