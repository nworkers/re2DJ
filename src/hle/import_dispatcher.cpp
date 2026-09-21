#include "re2dj/hle/import_dispatcher.h"

#include <limits>
#include <string_view>
#include <utility>
#include <vector>

namespace re2dj::hle
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

bool SameModule(std::string_view left, std::string_view right)
{
    if (left.size() != right.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index)
    {
        if (AsciiLower(left[index]) != AsciiLower(right[index]))
        {
            return false;
        }
    }
    return true;
}

bool SameImport(const ImportBinding& left, const ImportBinding& right)
{
    if (!SameModule(left.module, right.module) || left.by_ordinal != right.by_ordinal)
    {
        return false;
    }
    return left.by_ordinal ? left.ordinal == right.ordinal : left.name == right.name;
}

bool ValidBinding(const ImportBinding& binding, std::string* error)
{
    if (binding.module.empty() || binding.handler == nullptr ||
        binding.argument_count > ImportDispatcher::kMaximumArgumentCount ||
        (binding.calling_convention != CallingConvention::kStdcall &&
         binding.calling_convention != CallingConvention::kCdecl))
    {
        SetError(error, "invalid import binding");
        return false;
    }
    if ((binding.by_ordinal && (!binding.name.empty() || binding.ordinal == 0)) ||
        (!binding.by_ordinal && (binding.name.empty() || binding.ordinal != 0)))
    {
        SetError(error, "invalid import binding name or ordinal");
        return false;
    }
    return true;
}

std::uint32_t ReadLe32(const std::uint8_t* bytes)
{
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8) |
           (static_cast<std::uint32_t>(bytes[2]) << 16) |
           (static_cast<std::uint32_t>(bytes[3]) << 24);
}

}  // namespace

bool ImportDispatcher::Register(ImportBinding binding, std::string* error)
{
    if (!ValidBinding(binding, error))
    {
        return false;
    }
    for (const ImportBinding& existing : bindings_)
    {
        if (SameImport(existing, binding))
        {
            SetError(error, "duplicate import binding");
            return false;
        }
    }
    bindings_.push_back(std::move(binding));
    return true;
}

bool ImportDispatcher::Dispatch(const runtime::ImportGate& gate,
                                const runtime::ExecutionEvent& event,
                                runtime::ExecutionBackend* backend,
                                std::string* error) const
{
    if (backend == nullptr || event.kind != runtime::ExecutionEventKind::kImportGate ||
        event.gate_address != gate.address)
    {
        SetError(error, "invalid import dispatch event");
        return false;
    }

    const ImportBinding* binding = Find(gate);
    if (binding == nullptr)
    {
        SetError(error, "unimplemented import");
        return false;
    }
    if (event.stack_pointer.value() > (std::numeric_limits<std::uint32_t>::max)() - 4)
    {
        SetError(error, "guest stack address overflows while reading import arguments");
        return false;
    }

    const std::size_t byte_count =
        static_cast<std::size_t>(binding->argument_count) * sizeof(std::uint32_t);
    std::vector<std::uint8_t> argument_bytes(byte_count);
    const runtime::GuestAddress argument_address = event.stack_pointer + 4;
    if (!backend->ReadMemory(argument_address, argument_bytes, error))
    {
        if (error != nullptr && error->empty())
        {
            *error = "cannot read guest import arguments";
        }
        return false;
    }

    std::vector<std::uint32_t> arguments(binding->argument_count);
    for (std::size_t index = 0; index < arguments.size(); ++index)
    {
        arguments[index] = ReadLe32(argument_bytes.data() + index * sizeof(std::uint32_t));
    }

    ImportReturn result;
    const ImportCall call{gate, arguments};
    if (!binding->handler(call, &result, error))
    {
        if (error != nullptr && error->empty())
        {
            *error = "import handler failed";
        }
        return false;
    }

    runtime::ImportCompletion completion;
    completion.event_id = event.event_id;
    completion.eax = result.eax;
    completion.edx = result.edx;
    completion.stack_bytes_to_pop =
        binding->calling_convention == CallingConvention::kStdcall
            ? static_cast<std::uint32_t>(byte_count)
            : 0;
    return backend->CompleteImport(completion, error);
}

const ImportBinding* ImportDispatcher::Find(const runtime::ImportGate& gate) const
{
    for (const ImportBinding& binding : bindings_)
    {
        if (!SameModule(binding.module, gate.module) || binding.by_ordinal != gate.by_ordinal)
        {
            continue;
        }
        if ((binding.by_ordinal && binding.ordinal == gate.ordinal) ||
            (!binding.by_ordinal && binding.name == gate.name))
        {
            return &binding;
        }
    }
    return nullptr;
}

}  // namespace re2dj::hle
