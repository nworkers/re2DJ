// IDirect3DDevice7 of the ddraw facade: the shared DirectX core's device
// state, set and read by the guest. Drawing and presentation come later.

#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "ddraw_interfaces.h"
#include "re2dj/directx/abi.h"
#include "re2dj/directx/direct3d_description.h"
#include "re2dj/directx/direct3d_device.h"
#include "re2dj/directx/direct3d_draw.h"
#include "re2dj/directx/directdraw_surface.h"
#include "re2dj/graphics/color_depth.h"
#include "re2dj/graphics/legacy_transform.h"
#include "re2dj/graphics/true_color.h"
#include "re2dj/graphics/legacy_vertex_buffer.h"
#include "re2dj/hle/host_presentation.h"
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

// A device's state, and the render target and stage 0 texture whose
// references it holds.
struct DeviceState final : GuestComState
{
    dx::DeviceState device = dx::InitialDeviceState();
    std::uint32_t render_target = 0;
    std::uint32_t texture = 0;
    // A DirectX 6 device's viewports: the one AddViewport attached and the
    // one SetCurrentViewport made current, each holding a reference.
    std::uint32_t attached_viewport = 0;
    std::uint32_t current_viewport = 0;

    std::vector<std::uint32_t> HeldReferences() const override
    {
        return {render_target, texture, attached_viewport, current_viewport};
    }
};

DeviceState& DeviceOf(GuestProcess& process, std::uint32_t device)
{
    return *process.com().Find(device)->StateAs<DeviceState>();
}

DirectDrawState& DirectDrawOf(GuestProcess& process, std::uint32_t device)
{
    return *process.com().Find(process.com().Find(device)->parent)->StateAs<DirectDrawState>();
}

// The device's core state, once MethodProcess has checked the object.
dx::DeviceState& StateOf(GuestProcess& process, std::uint32_t device)
{
    return process.com().Find(device)->StateAs<DeviceState>()->device;
}

// A method whose guest pointer argument must not be null, answering
// DDERR_INVALIDPARAMS when it is: the process, or null with the result set
// (valid is then false only when the call itself failed).
GuestProcess* PointerMethod(const ImportCall& call,
                            ImportReturn* result,
                            std::size_t argument_count,
                            std::size_t pointer_index,
                            bool* valid,
                            std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, argument_count, kDeviceObject, error);
    *valid = process != nullptr;
    if (process != nullptr && call.arguments[pointer_index] == 0)
    {
        Succeed(result, dx::kDdErrInvalidParams, error);
        return nullptr;
    }
    return process;
}

// QueryInterface(this, riid, ppvObj): IUnknown and IDirect3DDevice7 answer
// with the device itself. The 4th asks for nothing else, so anything else is
// not modelled.
bool QueryInterface(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 3, kDeviceObject, error);
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
    if (iid != dx::kIidUnknown && iid != dx::kIidDirect3DDevice7)
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

// GetCaps(this, lpD3DDevDesc): the device the enumeration reported.
bool GetCaps(const ImportCall& call, ImportReturn* result, std::string* error)
{
    bool valid = false;
    if (PointerMethod(call, result, 2, 1, &valid, error) == nullptr)
    {
        return valid;
    }
    return com::WriteStruct(call, call.arguments[1], dx::CreatedDeviceDescription(), error) &&
           Succeed(result, dx::kDdOk, error);
}

// EnumTextureFormats(this, lpd3dEnumPixelProc, lpArg): the one RGB565
// format; the callback's answer ends nothing more.
bool EnumTextureFormats(const ImportCall& call, ImportReturn* result, std::string* error)
{
    bool valid = false;
    if (PointerMethod(call, result, 3, 1, &valid, error) == nullptr)
    {
        return valid;
    }
    const std::array<std::uint32_t, 1> context = {call.arguments[2]};
    std::uint32_t answer = 0;
    return com::CallWithStruct(call, call.arguments[1], dx::Rgb565Format(), context, &answer, error) &&
           Succeed(result, dx::kDdOk, error);
}

bool BeginScene(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 1, kDeviceObject, error);
    return process != nullptr && Succeed(result, dx::BeginScene(StateOf(*process, call.arguments[0])), error);
}

bool EndScene(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 1, kDeviceObject, error);
    return process != nullptr && Succeed(result, dx::EndScene(StateOf(*process, call.arguments[0])), error);
}

// SetRenderTarget(this, lpNewRenderTarget, dwFlags): any surface of the same
// DirectDraw object, as the Windows facade takes it; the device lets go of
// the one before.
bool SetRenderTarget(const ImportCall& call, ImportReturn* result, std::string* error)
{
    bool valid = false;
    GuestProcess* process = PointerMethod(call, result, 3, 1, &valid, error);
    if (process == nullptr)
    {
        return valid;
    }
    const std::uint32_t surface = call.arguments[1];
    const GuestComObject* target = process->com().Find(surface);
    if (SurfaceShapeOf(*process, surface) == nullptr ||
        target->parent != process->com().Find(call.arguments[0])->parent)
    {
        return Succeed(result, dx::kDdErrInvalidObject, error);
    }
    DeviceState& state = *process->com().Find(call.arguments[0])->StateAs<DeviceState>();
    if (state.render_target != surface)
    {
        process->com().AddRef(surface);
        const std::uint32_t previous = state.render_target;
        state.render_target = surface;
        process->com().Release(*process, previous);
    }
    DirectDrawOf(*process, call.arguments[0]).presentation_surface = surface;
    return Succeed(result, dx::kDdOk, error);
}

// GetRenderTarget(this, lplpRenderTarget): the render target, AddRef'd.
bool GetRenderTarget(const ImportCall& call, ImportReturn* result, std::string* error)
{
    bool valid = false;
    GuestProcess* process = PointerMethod(call, result, 2, 1, &valid, error);
    if (process == nullptr)
    {
        return valid;
    }
    const std::uint32_t target = process->com().Find(call.arguments[0])->StateAs<DeviceState>()->render_target;
    if (!com::WriteWord(call, call.arguments[1], target, error))
    {
        return false;
    }
    process->com().AddRef(target);
    return Succeed(result, dx::kDdOk, error);
}

// Clear(this, dwCount, lpRects, dwFlags, dwColor, dvZ, dwStencil): a clear of
// the whole render target when it is the surface the guest presents from
// fills its pixels and clears the host's target to the color; anything else
// changes nothing drawn. D3D_OK either way, as the Windows facade answers.
bool Clear(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 7, kDeviceObject, error);
    if (process == nullptr)
    {
        return false;
    }
    const DeviceState& device = DeviceOf(*process, call.arguments[0]);
    const bool whole_target = call.arguments[1] == 0 && (call.arguments[3] & dx::kD3dClearTarget) != 0;
    if (whole_target && device.render_target != 0 &&
        device.render_target == DirectDrawOf(*process, call.arguments[0]).presentation_surface)
    {
        // In 32-bit colour the guest's 8-bit channels are kept; the RGB565
        // pixel is their narrowing either way.
        const std::uint16_t color = dx::Rgb565FromD3dColor(call.arguments[4]);
        const bool true_color = graphics::TrueColorSelected();
        const std::uint32_t xrgb = true_color ? call.arguments[4] & 0x00FFFFFFU : graphics::WidenRgb565(color);
        if (!FillSurface(call, *process, device.render_target, color, xrgb, error))
        {
            return false;
        }
        HostPresentation* presentation = call.services->Presentation();
        std::string clear_error;
        if (presentation != nullptr && !(true_color ? presentation->ClearTargetColor(xrgb, &clear_error)
                                                    : presentation->ClearTarget(color, &clear_error)))
        {
            return Fail(error, CallName(call) + " cannot clear the host target: " + clear_error);
        }
    }
    return Succeed(result, dx::kDdOk, error);
}

// Draws vertices already read from the guest under the draw's plan, as
// DrawPrimitive and the vertex buffer draws share: decoded (and transformed
// when untransformed), textured with stage 0, and drawn through the host at
// the display's size. Vertices that do not decode are DDERR_INVALIDPARAMS, a
// state or draw the backend refuses DDERR_GENERIC.
bool DrawVertices(const ImportCall& call,
                  ImportReturn* result,
                  GuestProcess& process,
                  const dx::DrawPlan& plan,
                  std::uint32_t fvf,
                  std::span<const std::byte> vertices,
                  std::uint32_t vertex_count,
                  std::string* error)
{
    const DeviceState& device = DeviceOf(process, call.arguments[0]);
    graphics::LegacyDrawCommand command;
    std::string decode_error;
    bool decoded = false;
    if (plan.transformed)
    {
        decoded = graphics::DecodeTransformedLitVertices(vertices, vertex_count, plan.topology, &command, &decode_error);
    }
    else
    {
        graphics::LegacyTransformState transform;
        const GuestComObject* viewport =
            device.current_viewport == 0 ? nullptr : process.com().Find(device.current_viewport);
        decoded = (viewport != nullptr
                       ? dx::BuildViewport2TransformState(device.device, viewport->StateAs<ViewportState>()->viewport,
                                                          &transform, &decode_error)
                       : dx::BuildTransformState(device.device, &transform, &decode_error)) &&
                  graphics::DecodeUntransformedVertices(vertices, vertex_count, fvf, plan.topology, transform,
                                                        &command, &decode_error);
    }
    if (!decoded)
    {
        return Succeed(result, dx::kDdErrInvalidParams, error);
    }
    graphics::LegacyTextureView texture_view;
    const graphics::LegacyTextureView* texture = nullptr;
    if (device.texture != 0)
    {
        if (!SurfaceTextureView(call, process, device.texture, &texture_view, error))
        {
            return false;
        }
        texture = &texture_view;
    }
    const dx::DisplayMode& mode = DirectDrawOf(process, call.arguments[0]).display.mode;
    graphics::LegacyFixedFunctionState state;
    std::string draw_error;
    bool drawn = dx::BuildFixedFunctionState(device.device, &state, &draw_error);
    if (drawn)
    {
        dx::ApplyFadeCompatibility(device.device, command, texture != nullptr, mode.width, mode.height, &state);
        HostPresentation* presentation = call.services->Presentation();
        drawn = presentation != nullptr &&
                presentation->Draw(command, state, mode.width, mode.height, texture, &draw_error);
    }
    return Succeed(result, drawn ? dx::kDdOk : dx::kDdErrGeneric, error);
}

std::span<const std::byte> AsBytes(const std::vector<std::uint8_t>& bytes)
{
    return {reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()};
}

// DrawPrimitive(this, dptPrimitiveType, dwVertexTypeDesc, lpvVertices,
// dwVertexCount, dwFlags): the draws the core plans, decoded from guest
// memory, drawn through the host's render backend at the display's size with
// the stage 0 texture. A draw outside the plan is DDERR_UNSUPPORTED,
// vertices that do not decode DDERR_INVALIDPARAMS, and a state or draw the
// backend refuses DDERR_GENERIC, as the Windows facade answers.
bool DrawPrimitive(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 6, kDeviceObject, error);
    if (process == nullptr)
    {
        return false;
    }
    const dx::DrawPlan plan = dx::PlanDrawPrimitive(call.arguments[1], call.arguments[2], call.arguments[4],
                                                    call.arguments[5]);
    if (plan.result != dx::kDdOk || call.arguments[3] == 0)
    {
        return Succeed(result, dx::kDdErrUnsupported, error);
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(call.arguments[4]) * plan.vertex_stride);
    if (!com::ReadBytes(call, call.arguments[3], bytes, error))
    {
        return false;
    }
    return DrawVertices(call, result, *process, plan, call.arguments[2], AsBytes(bytes), call.arguments[4], error);
}

// The vertex buffer a VB draw names, when it is one of this device's
// DirectDraw object; false otherwise.
bool DeviceVertexBuffer(GuestProcess& process, std::uint32_t device, std::uint32_t buffer, VertexBufferView* view)
{
    if (buffer == 0 || !VertexBufferOf(process, buffer, view))
    {
        return false;
    }
    const GuestComObject* direct3d = process.com().Find(view->direct3d);
    return direct3d != nullptr && direct3d->parent == process.com().Find(device)->parent;
}

// DrawPrimitiveVB(this, d3dptPrimitiveType, lpd3dVertexBuffer,
// dwStartVertex, dwNumVertices, dwFlags): the core's checks, then the
// buffer's vertices from the start vertex drawn as DrawPrimitive draws them.
bool DrawPrimitiveVB(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 6, kDeviceObject, error);
    if (process == nullptr)
    {
        return false;
    }
    VertexBufferView buffer;
    if (!DeviceVertexBuffer(*process, call.arguments[0], call.arguments[2], &buffer))
    {
        return Succeed(result, dx::kDdErrInvalidParams, error);
    }
    const std::uint32_t checked =
        dx::CheckDrawPrimitiveVB(call.arguments[4], buffer.locked, call.arguments[3], buffer.vertex_count);
    if (checked != dx::kDdOk)
    {
        return Succeed(result, checked, error);
    }
    const dx::DrawPlan plan = dx::PlanDrawPrimitive(call.arguments[1], buffer.fvf, call.arguments[4], call.arguments[5]);
    if (plan.result != dx::kDdOk)
    {
        return Succeed(result, dx::kDdErrUnsupported, error);
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(call.arguments[4]) * plan.vertex_stride);
    if (!com::ReadBytes(call, buffer.address + call.arguments[3] * buffer.stride, bytes, error))
    {
        return false;
    }
    return DrawVertices(call, result, *process, plan, buffer.fvf, AsBytes(bytes), call.arguments[4], error);
}

// IDirect3DDevice3::DrawIndexedPrimitiveVB(this, d3dptPrimitiveType,
// lpd3dVertexBuffer, lpwIndices, dwIndexCount, dwFlags), as the Windows DX6
// facade answers: no buffer, no indices, no count, or another device's
// buffer is DDERR_INVALIDPARAMS, a locked buffer D3DERR_VERTEXBUFFERLOCKED;
// the indexed vertices are expanded from the whole buffer and drawn as
// DrawPrimitive draws them.
bool Device3DrawIndexedPrimitiveVB(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 6, kDeviceObject, error);
    if (process == nullptr)
    {
        return false;
    }
    const std::uint32_t index_count = call.arguments[4];
    VertexBufferView buffer;
    if (call.arguments[3] == 0 || index_count == 0 ||
        !DeviceVertexBuffer(*process, call.arguments[0], call.arguments[2], &buffer))
    {
        return Succeed(result, dx::kDdErrInvalidParams, error);
    }
    if (buffer.locked)
    {
        return Succeed(result, dx::kD3dErrVertexBufferLocked, error);
    }
    std::vector<std::uint8_t> vertices(static_cast<std::size_t>(buffer.vertex_count) * buffer.stride);
    std::vector<std::uint8_t> index_bytes(static_cast<std::size_t>(index_count) * sizeof(std::uint16_t));
    if (!com::ReadBytes(call, buffer.address, vertices, error) ||
        !com::ReadBytes(call, call.arguments[3], index_bytes, error))
    {
        return false;
    }
    std::vector<std::uint16_t> indices(index_count);
    std::memcpy(indices.data(), index_bytes.data(), index_bytes.size());
    std::vector<std::byte> expanded;
    if (!graphics::ExpandIndexedVertices(AsBytes(vertices), buffer.stride, buffer.vertex_count, indices, &expanded))
    {
        return Succeed(result, dx::kDdErrInvalidParams, error);
    }
    const dx::DrawPlan plan = dx::PlanDrawPrimitive(call.arguments[1], buffer.fvf, index_count, call.arguments[5]);
    if (plan.result != dx::kDdOk)
    {
        return Succeed(result, dx::kDdErrUnsupported, error);
    }
    return DrawVertices(call, result, *process, plan, buffer.fvf, expanded, index_count, error);
}

// DrawIndexedPrimitiveVB(this, d3dptPrimitiveType, lpd3dVertexBuffer,
// dwStartVertex, dwNumVertices, lpwIndices, dwIndexCount, dwFlags): the
// core's checks, then the indexed vertices expanded from the whole buffer
// and drawn as DrawPrimitive draws them.
bool DrawIndexedPrimitiveVB(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 8, kDeviceObject, error);
    if (process == nullptr)
    {
        return false;
    }
    const std::uint32_t index_count = call.arguments[6];
    if (call.arguments[3] != 0)
    {
        return Succeed(result, dx::kDdErrUnsupported, error);
    }
    VertexBufferView buffer;
    if (!DeviceVertexBuffer(*process, call.arguments[0], call.arguments[2], &buffer))
    {
        return Succeed(result, dx::kDdErrInvalidParams, error);
    }
    const std::uint32_t checked =
        dx::CheckDrawIndexedPrimitiveVB(call.arguments[3], call.arguments[5] != 0, index_count, buffer.locked);
    if (checked != dx::kDdOk)
    {
        return Succeed(result, checked, error);
    }
    std::vector<std::uint8_t> vertices(static_cast<std::size_t>(buffer.vertex_count) * buffer.stride);
    std::vector<std::uint8_t> index_bytes(static_cast<std::size_t>(index_count) * sizeof(std::uint16_t));
    if (!com::ReadBytes(call, buffer.address, vertices, error) ||
        !com::ReadBytes(call, call.arguments[5], index_bytes, error))
    {
        return false;
    }
    std::vector<std::uint16_t> indices(index_count);
    std::memcpy(indices.data(), index_bytes.data(), index_bytes.size());
    std::vector<std::byte> expanded;
    if (!graphics::ExpandIndexedVertices(AsBytes(vertices), buffer.stride, buffer.vertex_count, indices, &expanded))
    {
        return Succeed(result, dx::kDdErrInvalidParams, error);
    }
    const dx::DrawPlan plan = dx::PlanDrawPrimitive(call.arguments[1], buffer.fvf, index_count, call.arguments[7]);
    if (plan.result != dx::kDdOk)
    {
        return Succeed(result, dx::kDdErrUnsupported, error);
    }
    return DrawVertices(call, result, *process, plan, buffer.fvf, expanded, index_count, error);
}

// SetTexture(this, dwStage, lpTexture): stage 0 only (another stage is
// DDERR_UNSUPPORTED); a surface that is not a texture is DDERR_INVALIDOBJECT;
// null clears the stage. The device holds the texture.
bool SetTextureSurface(const ImportCall& call,
                       ImportReturn* result,
                       GuestProcess* process,
                       std::uint32_t surface,
                       std::string* error);

bool SetTexture(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 3, kDeviceObject, error);
    if (process == nullptr)
    {
        return false;
    }
    if (call.arguments[1] != 0)
    {
        return Succeed(result, dx::kDdErrUnsupported, error);
    }
    return SetTextureSurface(call, result, process, call.arguments[2], error);
}

// IDirect3DDevice3::SetTexture(this, dwStage, lpTexture2), as the Windows DX6
// facade answers it: stage 0 only, an IDirect3DTexture2 or null; anything else
// is DDERR_INVALIDOBJECT. The texture's surface takes DirectX 7's place.
bool Device3SetTexture(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 3, kDeviceObject, error);
    if (process == nullptr)
    {
        return false;
    }
    if (call.arguments[1] != 0)
    {
        return Succeed(result, dx::kDdErrUnsupported, error);
    }
    const std::uint32_t texture = call.arguments[2];
    const std::uint32_t surface = texture == 0 ? 0 : SurfaceOfTexture2(*process, texture);
    if (texture != 0 && surface == 0)
    {
        return Succeed(result, dx::kDdErrInvalidObject, error);
    }
    return SetTextureSurface(call, result, process, surface, error);
}

// The device's stage-0 texture set to surface, which must be a texture.
bool SetTextureSurface(const ImportCall& call,
                       ImportReturn* result,
                       GuestProcess* process,
                       std::uint32_t surface,
                       std::string* error)
{
    if (surface != 0)
    {
        const dx::SurfaceShape* shape = SurfaceShapeOf(*process, surface);
        if (shape == nullptr || (shape->caps & dx::kDdsCapsTexture) == 0)
        {
            return Succeed(result, dx::kDdErrInvalidObject, error);
        }
    }
    DeviceState& device = DeviceOf(*process, call.arguments[0]);
    if (device.texture != surface)
    {
        if (surface != 0)
        {
            process->com().AddRef(surface);
        }
        const std::uint32_t previous = device.texture;
        device.texture = surface;
        if (previous != 0)
        {
            process->com().Release(*process, previous);
        }
    }
    return Succeed(result, dx::kDdOk, error);
}

// SetTransform(this, dtstTransformStateType, lpD3DMatrix).
bool SetTransform(const ImportCall& call, ImportReturn* result, std::string* error)
{
    bool valid = false;
    GuestProcess* process = PointerMethod(call, result, 3, 2, &valid, error);
    if (process == nullptr)
    {
        return valid;
    }
    dx::D3dMatrix matrix;
    return com::ReadStruct(call, call.arguments[2], &matrix, error) &&
           Succeed(result, dx::SetTransform(StateOf(*process, call.arguments[0]), call.arguments[1], matrix), error);
}

// GetTransform(this, dtstTransformStateType, lpD3DMatrix).
bool GetTransform(const ImportCall& call, ImportReturn* result, std::string* error)
{
    bool valid = false;
    GuestProcess* process = PointerMethod(call, result, 3, 2, &valid, error);
    if (process == nullptr)
    {
        return valid;
    }
    dx::D3dMatrix matrix;
    const std::uint32_t answer = dx::GetTransform(StateOf(*process, call.arguments[0]), call.arguments[1], &matrix);
    if (answer == dx::kDdOk && !com::WriteStruct(call, call.arguments[2], matrix, error))
    {
        return false;
    }
    return Succeed(result, answer, error);
}

// SetViewport(this, lpViewport).
bool SetViewport(const ImportCall& call, ImportReturn* result, std::string* error)
{
    bool valid = false;
    GuestProcess* process = PointerMethod(call, result, 2, 1, &valid, error);
    if (process == nullptr)
    {
        return valid;
    }
    dx::D3dViewport7 viewport;
    return com::ReadStruct(call, call.arguments[1], &viewport, error) &&
           Succeed(result, dx::SetViewport(StateOf(*process, call.arguments[0]), viewport), error);
}

// GetViewport(this, lpViewport).
bool GetViewport(const ImportCall& call, ImportReturn* result, std::string* error)
{
    bool valid = false;
    GuestProcess* process = PointerMethod(call, result, 2, 1, &valid, error);
    if (process == nullptr)
    {
        return valid;
    }
    dx::D3dViewport7 viewport;
    const std::uint32_t answer = dx::GetViewport(StateOf(*process, call.arguments[0]), &viewport);
    if (answer == dx::kDdOk && !com::WriteStruct(call, call.arguments[1], viewport, error))
    {
        return false;
    }
    return Succeed(result, answer, error);
}

// SetMaterial(this, lpMaterial): kept on the device, where it colours lit
// D3DVERTEX draws (dx::UntransformedVertexColor).
bool SetMaterial(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 2, kDeviceObject, error);
    if (process == nullptr)
    {
        return false;
    }
    if (call.arguments[1] != 0 &&
        !com::ReadStruct(call, call.arguments[1], &StateOf(*process, call.arguments[0]).material, error))
    {
        return false;
    }
    return Succeed(result, dx::kDdOk, error);
}

// GetMaterial(this, lpMaterial): the device's material, all zero on a new
// device as on Windows 11.
bool GetMaterial(const ImportCall& call, ImportReturn* result, std::string* error)
{
    bool valid = false;
    GuestProcess* process = PointerMethod(call, result, 2, 1, &valid, error);
    if (process == nullptr)
    {
        return valid;
    }
    if (!com::WriteStruct(call, call.arguments[1], StateOf(*process, call.arguments[0]).material, error))
    {
        return false;
    }
    return Succeed(result, dx::kDdOk, error);
}

// SetRenderState(this, dwRenderStateType, dwRenderState).
bool SetRenderState(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 3, kDeviceObject, error);
    return process != nullptr &&
           Succeed(result,
                   dx::SetRenderState(StateOf(*process, call.arguments[0]), call.arguments[1], call.arguments[2]),
                   error);
}

// GetRenderState(this, dwRenderStateType, lpdwRenderState).
bool GetRenderState(const ImportCall& call, ImportReturn* result, std::string* error)
{
    bool valid = false;
    GuestProcess* process = PointerMethod(call, result, 3, 2, &valid, error);
    if (process == nullptr)
    {
        return valid;
    }
    std::uint32_t value = 0;
    const std::uint32_t answer = dx::GetRenderState(StateOf(*process, call.arguments[0]), call.arguments[1], &value);
    if (answer == dx::kDdOk && !com::WriteWord(call, call.arguments[2], value, error))
    {
        return false;
    }
    return Succeed(result, answer, error);
}

// IDirect3DDevice3::SetLightState(this, dwLightStateType, dwLightState) and
// GetLightState(this, dwLightStateType, lpdwLightState), under the shared
// core's rules the Windows DX6 facade uses.
bool Device3SetLightState(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 3, kDeviceObject, error);
    return process != nullptr &&
           Succeed(result,
                   dx::SetLightState(StateOf(*process, call.arguments[0]), call.arguments[1], call.arguments[2]),
                   error);
}

bool Device3GetLightState(const ImportCall& call, ImportReturn* result, std::string* error)
{
    bool valid = false;
    GuestProcess* process = PointerMethod(call, result, 3, 2, &valid, error);
    if (process == nullptr)
    {
        return valid;
    }
    std::uint32_t value = 0;
    const std::uint32_t answer = dx::GetLightState(StateOf(*process, call.arguments[0]), call.arguments[1], &value);
    if (answer == dx::kDdOk && !com::WriteWord(call, call.arguments[2], value, error))
    {
        return false;
    }
    return Succeed(result, answer, error);
}

// GetTextureStageState(this, dwStage, d3dTexStageStateType, lpdwState).
bool GetTextureStageState(const ImportCall& call, ImportReturn* result, std::string* error)
{
    bool valid = false;
    GuestProcess* process = PointerMethod(call, result, 4, 3, &valid, error);
    if (process == nullptr)
    {
        return valid;
    }
    std::uint32_t value = 0;
    const std::uint32_t answer = dx::GetTextureStageState(StateOf(*process, call.arguments[0]), call.arguments[1],
                                                          call.arguments[2], &value);
    if (answer == dx::kDdOk && !com::WriteWord(call, call.arguments[3], value, error))
    {
        return false;
    }
    return Succeed(result, answer, error);
}

// SetTextureStageState(this, dwStage, d3dTexStageStateType, dwState).
bool SetTextureStageState(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 4, kDeviceObject, error);
    return process != nullptr &&
           Succeed(result,
                   dx::SetTextureStageState(StateOf(*process, call.arguments[0]), call.arguments[1],
                                            call.arguments[2], call.arguments[3]),
                   error);
}

// IDirect3DDevice7 in vtable order (d3d.h).
constexpr com::Method kMethods[] = {
    {"QueryInterface", 3, &QueryInterface},
    {"AddRef", 1, &com::AddRef},
    {"Release", 1, &com::Release},
    {"GetCaps", 2, &GetCaps},
    {"EnumTextureFormats", 3, &EnumTextureFormats},
    {"BeginScene", 1, &BeginScene},
    {"EndScene", 1, &EndScene},
    {"GetDirect3D", 2, &UnimplementedExport},
    {"SetRenderTarget", 3, &SetRenderTarget},
    {"GetRenderTarget", 2, &GetRenderTarget},
    {"Clear", 7, &Clear},
    {"SetTransform", 3, &SetTransform},
    {"GetTransform", 3, &GetTransform},
    {"SetViewport", 2, &SetViewport},
    {"MultiplyTransform", 3, &UnimplementedExport},
    {"GetViewport", 2, &GetViewport},
    {"SetMaterial", 2, &SetMaterial},
    {"GetMaterial", 2, &GetMaterial},
    {"SetLight", 3, &UnimplementedExport},
    {"GetLight", 3, &UnimplementedExport},
    {"SetRenderState", 3, &SetRenderState},
    {"GetRenderState", 3, &GetRenderState},
    {"BeginStateBlock", 1, &UnimplementedExport},
    {"EndStateBlock", 2, &UnimplementedExport},
    {"PreLoad", 2, &UnimplementedExport},
    {"DrawPrimitive", 6, &DrawPrimitive},
    {"DrawIndexedPrimitive", 8, &UnimplementedExport},
    {"SetClipStatus", 2, &UnimplementedExport},
    {"GetClipStatus", 2, &UnimplementedExport},
    {"DrawPrimitiveStrided", 6, &UnimplementedExport},
    {"DrawIndexedPrimitiveStrided", 8, &UnimplementedExport},
    {"DrawPrimitiveVB", 6, &DrawPrimitiveVB},
    {"DrawIndexedPrimitiveVB", 8, &DrawIndexedPrimitiveVB},
    {"ComputeSphereVisibility", 6, &UnimplementedExport},
    {"GetTexture", 3, &UnimplementedExport},
    {"SetTexture", 3, &SetTexture},
    {"GetTextureStageState", 4, &GetTextureStageState},
    {"SetTextureStageState", 4, &SetTextureStageState},
    {"ValidateDevice", 2, &UnimplementedExport},
    {"ApplyStateBlock", 2, &UnimplementedExport},
    {"CaptureStateBlock", 2, &UnimplementedExport},
    {"DeleteStateBlock", 2, &UnimplementedExport},
    {"CreateStateBlock", 3, &UnimplementedExport},
    {"Load", 6, &UnimplementedExport},
    {"LightEnable", 3, &UnimplementedExport},
    {"GetLightEnable", 3, &UnimplementedExport},
    {"SetClipPlane", 3, &UnimplementedExport},
    {"GetClipPlane", 3, &UnimplementedExport},
    {"GetInfo", 4, &UnimplementedExport},
};

// A viewport argument that is a facade viewport, or 0.
std::uint32_t ViewportArgument(GuestProcess& process, std::uint32_t address)
{
    const GuestComObject* object = address == 0 ? nullptr : process.com().Find(address);
    return object != nullptr && object->kind == kViewportObject ? address : 0;
}

// IDirect3DDevice3::AddViewport(this, lpDirect3DViewport), as the Windows
// DX6 facade keeps it: one attached viewport, held by the device; a second,
// or no viewport, is DDERR_INVALIDPARAMS.
bool Device3AddViewport(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 2, kDeviceObject, error);
    if (process == nullptr)
    {
        return false;
    }
    DeviceState& device = DeviceOf(*process, call.arguments[0]);
    const std::uint32_t viewport = ViewportArgument(*process, call.arguments[1]);
    if (viewport == 0 || device.attached_viewport != 0)
    {
        return Succeed(result, dx::kDdErrInvalidParams, error);
    }
    process->com().AddRef(viewport);
    device.attached_viewport = viewport;
    return Succeed(result, dx::kDdOk, error);
}

// DeleteViewport(this, lpDirect3DViewport): the attached viewport detached
// (and no longer current), DDERR_NOTFOUND for another, DDERR_INVALIDPARAMS
// for none.
bool Device3DeleteViewport(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 2, kDeviceObject, error);
    if (process == nullptr)
    {
        return false;
    }
    DeviceState& device = DeviceOf(*process, call.arguments[0]);
    const std::uint32_t viewport = call.arguments[1];
    if (viewport == 0)
    {
        return Succeed(result, dx::kDdErrInvalidParams, error);
    }
    if (device.attached_viewport != viewport)
    {
        return Succeed(result, dx::kDdErrNotFound, error);
    }
    if (device.current_viewport == viewport)
    {
        device.current_viewport = 0;
        process->com().Release(*process, viewport);
    }
    device.attached_viewport = 0;
    process->com().Release(*process, viewport);
    return Succeed(result, dx::kDdOk, error);
}

// SetCurrentViewport(this, lpd3dViewport): the viewport draws transform
// through, held by the device; no viewport is DDERR_INVALIDPARAMS.
bool Device3SetCurrentViewport(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 2, kDeviceObject, error);
    if (process == nullptr)
    {
        return false;
    }
    DeviceState& device = DeviceOf(*process, call.arguments[0]);
    const std::uint32_t viewport = ViewportArgument(*process, call.arguments[1]);
    if (viewport == 0)
    {
        return Succeed(result, dx::kDdErrInvalidParams, error);
    }
    process->com().AddRef(viewport);
    if (device.current_viewport != 0)
    {
        process->com().Release(*process, device.current_viewport);
    }
    device.current_viewport = viewport;
    return Succeed(result, dx::kDdOk, error);
}

// GetCurrentViewport(this, lplpd3dViewport): the current viewport with a new
// reference, or DDERR_NOTFOUND (the out pointer 0) when there is none.
bool Device3GetCurrentViewport(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 2, kDeviceObject, error);
    if (process == nullptr)
    {
        return false;
    }
    if (call.arguments[1] == 0)
    {
        return Succeed(result, dx::kDdErrInvalidParams, error);
    }
    const std::uint32_t viewport = DeviceOf(*process, call.arguments[0]).current_viewport;
    if (!com::WriteWord(call, call.arguments[1], viewport, error))
    {
        return false;
    }
    if (viewport == 0)
    {
        return Succeed(result, dx::kDdErrNotFound, error);
    }
    process->com().AddRef(viewport);
    return Succeed(result, dx::kDdOk, error);
}

// IDirect3DDevice3::GetCaps(this, lpD3DHWDevDesc, lpD3DHELDevDesc) under the
// shared core; the hardware description is written whenever it was of the
// right size, the software one only on DD_OK.
bool Device3GetCaps(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (MethodProcess(call, result, 3, kDeviceObject, error) == nullptr)
    {
        return false;
    }
    const std::uint32_t hardware_address = call.arguments[1];
    const std::uint32_t software_address = call.arguments[2];
    dx::D3dDeviceDesc6 hardware;
    dx::D3dDeviceDesc6 software;
    if ((hardware_address != 0 && !com::ReadStruct(call, hardware_address, &hardware, error)) ||
        (software_address != 0 && !com::ReadStruct(call, software_address, &software, error)))
    {
        return false;
    }
    const bool hardware_sized = hardware_address != 0 && hardware.size == sizeof(dx::D3dDeviceDesc6);
    const std::uint32_t answer =
        dx::GetDevice3Caps(hardware_address == 0 ? nullptr : &hardware, software_address == 0 ? nullptr : &software);
    if ((hardware_sized && !com::WriteStruct(call, hardware_address, hardware, error)) ||
        (answer == dx::kDdOk && software_address != 0 && !com::WriteStruct(call, software_address, software, error)))
    {
        return false;
    }
    return Succeed(result, answer, error);
}

// IDirect3DDevice3 in vtable order (d3d.h). Where the DirectX 7 method takes
// the same arguments it shares that handler; the others (GetCaps's DX6
// descriptions, viewports, IDirect3DTexture2 textures, DirectX 6 vertex
// buffers) stop until modelled.
constexpr com::Method kDevice3Methods[] = {
    {"QueryInterface", 3, &UnimplementedExport},
    {"AddRef", 1, &com::AddRef},
    {"Release", 1, &com::Release},
    {"GetCaps", 3, &Device3GetCaps},
    {"GetStats", 2, &UnimplementedExport},
    {"AddViewport", 2, &Device3AddViewport},
    {"DeleteViewport", 2, &Device3DeleteViewport},
    {"NextViewport", 4, &UnimplementedExport},
    {"EnumTextureFormats", 3, &EnumTextureFormats},
    {"BeginScene", 1, &BeginScene},
    {"EndScene", 1, &EndScene},
    {"GetDirect3D", 2, &UnimplementedExport},
    {"SetCurrentViewport", 2, &Device3SetCurrentViewport},
    {"GetCurrentViewport", 2, &Device3GetCurrentViewport},
    {"SetRenderTarget", 3, &SetRenderTarget},
    {"GetRenderTarget", 2, &GetRenderTarget},
    {"Begin", 4, &UnimplementedExport},
    {"BeginIndexed", 6, &UnimplementedExport},
    {"Vertex", 2, &UnimplementedExport},
    {"Index", 2, &UnimplementedExport},
    {"End", 2, &UnimplementedExport},
    {"GetRenderState", 3, &GetRenderState},
    {"SetRenderState", 3, &SetRenderState},
    {"GetLightState", 3, &Device3GetLightState},
    {"SetLightState", 3, &Device3SetLightState},
    {"SetTransform", 3, &SetTransform},
    {"GetTransform", 3, &GetTransform},
    {"MultiplyTransform", 3, &UnimplementedExport},
    {"DrawPrimitive", 6, &DrawPrimitive},
    {"DrawIndexedPrimitive", 8, &UnimplementedExport},
    {"SetClipStatus", 2, &UnimplementedExport},
    {"GetClipStatus", 2, &UnimplementedExport},
    {"DrawPrimitiveStrided", 6, &UnimplementedExport},
    {"DrawIndexedPrimitiveStrided", 8, &UnimplementedExport},
    {"DrawPrimitiveVB", 6, &DrawPrimitiveVB},
    {"DrawIndexedPrimitiveVB", 6, &Device3DrawIndexedPrimitiveVB},
    {"ComputeSphereVisibility", 6, &UnimplementedExport},
    {"GetTexture", 3, &UnimplementedExport},
    {"SetTexture", 3, &Device3SetTexture},
    {"GetTextureStageState", 4, &GetTextureStageState},
    {"SetTextureStageState", 4, &SetTextureStageState},
    {"ValidateDevice", 2, &UnimplementedExport},
};

}  // namespace

std::span<const com::Method> Direct3DDevice3Methods()
{
    return kDevice3Methods;
}

std::span<const com::Method> Direct3DDevice7Methods()
{
    return kMethods;
}

std::uint32_t CreateDirect3DDevice7(const ImportCall& call,
                                    GuestProcess& process,
                                    std::uint32_t direct_draw,
                                    std::uint32_t render_target,
                                    bool directx6,
                                    std::string* error)
{
    auto state = std::make_shared<DeviceState>();
    if (!directx6)
    {
        state->device = dx::InitialDevice7State();
    }
    state->render_target = render_target;
    GuestComObject object;
    object.kind = kDeviceObject;
    object.parent = direct_draw;
    object.state = state;
    const std::uint32_t device =
        directx6 ? com::CreateObject(call, process, kModule, kDirect3DDevice3, kDevice3Methods, object, error)
                 : com::CreateObject(call, process, kModule, kDirect3DDevice7, kMethods, object, error);
    if (device != 0)
    {
        // As the Windows facade's device does, it keeps the DirectDraw object
        // and its render target alive.
        process.com().AddRef(direct_draw);
        process.com().AddRef(render_target);
        // The device's target is what the guest presents from.
        process.com().Find(direct_draw)->StateAs<DirectDrawState>()->presentation_surface = render_target;
    }
    return device;
}

}  // namespace re2dj::hle::modules::ddraw
