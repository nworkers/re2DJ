#include "re2dj/hle/modules/guest_module.h"

#include <array>
#include <cstdint>
#include <string_view>
#include <unordered_set>

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

bool ContainsEmbeddedNull(std::string_view value)
{
    return value.find('\0') != std::string_view::npos;
}

bool ValidCallingConvention(CallingConvention convention)
{
    return convention == CallingConvention::kStdcall ||
           convention == CallingConvention::kCdecl;
}

}  // namespace

bool ValidateGuestModuleDescriptor(const GuestModuleDescriptor& descriptor,
                                   std::string* error)
{
    if (descriptor.name.empty() || ContainsEmbeddedNull(descriptor.name))
    {
        SetError(error, "guest module name is empty or contains an embedded null");
        return false;
    }
    if (descriptor.exports.empty())
    {
        SetError(error, "guest module has no exports");
        return false;
    }

    std::unordered_set<std::string> module_names;
    module_names.insert(NormalizeModuleName(descriptor.name));
    for (const std::string& alias : descriptor.aliases)
    {
        if (alias.empty() || ContainsEmbeddedNull(alias) ||
            !module_names.insert(NormalizeModuleName(alias)).second)
        {
            SetError(error, "guest module contains an invalid or duplicate alias");
            return false;
        }
    }

    std::unordered_set<std::string> export_names;
    for (const GuestExportDescriptor& export_descriptor : descriptor.exports)
    {
        if (!export_descriptor.name.empty())
        {
            export_names.insert(export_descriptor.name);
        }
    }
    for (const std::string& absent : descriptor.absent_exports)
    {
        if (absent.empty() || ContainsEmbeddedNull(absent) || export_names.count(absent) != 0)
        {
            SetError(error, "guest module contains an invalid or exported absent name");
            return false;
        }
    }
    export_names.clear();
    std::unordered_set<std::uint16_t> export_ordinals;
    for (const GuestExportDescriptor& export_descriptor : descriptor.exports)
    {
        if (ContainsEmbeddedNull(export_descriptor.name) ||
            (export_descriptor.name.empty() && !export_descriptor.ordinal.has_value()) ||
            (export_descriptor.ordinal.has_value() && export_descriptor.ordinal.value() == 0))
        {
            SetError(error, "guest export has an invalid name or ordinal");
            return false;
        }
        if (export_descriptor.handler == nullptr ||
            !ValidCallingConvention(export_descriptor.calling_convention) ||
            export_descriptor.argument_count > ImportDispatcher::kMaximumArgumentCount)
        {
            SetError(error, "guest export has invalid handler or ABI metadata");
            return false;
        }
        if (!export_descriptor.name.empty() &&
            !export_names.insert(export_descriptor.name).second)
        {
            SetError(error, "guest module contains a duplicate export name");
            return false;
        }
        if (export_descriptor.ordinal.has_value() &&
            !export_ordinals.insert(export_descriptor.ordinal.value()).second)
        {
            SetError(error, "guest module contains a duplicate export ordinal");
            return false;
        }
    }
    if (error != nullptr)
    {
        error->clear();
    }
    return true;
}

bool UnimplementedExport(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (result != nullptr)
    {
        *result = {};
    }
    if (error != nullptr)
    {
        *error = call.gate.module + "!" + call.gate.name +
                 " is resolvable but not implemented";
    }
    return false;
}

bool IsAbsentGuestModule(std::string_view name)
{
    static constexpr std::array<std::string_view, 1> kAbsentModules = {"wfapi.dll"};
    std::string normalized = NormalizeModuleName(name);
    if (normalized.find('.') == std::string::npos)
    {
        normalized += ".dll";
    }
    for (const std::string_view absent : kAbsentModules)
    {
        if (normalized == absent)
        {
            return true;
        }
    }
    return false;
}

}  // namespace re2dj::hle::modules
