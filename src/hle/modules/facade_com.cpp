#include "facade_com.h"

#include <cstdio>
#include <utility>

namespace re2dj::hle::modules::com
{

bool Fail(std::string* error, std::string text)
{
    if (error != nullptr)
    {
        *error = std::move(text);
    }
    return false;
}

bool Succeed(ImportReturn* result, std::uint32_t value, std::string* error)
{
    result->eax = value;
    if (error != nullptr)
    {
        error->clear();
    }
    return true;
}

std::string CallName(const ImportCall& call)
{
    return call.gate.module + "!" + call.gate.name;
}

std::string FormatGuid(const re2dj::directx::Guid& guid)
{
    char text[40];
    std::snprintf(text, sizeof(text), "{%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
                  guid[3], guid[2], guid[1], guid[0], guid[5], guid[4], guid[7], guid[6], guid[8], guid[9],
                  guid[10], guid[11], guid[12], guid[13], guid[14], guid[15]);
    return text;
}

std::array<std::uint8_t, 4> Word(std::uint32_t value)
{
    return {static_cast<std::uint8_t>(value), static_cast<std::uint8_t>(value >> 8),
            static_cast<std::uint8_t>(value >> 16), static_cast<std::uint8_t>(value >> 24)};
}

void AppendText(std::vector<std::uint8_t>* bytes, std::string_view text)
{
    bytes->insert(bytes->end(), text.begin(), text.end());
    bytes->push_back(0);
}

bool WriteBytes(const ImportCall& call, std::uint32_t address, std::span<const std::uint8_t> bytes, std::string* error)
{
    std::string write_error;
    if (!call.services->WriteGuestBytes(runtime::GuestAddress(address), bytes, &write_error))
    {
        return Fail(error, CallName(call) + " cannot write its result: " + write_error);
    }
    return true;
}

bool WriteWord(const ImportCall& call, std::uint32_t address, std::uint32_t value, std::string* error)
{
    return WriteBytes(call, address, Word(value), error);
}

bool ReadGuid(const ImportCall& call, std::uint32_t address, re2dj::directx::Guid* guid, std::string* error)
{
    std::string read_error;
    if (!call.services->ReadGuestBytes(runtime::GuestAddress(address), *guid, &read_error))
    {
        return Fail(error, CallName(call) + " cannot read a GUID: " + read_error);
    }
    return true;
}

bool ReadBytes(const ImportCall& call, std::uint32_t address, std::span<std::uint8_t> bytes, std::string* error)
{
    std::string read_error;
    if (!call.services->ReadGuestBytes(runtime::GuestAddress(address), bytes, &read_error))
    {
        return Fail(error, CallName(call) + " cannot read guest memory: " + read_error);
    }
    return true;
}

std::uint32_t PlaceTemporary(const ImportCall& call, GuestProcess& process, std::span<const std::uint8_t> bytes)
{
    const std::uint32_t block = process.Allocate(static_cast<std::uint32_t>(bytes.size()));
    std::string write_error;
    if (block != 0 && !call.services->WriteGuestBytes(runtime::GuestAddress(block), bytes, &write_error))
    {
        process.Free(block);
        return 0;
    }
    return block;
}

bool CallWithData(const ImportCall& call,
                  std::uint32_t callback,
                  std::vector<std::uint8_t> data,
                  std::span<const std::uint32_t> other_arguments,
                  std::uint32_t* answer,
                  std::string* error)
{
    GuestCall guest_call;
    guest_call.function = callback;
    guest_call.arguments = {0};
    guest_call.arguments.insert(guest_call.arguments.end(), other_arguments.begin(), other_arguments.end());
    guest_call.data = std::move(data);
    guest_call.data_argument = 0;
    std::string call_error;
    if (!call.services->CallGuest(&guest_call, answer, &call_error))
    {
        return Fail(error, CallName(call) + " cannot call the callback: " + call_error);
    }
    return true;
}

GuestProcess* MethodProcess(const ImportCall& call,
                            ImportReturn* result,
                            std::size_t argument_count,
                            std::uint32_t kind,
                            std::string* error)
{
    GuestProcess* process = call.services == nullptr ? nullptr : call.services->Process();
    const GuestComObject* object =
        process == nullptr || call.arguments.empty() ? nullptr : process->com().Find(call.arguments[0]);
    if (result == nullptr || call.arguments.size() != argument_count || object == nullptr ||
        (kind != 0 && object->kind != kind))
    {
        Fail(error, CallName(call) + " has no model of this call or object");
        return nullptr;
    }
    *result = {};
    return process;
}

bool AddRef(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 1, 0, error);
    return process != nullptr && Succeed(result, process->com().AddRef(call.arguments[0]), error);
}

bool Release(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 1, 0, error);
    return process != nullptr && Succeed(result, process->com().Release(*process, call.arguments[0]), error);
}

std::vector<std::string_view> MethodNames(std::span<const Method> methods)
{
    std::vector<std::string_view> names;
    for (const Method& method : methods)
    {
        names.push_back(method.name);
    }
    return names;
}

std::uint32_t CreateObject(const ImportCall& call,
                           GuestProcess& process,
                           std::string_view module,
                           std::string_view interface_name,
                           std::span<const Method> methods,
                           GuestComObject object,
                           std::string* error)
{
    const std::vector<std::string_view> names = MethodNames(methods);
    std::string create_error;
    const std::uint32_t address =
        process.com().Create(*call.services, process, module, interface_name, names, std::move(object), &create_error);
    if (address == 0)
    {
        Fail(error, CallName(call) + " cannot create " + std::string(interface_name) + ": " + create_error);
    }
    return address;
}

GuestExportDescriptor MakeExport(std::string name, std::uint32_t argument_count, ImportHandler handler)
{
    GuestExportDescriptor descriptor;
    descriptor.name = std::move(name);
    descriptor.calling_convention = CallingConvention::kStdcall;
    descriptor.argument_count = argument_count;
    descriptor.handler = handler;
    return descriptor;
}

void AddMethods(GuestModuleDescriptor* descriptor, std::string_view interface_name, std::span<const Method> methods)
{
    for (const Method& method : methods)
    {
        descriptor->exports.push_back(MakeExport(std::string(interface_name) + "::" + std::string(method.name),
                                                 method.argument_count, method.handler));
    }
}

}  // namespace re2dj::hle::modules::com
