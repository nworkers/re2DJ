// IDirect3D7 of the ddraw facade, answering from the shared DirectX core.

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "ddraw_interfaces.h"
#include "re2dj/directx/abi.h"
#include "re2dj/directx/direct3d_description.h"
#include "re2dj/hle/guest_com.h"
#include "re2dj/hle/guest_process.h"

namespace re2dj::hle::modules::ddraw
{
namespace
{

namespace dx = re2dj::directx;
using com::CallName;
using com::Fail;
using com::MethodProcess;
using com::Succeed;

// IDirect3D7::QueryInterface(this, riid, ppvObj): IUnknown and IDirect3D7
// answer with the object itself; anything else is E_NOINTERFACE.
bool QueryInterface(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 3, kDirect3DObject, error);
    if (process == nullptr)
    {
        return false;
    }
    if (call.arguments[2] == 0)
    {
        return Succeed(result, dx::kEPointer, error);
    }
    dx::Guid iid{};
    if (!com::ReadGuid(call, call.arguments[1], &iid, error))
    {
        return false;
    }
    const bool known = iid == dx::kIidUnknown || iid == dx::kIidDirect3D7;
    if (!com::WriteWord(call, call.arguments[2], known ? call.arguments[0] : 0U, error))
    {
        return false;
    }
    if (!known)
    {
        return Succeed(result, dx::kENoInterface, error);
    }
    process->com().AddRef(call.arguments[0]);
    return Succeed(result, dx::kDdOk, error);
}

// IDirect3D7::EnumDevices(this, lpEnumDevicesCallback, lpUserArg): each
// shared-core device as callback(description, name, D3DDEVICEDESC7*, arg),
// until the callback answers D3DENUMRET_CANCEL.
bool EnumDevices(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 3, kDirect3DObject, error);
    if (process == nullptr)
    {
        return false;
    }
    const std::uint32_t callback = call.arguments[1];
    if (callback == 0)
    {
        return Succeed(result, dx::kDdErrInvalidParams, error);
    }
    for (const dx::Direct3DDevice& device : dx::Direct3D7Devices())
    {
        std::vector<std::uint8_t> bytes =
            com::Bytes(dx::DeviceDescription(device.guid, device.hardware_transform_and_light));
        const auto description_offset = static_cast<std::uint32_t>(bytes.size());
        com::AppendText(&bytes, device.description);
        const auto name_offset = static_cast<std::uint32_t>(bytes.size());
        com::AppendText(&bytes, device.name);
        const std::uint32_t block = com::PlaceTemporary(call, *process, bytes);
        if (block == 0)
        {
            return Fail(error, CallName(call) + " cannot place the device description");
        }
        GuestCall guest_call;
        guest_call.function = callback;
        guest_call.arguments = {block + description_offset, block + name_offset, block, call.arguments[2]};
        std::uint32_t answer = 0;
        std::string call_error;
        const bool called = call.services->CallGuest(&guest_call, &answer, &call_error);
        process->Free(block);
        if (!called)
        {
            return Fail(error, CallName(call) + " cannot call the callback: " + call_error);
        }
        if (answer == dx::kEnumCancel)
        {
            break;
        }
    }
    return Succeed(result, dx::kDdOk, error);
}

// IDirect3D7::EnumZBufferFormats(this, riidDevice, callback, lpContext): the
// one 16-bit depth format, whatever the device.
bool EnumZBufferFormats(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (MethodProcess(call, result, 4, kDirect3DObject, error) == nullptr)
    {
        return false;
    }
    if (call.arguments[2] == 0)
    {
        return Succeed(result, dx::kDdErrInvalidParams, error);
    }
    const std::array<std::uint32_t, 1> context = {call.arguments[3]};
    std::uint32_t answer = 0;
    return com::CallWithStruct(call, call.arguments[2], dx::Depth16Format(), context, &answer, error) &&
           Succeed(result, dx::kDdOk, error);
}

bool EvictManagedTextures(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return MethodProcess(call, result, 1, kDirect3DObject, error) != nullptr && Succeed(result, dx::kDdOk, error);
}

// IDirect3D7 in vtable order (d3d.h).
constexpr com::Method kMethods[] = {
    {"QueryInterface", 3, &QueryInterface},
    {"AddRef", 1, &com::AddRef},
    {"Release", 1, &com::Release},
    {"EnumDevices", 3, &EnumDevices},
    {"CreateDevice", 4, &UnimplementedExport},
    {"CreateVertexBuffer", 4, &UnimplementedExport},
    {"EnumZBufferFormats", 4, &EnumZBufferFormats},
    {"EvictManagedTextures", 1, &EvictManagedTextures},
};

}  // namespace

std::span<const com::Method> Direct3D7Methods()
{
    return kMethods;
}

std::uint32_t CreateDirect3D7(const ImportCall& call,
                              GuestProcess& process,
                              std::uint32_t direct_draw,
                              std::string* error)
{
    GuestComObject object;
    object.kind = kDirect3DObject;
    object.parent = direct_draw;
    const std::uint32_t direct3d = com::CreateObject(call, process, kModule, kDirect3D7, kMethods, object, error);
    if (direct3d != 0)
    {
        // The Direct3D interface keeps the DirectDraw object alive for as long
        // as the guest holds it, as the Windows facade's does.
        process.com().AddRef(direct_draw);
    }
    return direct3d;
}

}  // namespace re2dj::hle::modules::ddraw
