#ifndef RE2DJ_HLE_GUEST_COM_H_
#define RE2DJ_HLE_GUEST_COM_H_

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "re2dj/hle/import_dispatcher.h"

namespace re2dj::hle
{

class GuestProcess;

using GuestGuid = std::array<std::uint8_t, 16>;

// The state a facade module keeps for one of its objects, such as a
// DirectDraw object's display; each module derives its own.
struct GuestComState
{
    virtual ~GuestComState() = default;
    // Other facade objects this object holds a reference to, such as a
    // primary surface's back buffer, released after it goes.
    virtual std::vector<std::uint32_t> HeldReferences() const { return {}; }
    // Returns what the object owns in the guest process, such as a surface's
    // pixel memory, when it goes.
    virtual void ReleaseResources(GuestProcess& process) { static_cast<void>(process); }
};

// A COM object the facade implements. Its guest block holds only the vtable
// pointer; the facade keeps the rest here, by the block's address.
struct GuestComObject
{
    std::string interface_name;
    std::uint32_t reference_count = 1;
    // What the object is, for the module that made it (a DirectDraw object,
    // a surface, ...), and its index in that module's own tables.
    std::uint32_t kind = 0;
    std::uint32_t index = 0;
    // An object this one holds a reference to, such as the DirectDraw object
    // behind an IDirect3D7, released after this object goes; 0 for none.
    std::uint32_t parent = 0;
    // The module's own state for the object, if it keeps any.
    std::shared_ptr<GuestComState> state;

    // The state as the module's type, or null when it has none of that type.
    template <typename T>
    T* StateAs() const
    {
        return dynamic_cast<T*>(state.get());
    }
};

// The guest's facade COM objects. An interface's vtable is built once from the
// facade module's exports named "<interface>::<method>", in vtable order, and
// lives in the process heap like a DLL's static vtable.
class GuestComObjects
{
public:
    // Creates an object of the interface and returns its guest address, or 0
    // with error set when the vtable or the block cannot be made.
    std::uint32_t Create(const ImportCallServices& services,
                         GuestProcess& process,
                         std::string_view module,
                         std::string_view interface_name,
                         std::span<const std::string_view> methods,
                         GuestComObject object,
                         std::string* error);
    GuestComObject* Find(std::uint32_t address);

    // IUnknown::AddRef and Release: the new count. Release frees the block
    // and the state's resources at zero and forgets the object, then releases
    // the references the state held and last the parent.
    std::uint32_t AddRef(std::uint32_t address);
    std::uint32_t Release(GuestProcess& process, std::uint32_t address);

private:
    std::map<std::string, std::uint32_t, std::less<>> vtables_;
    std::map<std::uint32_t, GuestComObject> objects_;
};

}  // namespace re2dj::hle

#endif  // RE2DJ_HLE_GUEST_COM_H_
