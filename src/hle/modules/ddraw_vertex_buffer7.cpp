// IDirect3DVertexBuffer7 of the ddraw facade, under the shared core's vertex
// buffer rules. The vertices live in guest memory, so a lock hands the guest
// that memory and a draw reads it back.

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ddraw_interfaces.h"
#include "re2dj/directx/abi.h"
#include "re2dj/directx/direct3d_vertex_buffer.h"
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

// A buffer: its description, vertex size, guest memory, and whether the
// guest holds it locked.
struct VertexBufferState final : GuestComState
{
    dx::D3dVertexBufferDesc description;
    std::uint32_t stride = 0;
    std::uint32_t address = 0;
    bool locked = false;

    std::uint32_t bytes() const { return description.vertex_count * stride; }
    void ReleaseResources(GuestProcess& process) override
    {
        if (address != 0)
        {
            process.VirtualFree(address, 0, kMemRelease);
            address = 0;
        }
    }
};

VertexBufferState* StateOf(GuestProcess& process, std::uint32_t buffer)
{
    const GuestComObject* object = process.com().Find(buffer);
    return object == nullptr || object->kind != kVertexBufferObject ? nullptr : object->StateAs<VertexBufferState>();
}

// QueryInterface(this, riid, ppvObj): IUnknown, IDirect3DVertexBuffer7, and
// IDirect3DVertexBuffer answer with the object itself, as the Windows facade
// answers; anything else is E_NOINTERFACE.
bool QueryInterface(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 3, kVertexBufferObject, error);
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
    const bool known =
        iid == dx::kIidUnknown || iid == dx::kIidDirect3DVertexBuffer7 || iid == dx::kIidDirect3DVertexBuffer;
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

// Lock(this, dwFlags, lplpData, lpdwSize): the whole buffer's guest memory,
// with the outputs cleared first as the Windows facade clears them.
bool Lock(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 4, kVertexBufferObject, error);
    if (process == nullptr)
    {
        return false;
    }
    VertexBufferState& buffer = *StateOf(*process, call.arguments[0]);
    const std::uint32_t data = call.arguments[2];
    const std::uint32_t size = call.arguments[3];
    if (data != 0 && (!com::WriteWord(call, data, 0, error) || (size != 0 && !com::WriteWord(call, size, 0, error))))
    {
        return false;
    }
    const std::uint32_t checked = dx::CheckLockVertexBuffer(data != 0, buffer.locked, buffer.bytes());
    if (checked != dx::kDdOk)
    {
        return Succeed(result, checked, error);
    }
    if (!com::WriteWord(call, data, buffer.address, error) ||
        (size != 0 && !com::WriteWord(call, size, buffer.bytes(), error)))
    {
        return false;
    }
    buffer.locked = true;
    return Succeed(result, dx::kDdOk, error);
}

// Unlock(this).
bool Unlock(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 1, kVertexBufferObject, error);
    if (process == nullptr)
    {
        return false;
    }
    VertexBufferState& buffer = *StateOf(*process, call.arguments[0]);
    const std::uint32_t checked = dx::CheckUnlockVertexBuffer(buffer.locked);
    buffer.locked = false;
    return Succeed(result, checked, error);
}

// ProcessVertices and Optimize are E_NOTIMPL, ProcessVerticesStrided
// DDERR_UNSUPPORTED (the same value), as the Windows facade answers.
bool NotImplemented8(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return MethodProcess(call, result, 8, kVertexBufferObject, error) != nullptr &&
           Succeed(result, dx::kDdErrUnsupported, error);
}

bool Optimize(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return MethodProcess(call, result, 3, kVertexBufferObject, error) != nullptr &&
           Succeed(result, dx::kDdErrUnsupported, error);
}

// GetVertexBufferDesc(this, lpVBDesc).
bool GetVertexBufferDesc(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 2, kVertexBufferObject, error);
    if (process == nullptr)
    {
        return false;
    }
    std::uint32_t size = 0;
    if (call.arguments[1] != 0 && !com::ReadStruct(call, call.arguments[1], &size, error))
    {
        return false;
    }
    const std::uint32_t checked = dx::CheckGetVertexBufferDesc(call.arguments[1] != 0, size);
    if (checked != dx::kDdOk)
    {
        return Succeed(result, checked, error);
    }
    return com::WriteStruct(call, call.arguments[1], StateOf(*process, call.arguments[0])->description, error) &&
           Succeed(result, dx::kDdOk, error);
}

// IDirect3DVertexBuffer7 in vtable order (d3d.h).
constexpr com::Method kMethods[] = {
    {"QueryInterface", 3, &QueryInterface},
    {"AddRef", 1, &com::AddRef},
    {"Release", 1, &com::Release},
    {"Lock", 4, &Lock},
    {"Unlock", 1, &Unlock},
    {"ProcessVertices", 8, &NotImplemented8},
    {"GetVertexBufferDesc", 2, &GetVertexBufferDesc},
    {"Optimize", 3, &Optimize},
    {"ProcessVerticesStrided", 8, &NotImplemented8},
};

}  // namespace

std::span<const com::Method> Direct3DVertexBuffer7Methods()
{
    return kMethods;
}

std::span<const com::Method> Direct3DVertexBufferMethods()
{
    // IDirect3DVertexBuffer is IDirect3DVertexBuffer7 without
    // ProcessVerticesStrided, in the same order.
    return std::span<const com::Method>(kMethods).first(std::size(kMethods) - 1);
}

std::uint32_t CreateVertexBuffer7(const ImportCall& call,
                                  GuestProcess& process,
                                  std::uint32_t direct3d,
                                  const dx::D3dVertexBufferDesc& description,
                                  std::uint32_t stride,
                                  bool directx6,
                                  std::string* error)
{
    auto state = std::make_shared<VertexBufferState>();
    state->description = description;
    state->description.size = sizeof(dx::D3dVertexBufferDesc);
    state->stride = stride;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> committed;
    if (process.VirtualAlloc(0, state->bytes(), kMemCommit | kMemReserve, kPageReadWrite, &state->address,
                             &committed) != GuestMemoryResult::kOk)
    {
        Fail(error, CallName(call) + " has no guest memory for a " + std::to_string(state->bytes()) +
                        "-byte vertex buffer");
        return 0;
    }
    for (const auto& [address, length] : committed)
    {
        const std::vector<std::uint8_t> zeros(length, 0);
        if (!com::WriteBytes(call, address, zeros, error))
        {
            state->ReleaseResources(process);
            return 0;
        }
    }
    GuestComObject object;
    object.kind = kVertexBufferObject;
    object.parent = direct3d;
    object.state = state;
    const std::uint32_t buffer =
        directx6 ? com::CreateObject(call, process, kModule, kDirect3DVertexBuffer, Direct3DVertexBufferMethods(),
                                     object, error)
                 : com::CreateObject(call, process, kModule, kDirect3DVertexBuffer7, kMethods, object, error);
    if (buffer == 0)
    {
        state->ReleaseResources(process);
        return 0;
    }
    // The buffer keeps its Direct3D object alive, as the Windows facade's
    // keeps its root.
    process.com().AddRef(direct3d);
    return buffer;
}

bool VertexBufferOf(GuestProcess& process, std::uint32_t buffer, VertexBufferView* view)
{
    const GuestComObject* object = process.com().Find(buffer);
    const VertexBufferState* state =
        object == nullptr || object->kind != kVertexBufferObject ? nullptr : object->StateAs<VertexBufferState>();
    if (state == nullptr)
    {
        return false;
    }
    view->direct3d = object->parent;
    view->address = state->address;
    view->stride = state->stride;
    view->vertex_count = state->description.vertex_count;
    view->fvf = state->description.fvf;
    view->locked = state->locked;
    return true;
}

}  // namespace re2dj::hle::modules::ddraw
