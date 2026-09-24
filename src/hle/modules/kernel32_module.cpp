#include "re2dj/hle/modules/kernel32_module.h"

#include <limits>
#include <string>

namespace re2dj::hle::modules
{
namespace
{

bool ReturnZero(const ImportCall&, ImportReturn* result, std::string* error)
{
    if (result == nullptr)
    {
        if (error != nullptr)
        {
            *error = "kernel32 result is null";
        }
        return false;
    }
    *result = {};
    if (error != nullptr)
    {
        error->clear();
    }
    return true;
}

bool GetModuleHandleA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!ReturnZero(call, result, error) || call.arguments.size() != 1)
    {
        if (error != nullptr && error->empty())
        {
            *error = "kernel32 GetModuleHandleA argument shape is invalid";
        }
        return false;
    }
    if (call.services == nullptr || call.arguments[0] == 0)
    {
        return true;
    }
    std::string name;
    if (!call.services->ReadGuestString(runtime::GuestAddress(call.arguments[0]),
                                        &name,
                                        error))
    {
        return false;
    }
    result->eax = call.services->FindGuestModule(name).value();
    return true;
}

bool GetProcAddress(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!ReturnZero(call, result, error) || call.arguments.size() != 2)
    {
        if (error != nullptr && error->empty())
        {
            *error = "kernel32 GetProcAddress argument shape is invalid";
        }
        return false;
    }
    if (call.services == nullptr || call.arguments[0] == 0 || call.arguments[1] == 0)
    {
        return true;
    }

    const runtime::GuestAddress module(call.arguments[0]);
    if ((call.arguments[1] & 0xFFFF0000U) == 0)
    {
        result->eax = call.services
                          ->FindGuestExport(module,
                                            static_cast<std::uint16_t>(call.arguments[1]))
                          .value();
        return true;
    }

    std::string name;
    if (!call.services->ReadGuestString(runtime::GuestAddress(call.arguments[1]),
                                        &name,
                                        error))
    {
        return false;
    }
    result->eax = call.services->FindGuestExport(module, name).value();
    return true;
}

bool GetVersion(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!ReturnZero(call, result, error))
    {
        return false;
    }
    result->eax = kKernel32GuestVersion;
    return true;
}

bool CreateFileA(const ImportCall&, ImportReturn* result, std::string* error)
{
    if (result == nullptr)
    {
        if (error != nullptr)
        {
            *error = "kernel32 result is null";
        }
        return false;
    }
    *result = {};
    result->eax = (std::numeric_limits<std::uint32_t>::max)();
    if (error != nullptr)
    {
        error->clear();
    }
    return true;
}

// ExitProcess(uExitCode) never returns: it asks the backend to end the guest
// process with the given code.
bool ExitProcess(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!ReturnZero(call, result, error) || call.arguments.size() != 1)
    {
        if (error != nullptr && error->empty())
        {
            *error = "kernel32 ExitProcess argument shape is invalid";
        }
        return false;
    }
    result->exit_process = true;
    result->exit_code = call.arguments[0];
    return true;
}

GuestExportDescriptor MakeExport(std::string name,
                                 std::uint32_t argument_count,
                                 ImportHandler handler)
{
    GuestExportDescriptor descriptor;
    descriptor.name = std::move(name);
    descriptor.calling_convention = CallingConvention::kStdcall;
    descriptor.argument_count = argument_count;
    descriptor.handler = handler;
    return descriptor;
}

}  // namespace

GuestModuleDescriptor MakeKernel32ModuleDescriptor()
{
    GuestModuleDescriptor descriptor;
    descriptor.name = "kernel32.dll";
    descriptor.aliases = {"kernel32"};
    descriptor.exports.push_back(MakeExport("GetModuleHandleA", 1, &GetModuleHandleA));
    descriptor.exports.push_back(MakeExport("GetProcAddress", 2, &GetProcAddress));
    descriptor.exports.push_back(MakeExport("GetVersion", 0, &GetVersion));
    descriptor.exports.push_back(MakeExport("CreateFileA", 7, &CreateFileA));
    descriptor.exports.push_back(MakeExport("ExitProcess", 1, &ExitProcess));
    return descriptor;
}

}  // namespace re2dj::hle::modules
