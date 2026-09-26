// IDirect3DDevice7 of the ddraw facade: the shared DirectX core's device
// state, set and read by the guest. Drawing and presentation come later.

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "ddraw_interfaces.h"
#include "re2dj/directx/abi.h"
#include "re2dj/directx/direct3d_description.h"
#include "re2dj/directx/direct3d_device.h"
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

// A device's state and the render target whose reference it holds.
struct DeviceState final : GuestComState
{
    dx::DeviceState device = dx::InitialDeviceState();
    std::uint32_t render_target = 0;

    std::vector<std::uint32_t> HeldReferences() const override { return {render_target}; }
};

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

// SetMaterial(this, lpMaterial): kept on the device. The Windows facade
// answers D3D_OK without keeping it, as its draw path lights nothing.
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
    {"Clear", 7, &UnimplementedExport},
    {"SetTransform", 3, &SetTransform},
    {"GetTransform", 3, &GetTransform},
    {"SetViewport", 2, &SetViewport},
    {"MultiplyTransform", 3, &UnimplementedExport},
    {"GetViewport", 2, &GetViewport},
    {"SetMaterial", 2, &SetMaterial},
    {"GetMaterial", 2, &UnimplementedExport},
    {"SetLight", 3, &UnimplementedExport},
    {"GetLight", 3, &UnimplementedExport},
    {"SetRenderState", 3, &SetRenderState},
    {"GetRenderState", 3, &GetRenderState},
    {"BeginStateBlock", 1, &UnimplementedExport},
    {"EndStateBlock", 2, &UnimplementedExport},
    {"PreLoad", 2, &UnimplementedExport},
    {"DrawPrimitive", 6, &UnimplementedExport},
    {"DrawIndexedPrimitive", 8, &UnimplementedExport},
    {"SetClipStatus", 2, &UnimplementedExport},
    {"GetClipStatus", 2, &UnimplementedExport},
    {"DrawPrimitiveStrided", 6, &UnimplementedExport},
    {"DrawIndexedPrimitiveStrided", 8, &UnimplementedExport},
    {"DrawPrimitiveVB", 6, &UnimplementedExport},
    {"DrawIndexedPrimitiveVB", 8, &UnimplementedExport},
    {"ComputeSphereVisibility", 6, &UnimplementedExport},
    {"GetTexture", 3, &UnimplementedExport},
    {"SetTexture", 3, &UnimplementedExport},
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

}  // namespace

std::span<const com::Method> Direct3DDevice7Methods()
{
    return kMethods;
}

std::uint32_t CreateDirect3DDevice7(const ImportCall& call,
                                    GuestProcess& process,
                                    std::uint32_t direct_draw,
                                    std::uint32_t render_target,
                                    std::string* error)
{
    auto state = std::make_shared<DeviceState>();
    state->render_target = render_target;
    GuestComObject object;
    object.kind = kDeviceObject;
    object.parent = direct_draw;
    object.state = state;
    const std::uint32_t device =
        com::CreateObject(call, process, kModule, kDirect3DDevice7, kMethods, object, error);
    if (device != 0)
    {
        // As the Windows facade's device does, it keeps the DirectDraw object
        // and its render target alive.
        process.com().AddRef(direct_draw);
        process.com().AddRef(render_target);
    }
    return device;
}

}  // namespace re2dj::hle::modules::ddraw
