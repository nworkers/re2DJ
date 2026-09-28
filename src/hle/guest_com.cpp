#include "re2dj/hle/guest_com.h"

#include <utility>
#include <vector>

#include "re2dj/hle/guest_process.h"

namespace re2dj::hle
{
namespace
{

void PutU32(std::vector<std::uint8_t>* bytes, std::uint32_t value)
{
    for (int shift = 0; shift < 32; shift += 8)
    {
        bytes->push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

}  // namespace

std::uint32_t GuestComObjects::Create(const ImportCallServices& services,
                                      GuestProcess& process,
                                      std::string_view module,
                                      std::string_view interface_name,
                                      std::span<const std::string_view> methods,
                                      GuestComObject object,
                                      std::string* error)
{
    auto vtable = vtables_.find(interface_name);
    if (vtable == vtables_.end())
    {
        const runtime::GuestAddress module_base = services.FindGuestModule(module);
        std::vector<std::uint8_t> bytes;
        for (const std::string_view method : methods)
        {
            const std::string name = std::string(interface_name) + "::" + std::string(method);
            const runtime::GuestAddress entry =
                module_base.value() == 0 ? runtime::GuestAddress() : services.FindGuestExport(module_base, name);
            if (entry.value() == 0)
            {
                *error = "no facade export for " + std::string(module) + "!" + name;
                return 0;
            }
            PutU32(&bytes, entry.value());
        }
        const std::uint32_t address = process.Allocate(static_cast<std::uint32_t>(bytes.size()));
        if (address == 0 || !services.WriteGuestBytes(runtime::GuestAddress(address), bytes, error))
        {
            if (address == 0)
            {
                *error = "the process heap has no room for a vtable";
            }
            return 0;
        }
        vtable = vtables_.emplace(std::string(interface_name), address).first;
    }
    const std::uint32_t block = process.Allocate(4);
    std::vector<std::uint8_t> bytes;
    PutU32(&bytes, vtable->second);
    if (block == 0 || !services.WriteGuestBytes(runtime::GuestAddress(block), bytes, error))
    {
        if (block == 0)
        {
            *error = "the process heap has no room for an object";
        }
        return 0;
    }
    object.interface_name = std::string(interface_name);
    objects_[block] = std::move(object);
    return block;
}

GuestComObject* GuestComObjects::Find(std::uint32_t address)
{
    const auto found = objects_.find(address);
    return found == objects_.end() ? nullptr : &found->second;
}

std::vector<std::uint32_t> GuestComObjects::Addresses() const
{
    std::vector<std::uint32_t> addresses;
    addresses.reserve(objects_.size());
    for (const auto& [address, object] : objects_)
    {
        static_cast<void>(object);
        addresses.push_back(address);
    }
    return addresses;
}

std::uint32_t GuestComObjects::AddRef(std::uint32_t address)
{
    GuestComObject* object = Find(address);
    return object == nullptr ? 0 : ++object->reference_count;
}

std::uint32_t GuestComObjects::Release(GuestProcess& process, std::uint32_t address)
{
    GuestComObject* object = Find(address);
    if (object == nullptr)
    {
        return 0;
    }
    const std::uint32_t count = --object->reference_count;
    if (count == 0)
    {
        const std::uint32_t parent = object->parent;
        const std::shared_ptr<GuestComState> state = object->state;
        const std::vector<std::uint32_t> held = state != nullptr ? state->HeldReferences() : std::vector<std::uint32_t>{};
        objects_.erase(address);
        process.Free(address);
        if (state != nullptr)
        {
            state->ReleaseResources(process);
        }
        for (const std::uint32_t reference : held)
        {
            if (reference != 0)
            {
                Release(process, reference);
            }
        }
        if (parent != 0)
        {
            Release(process, parent);
        }
    }
    return count;
}

}  // namespace re2dj::hle
