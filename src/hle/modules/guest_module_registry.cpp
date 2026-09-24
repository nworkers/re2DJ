#include "re2dj/hle/modules/guest_module_registry.h"

#include <cstdint>
#include <limits>
#include <string_view>
#include <utility>

namespace re2dj::hle::modules
{
namespace
{

void SetError(std::string* error, const char* message)
{
    if (error != nullptr)
    {
        *error = message;
    }
}

char AsciiLower(char value)
{
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
}

std::string NormalizeModuleName(std::string_view name)
{
    std::string normalized;
    normalized.reserve(name.size());
    for (const char value : name)
    {
        normalized.push_back(AsciiLower(value));
    }
    return normalized;
}

bool SameModuleName(std::string_view left, std::string_view right)
{
    return NormalizeModuleName(left) == NormalizeModuleName(right);
}

bool ValidateMapping(const GuestModuleDescriptor& descriptor,
                     const GuestModuleMapping& mapping,
                     std::string* error)
{
    if (mapping.base.value() == 0 || mapping.image_size == 0 ||
        mapping.export_thunks.size() != descriptor.exports.size())
    {
        SetError(error, "guest module mapping shape is invalid");
        return false;
    }

    constexpr std::uint64_t kGuestAddressLimit =
        static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) + 1U;
    const std::uint64_t begin = mapping.base.value();
    const std::uint64_t end = begin + mapping.image_size;
    if (end > kGuestAddressLimit)
    {
        SetError(error, "guest module mapping wraps the address space");
        return false;
    }
    for (const runtime::GuestAddress thunk : mapping.export_thunks)
    {
        if (thunk.value() < begin || thunk.value() >= end)
        {
            SetError(error, "guest export thunk lies outside the module image");
            return false;
        }
    }
    return true;
}

bool ContainsModuleName(const RegisteredGuestModule& module, std::string_view name)
{
    if (SameModuleName(module.name, name))
    {
        return true;
    }
    for (const std::string& alias : module.aliases)
    {
        if (SameModuleName(alias, name))
        {
            return true;
        }
    }
    return false;
}

bool RangesOverlap(runtime::GuestAddress left_base,
                   std::uint32_t left_size,
                   runtime::GuestAddress right_base,
                   std::uint32_t right_size)
{
    const std::uint64_t left_begin = left_base.value();
    const std::uint64_t left_end = left_begin + left_size;
    const std::uint64_t right_begin = right_base.value();
    const std::uint64_t right_end = right_begin + right_size;
    return left_begin < right_end && right_begin < left_end;
}

}  // namespace

bool GuestModuleRegistry::Register(GuestModuleDescriptor descriptor,
                                   GuestModuleMapping mapping,
                                   std::string* error)
{
    if (!ValidateGuestModuleDescriptor(descriptor, error) ||
        !ValidateMapping(descriptor, mapping, error))
    {
        return false;
    }

    for (const std::unique_ptr<RegisteredGuestModule>& registered : modules_)
    {
        if (ContainsModuleName(*registered, descriptor.name))
        {
            SetError(error, "guest module name is already registered");
            return false;
        }
        for (const std::string& alias : descriptor.aliases)
        {
            if (ContainsModuleName(*registered, alias))
            {
                SetError(error, "guest module alias is already registered");
                return false;
            }
        }
        if (RangesOverlap(registered->base,
                          registered->image_size,
                          mapping.base,
                          mapping.image_size))
        {
            SetError(error, "guest module image overlaps a registered module");
            return false;
        }
    }

    auto registered = std::make_unique<RegisteredGuestModule>();
    registered->name = std::move(descriptor.name);
    registered->aliases = std::move(descriptor.aliases);
    registered->base = mapping.base;
    registered->image_size = mapping.image_size;
    registered->exports.reserve(descriptor.exports.size());
    for (std::size_t index = 0; index < descriptor.exports.size(); ++index)
    {
        RegisteredGuestExport registered_export;
        registered_export.descriptor = std::move(descriptor.exports[index]);
        registered_export.thunk_address = mapping.export_thunks[index];
        registered->exports.push_back(std::move(registered_export));
    }
    modules_.push_back(std::move(registered));
    if (error != nullptr)
    {
        error->clear();
    }
    return true;
}

const RegisteredGuestModule* GuestModuleRegistry::FindModule(std::string_view name) const
{
    if (name.empty())
    {
        return nullptr;
    }
    for (const std::unique_ptr<RegisteredGuestModule>& module : modules_)
    {
        if (ContainsModuleName(*module, name))
        {
            return module.get();
        }
    }
    return nullptr;
}

const RegisteredGuestModule* GuestModuleRegistry::FindModule(runtime::GuestAddress handle) const
{
    if (handle.value() == 0)
    {
        return nullptr;
    }
    for (const std::unique_ptr<RegisteredGuestModule>& module : modules_)
    {
        if (module->base == handle)
        {
            return module.get();
        }
    }
    return nullptr;
}

const RegisteredGuestExport* GuestModuleRegistry::FindExport(
    runtime::GuestAddress module_handle,
    std::string_view name) const
{
    const RegisteredGuestModule* module = FindModule(module_handle);
    if (module == nullptr || name.empty())
    {
        return nullptr;
    }
    for (const RegisteredGuestExport& export_entry : module->exports)
    {
        if (export_entry.descriptor.name == name)
        {
            return &export_entry;
        }
    }
    return nullptr;
}

const RegisteredGuestExport* GuestModuleRegistry::FindExport(
    runtime::GuestAddress module_handle,
    std::uint16_t ordinal) const
{
    const RegisteredGuestModule* module = FindModule(module_handle);
    if (module == nullptr || ordinal == 0)
    {
        return nullptr;
    }
    for (const RegisteredGuestExport& export_entry : module->exports)
    {
        if (export_entry.descriptor.ordinal.has_value() &&
            export_entry.descriptor.ordinal.value() == ordinal)
        {
            return &export_entry;
        }
    }
    return nullptr;
}

const RegisteredGuestExport* GuestModuleRegistry::FindExport(
    const runtime::ImportGate& gate) const
{
    const RegisteredGuestModule* module = FindModule(gate.module);
    if (module == nullptr)
    {
        return nullptr;
    }
    return gate.by_ordinal ? FindExport(module->base, gate.ordinal)
                           : FindExport(module->base, gate.name);
}

std::size_t GuestModuleRegistry::module_count() const
{
    return modules_.size();
}

}  // namespace re2dj::hle::modules
