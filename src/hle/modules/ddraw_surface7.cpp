// IDirectDrawSurface7 of the ddraw facade, following the shared DirectX
// core's surface rules. Pixels live in guest memory, where the guest's locks
// and device contexts will reach them.

#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "ddraw_interfaces.h"
#include "re2dj/directx/abi.h"
#include "re2dj/directx/direct3d_description.h"
#include "re2dj/directx/directdraw_surface.h"
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

// A surface's state: its shape, its pixels in guest memory, and the surfaces
// attached to it, whose references it holds.
struct SurfaceState final : GuestComState
{
    dx::SurfaceShape shape;
    std::uint32_t pitch = 0;
    std::uint32_t pixels = 0;
    std::uint32_t back_buffer = 0;
    std::uint32_t depth_buffer = 0;

    std::vector<std::uint32_t> HeldReferences() const override { return {back_buffer, depth_buffer}; }
    void ReleaseResources(GuestProcess& process) override
    {
        if (pixels != 0)
        {
            process.VirtualFree(pixels, 0, kMemRelease);
            pixels = 0;
        }
    }
};

SurfaceState* StateOf(GuestProcess& process, std::uint32_t surface)
{
    const GuestComObject* object = process.com().Find(surface);
    return object == nullptr ? nullptr : object->StateAs<SurfaceState>();
}

bool ReadGuestWord(const ImportCall& call, std::uint32_t address, std::uint32_t* value, std::string* error)
{
    std::array<std::uint8_t, 4> bytes{};
    std::string read_error;
    if (!call.services->ReadGuestBytes(runtime::GuestAddress(address), bytes, &read_error))
    {
        return Fail(error, CallName(call) + " cannot read guest memory: " + read_error);
    }
    *value = static_cast<std::uint32_t>(bytes[0]) | (static_cast<std::uint32_t>(bytes[1]) << 8) |
             (static_cast<std::uint32_t>(bytes[2]) << 16) | (static_cast<std::uint32_t>(bytes[3]) << 24);
    return true;
}

bool QueryInterface(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 3, kSurfaceObject, error);
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
    // The 4th never asks a surface for another interface; one that is not
    // the surface itself is not modelled.
    if (iid != dx::kIidUnknown && iid != dx::kIidDirectDrawSurface7)
    {
        return Fail(error, CallName(call) + " has no model of " + com::FormatGuid(iid));
    }
    if (!com::WriteWord(call, call.arguments[2], call.arguments[0], error))
    {
        return false;
    }
    process->com().AddRef(call.arguments[0]);
    return Succeed(result, dx::kDdOk, error);
}

// AddAttachedSurface(this, lpDDSAttachedSurface): a depth surface of the same
// DirectDraw object, replacing any earlier one.
bool AddAttachedSurface(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 2, kSurfaceObject, error);
    if (process == nullptr)
    {
        return false;
    }
    const std::uint32_t attachment = call.arguments[1];
    if (attachment == 0)
    {
        return Succeed(result, dx::kDdErrInvalidParams, error);
    }
    const GuestComObject* self = process->com().Find(call.arguments[0]);
    const GuestComObject* attached = process->com().Find(attachment);
    SurfaceState* state = StateOf(*process, call.arguments[0]);
    SurfaceState* attached_state = StateOf(*process, attachment);
    if (attached == nullptr || attached_state == nullptr || attached->parent != self->parent)
    {
        return Succeed(result, dx::kDdErrInvalidObject, error);
    }
    const std::uint32_t attachable = dx::CheckAttachment(attached_state->shape);
    if (attachable != dx::kDdOk || state->depth_buffer == attachment)
    {
        return Succeed(result, attachable, error);
    }
    process->com().AddRef(attachment);
    const std::uint32_t previous = state->depth_buffer;
    state->depth_buffer = attachment;
    if (previous != 0)
    {
        process->com().Release(*process, previous);
    }
    return Succeed(result, dx::kDdOk, error);
}

// GetAttachedSurface(this, lpDDSCaps2, lplpDDAttachedSurface): the back
// buffer or depth surface asked for, AddRef'd, or DDERR_NOTFOUND.
bool GetAttachedSurface(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 3, kSurfaceObject, error);
    if (process == nullptr)
    {
        return false;
    }
    if (call.arguments[1] == 0 || call.arguments[2] == 0)
    {
        return Succeed(result, dx::kDdErrInvalidParams, error);
    }
    dx::DdsCaps2 caps;
    std::array<std::uint8_t, sizeof(dx::DdsCaps2)> bytes{};
    std::string read_error;
    if (!call.services->ReadGuestBytes(runtime::GuestAddress(call.arguments[1]), bytes, &read_error))
    {
        return Fail(error, CallName(call) + " cannot read DDSCAPS2: " + read_error);
    }
    std::memcpy(&caps, bytes.data(), sizeof(caps));
    if (!com::WriteWord(call, call.arguments[2], 0, error))
    {
        return false;
    }
    const SurfaceState* state = StateOf(*process, call.arguments[0]);
    std::uint32_t found = 0;
    switch (dx::QueryAttachment(caps))
    {
    case dx::AttachmentQuery::kBackBuffer:
        found = state->back_buffer;
        break;
    case dx::AttachmentQuery::kDepth:
        found = state->depth_buffer;
        break;
    case dx::AttachmentQuery::kNone:
        break;
    }
    if (found == 0)
    {
        return Succeed(result, dx::kDdErrNotFound, error);
    }
    if (!com::WriteWord(call, call.arguments[2], found, error))
    {
        return false;
    }
    process->com().AddRef(found);
    return Succeed(result, dx::kDdOk, error);
}

bool GetCaps(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 2, kSurfaceObject, error);
    if (process == nullptr)
    {
        return false;
    }
    if (call.arguments[1] == 0)
    {
        return Succeed(result, dx::kDdErrInvalidParams, error);
    }
    dx::DdsCaps2 caps;
    caps.caps = StateOf(*process, call.arguments[0])->shape.caps;
    return com::WriteStruct(call, call.arguments[1], caps, error) && Succeed(result, dx::kDdOk, error);
}

// GetPixelFormat(this, lpDDPixelFormat): RGB565 into a structure whose
// dwSize is right, DDERR_INVALIDPARAMS otherwise.
bool GetPixelFormat(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (MethodProcess(call, result, 2, kSurfaceObject, error) == nullptr)
    {
        return false;
    }
    std::uint32_t size = 0;
    if (call.arguments[1] == 0)
    {
        return Succeed(result, dx::kDdErrInvalidParams, error);
    }
    if (!ReadGuestWord(call, call.arguments[1], &size, error))
    {
        return false;
    }
    if (size != sizeof(dx::DdPixelFormat))
    {
        return Succeed(result, dx::kDdErrInvalidParams, error);
    }
    return com::WriteStruct(call, call.arguments[1], dx::Rgb565Format(), error) &&
           Succeed(result, dx::kDdOk, error);
}

// GetSurfaceDesc(this, lpDDSurfaceDesc2): the shared core's description into
// a structure whose dwSize is right, DDERR_INVALIDPARAMS otherwise.
bool GetSurfaceDesc(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 2, kSurfaceObject, error);
    if (process == nullptr)
    {
        return false;
    }
    std::uint32_t size = 0;
    if (call.arguments[1] == 0)
    {
        return Succeed(result, dx::kDdErrInvalidParams, error);
    }
    if (!ReadGuestWord(call, call.arguments[1], &size, error))
    {
        return false;
    }
    if (size != sizeof(dx::DdSurfaceDesc2))
    {
        return Succeed(result, dx::kDdErrInvalidParams, error);
    }
    const SurfaceState* state = StateOf(*process, call.arguments[0]);
    return com::WriteStruct(call, call.arguments[1], dx::SurfaceDescription(state->shape, state->pitch), error) &&
           Succeed(result, dx::kDdOk, error);
}

// The facade never loses a surface.
bool IsLost(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return MethodProcess(call, result, 1, kSurfaceObject, error) != nullptr && Succeed(result, dx::kDdOk, error);
}

// IDirectDrawSurface7 in vtable order (ddraw.h).
constexpr com::Method kMethods[] = {
    {"QueryInterface", 3, &QueryInterface},
    {"AddRef", 1, &com::AddRef},
    {"Release", 1, &com::Release},
    {"AddAttachedSurface", 2, &AddAttachedSurface},
    {"AddOverlayDirtyRect", 2, &UnimplementedExport},
    {"Blt", 6, &UnimplementedExport},
    {"BltBatch", 4, &UnimplementedExport},
    {"BltFast", 6, &UnimplementedExport},
    {"DeleteAttachedSurface", 3, &UnimplementedExport},
    {"EnumAttachedSurfaces", 3, &UnimplementedExport},
    {"EnumOverlayZOrders", 4, &UnimplementedExport},
    {"Flip", 3, &UnimplementedExport},
    {"GetAttachedSurface", 3, &GetAttachedSurface},
    {"GetBltStatus", 2, &UnimplementedExport},
    {"GetCaps", 2, &GetCaps},
    {"GetClipper", 2, &UnimplementedExport},
    {"GetColorKey", 3, &UnimplementedExport},
    {"GetDC", 2, &UnimplementedExport},
    {"GetFlipStatus", 2, &UnimplementedExport},
    {"GetOverlayPosition", 3, &UnimplementedExport},
    {"GetPalette", 2, &UnimplementedExport},
    {"GetPixelFormat", 2, &GetPixelFormat},
    {"GetSurfaceDesc", 2, &GetSurfaceDesc},
    {"Initialize", 3, &UnimplementedExport},
    {"IsLost", 1, &IsLost},
    {"Lock", 5, &UnimplementedExport},
    {"ReleaseDC", 2, &UnimplementedExport},
    {"Restore", 1, &UnimplementedExport},
    {"SetClipper", 2, &UnimplementedExport},
    {"SetColorKey", 3, &UnimplementedExport},
    {"SetOverlayPosition", 3, &UnimplementedExport},
    {"SetPalette", 2, &UnimplementedExport},
    {"Unlock", 2, &UnimplementedExport},
    {"UpdateOverlay", 6, &UnimplementedExport},
    {"UpdateOverlayDisplay", 2, &UnimplementedExport},
    {"UpdateOverlayZOrder", 3, &UnimplementedExport},
    {"GetDDInterface", 2, &UnimplementedExport},
    {"PageLock", 2, &UnimplementedExport},
    {"PageUnlock", 2, &UnimplementedExport},
    {"SetSurfaceDesc", 3, &UnimplementedExport},
    {"SetPrivateData", 5, &UnimplementedExport},
    {"GetPrivateData", 4, &UnimplementedExport},
    {"FreePrivateData", 2, &UnimplementedExport},
    {"GetUniquenessValue", 2, &UnimplementedExport},
    {"ChangeUniquenessValue", 1, &UnimplementedExport},
    {"SetPriority", 2, &UnimplementedExport},
    {"GetPriority", 2, &UnimplementedExport},
    {"SetLOD", 2, &UnimplementedExport},
    {"GetLOD", 2, &UnimplementedExport},
};

// One surface of a plan: its object, holding a reference to the DirectDraw
// object, and for a surface with pixels zeroed RGB565 memory in the guest's
// VirtualAlloc arena.
std::uint32_t MakeSurface(const ImportCall& call,
                          GuestProcess& process,
                          std::uint32_t direct_draw,
                          const dx::SurfaceShape& shape,
                          std::string* error)
{
    auto state = std::make_shared<SurfaceState>();
    state->shape = shape;
    if (shape.has_pixels())
    {
        state->pitch = dx::Rgb565Pitch(shape.width);
        const std::uint32_t size = state->pitch * shape.height;
        std::vector<std::pair<std::uint32_t, std::uint32_t>> committed;
        if (process.VirtualAlloc(0, size, kMemCommit | kMemReserve, kPageReadWrite, &state->pixels, &committed) !=
            GuestMemoryResult::kOk)
        {
            Fail(error, CallName(call) + " has no guest memory for a " + std::to_string(shape.width) + "x" +
                            std::to_string(shape.height) + " surface");
            return 0;
        }
        for (const auto& [address, length] : committed)
        {
            const std::vector<std::uint8_t> zeros(length, 0);
            if (!com::WriteBytes(call, address, zeros, error))
            {
                process.VirtualFree(state->pixels, 0, kMemRelease);
                return 0;
            }
        }
    }
    GuestComObject object;
    object.kind = kSurfaceObject;
    object.parent = direct_draw;
    object.state = state;
    const std::uint32_t surface = com::CreateObject(call, process, kModule, kDirectDrawSurface7, kMethods, object, error);
    if (surface == 0)
    {
        state->ReleaseResources(process);
        return 0;
    }
    process.com().AddRef(direct_draw);
    return surface;
}

}  // namespace

std::span<const com::Method> DirectDrawSurface7Methods()
{
    return kMethods;
}

const dx::SurfaceShape* SurfaceShapeOf(GuestProcess& process, std::uint32_t surface)
{
    const SurfaceState* state = StateOf(process, surface);
    return state == nullptr ? nullptr : &state->shape;
}

std::uint32_t CreateSurfaces(const ImportCall& call,
                             GuestProcess& process,
                             std::uint32_t direct_draw,
                             const dx::SurfacePlan& plan,
                             std::string* error)
{
    const std::uint32_t surface = MakeSurface(call, process, direct_draw, plan.surface, error);
    if (surface == 0 || !plan.has_back_buffer)
    {
        return surface;
    }
    const std::uint32_t back_buffer = MakeSurface(call, process, direct_draw, plan.back_buffer, error);
    if (back_buffer == 0)
    {
        process.com().Release(process, surface);
        return 0;
    }
    // The primary holds the back buffer's first reference.
    StateOf(process, surface)->back_buffer = back_buffer;
    return surface;
}

}  // namespace re2dj::hle::modules::ddraw
