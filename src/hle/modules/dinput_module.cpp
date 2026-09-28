// dinput.dll's entry point, IDirectInputA, and IDirectInputDeviceA,
// following the shared DirectInput core. No host input reaches the devices
// yet, so they report nothing held.

#include "re2dj/hle/modules/dinput_module.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "facade_com.h"
#include "re2dj/directx/abi.h"
#include "re2dj/directx/directinput.h"
#include "re2dj/hle/guest_com.h"
#include "re2dj/hle/guest_process.h"
#include "re2dj/hle/host_presentation.h"

namespace re2dj::hle::modules
{
namespace
{

namespace dx = re2dj::directx;
using com::CallName;
using com::Fail;
using com::MethodProcess;
using com::Succeed;

constexpr std::string_view kModule = "dinput.dll";
constexpr std::string_view kDirectInput = "IDirectInputA";
constexpr std::string_view kDirectInputDevice = "IDirectInputDeviceA";

// GuestComObject::kind of the dinput facade objects, apart from ddraw's and
// dsound's.
constexpr std::uint32_t kDirectInputObject = 0x20;
constexpr std::uint32_t kInputDeviceObject = 0x21;

// A device: what it is, and what the guest has told it.
struct InputDeviceState final : GuestComState
{
    dx::InputDeviceKind kind = dx::InputDeviceKind::kKeyboard;
    std::uint32_t window = 0;
    std::uint32_t cooperative_flags = 0;
    bool acquired = false;
};

InputDeviceState& DeviceOf(GuestProcess& process, std::uint32_t device)
{
    return *process.com().Find(device)->StateAs<InputDeviceState>();
}

// Answers the object's own interfaces with the object itself and anything
// else with E_NOINTERFACE, as the Windows facade answers.
bool AnswerQueryInterface(const ImportCall& call,
                          ImportReturn* result,
                          std::uint32_t kind,
                          bool (*answers)(const dx::Guid&),
                          std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 3, kind, error);
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
    const bool known = answers(iid);
    if (!com::WriteWord(call, call.arguments[2], known ? call.arguments[0] : 0U, error))
    {
        return false;
    }
    if (!known)
    {
        return Succeed(result, dx::kENoInterface, error);
    }
    process->com().AddRef(call.arguments[0]);
    return Succeed(result, dx::kDiOk, error);
}

// ---------------------------------------------------------------------------
// IDirectInputDeviceA
// ---------------------------------------------------------------------------

bool DeviceQueryInterface(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return AnswerQueryInterface(call, result, kInputDeviceObject, &dx::IsDirectInputDeviceInterface, error);
}

// GetCapabilities(this, lpDIDevCaps): attached, into a DIDEVCAPS whose dwSize
// is at least its own; anything else is left alone, as the Windows facade
// leaves it.
bool GetCapabilities(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (MethodProcess(call, result, 2, kInputDeviceObject, error) == nullptr)
    {
        return false;
    }
    std::uint32_t size = 0;
    if (call.arguments[1] != 0)
    {
        if (!com::ReadStruct(call, call.arguments[1], &size, error))
        {
            return false;
        }
        if (size >= sizeof(dx::DiDevCaps) && !com::WriteWord(call, call.arguments[1] + 4, dx::kDidcAttached, error))
        {
            return false;
        }
    }
    return Succeed(result, dx::kDiOk, error);
}

// Acquire and Unacquire: the device follows the guest's request.
bool Acquire(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 1, kInputDeviceObject, error);
    if (process == nullptr)
    {
        return false;
    }
    DeviceOf(*process, call.arguments[0]).acquired = true;
    return Succeed(result, dx::kDiOk, error);
}

bool Unacquire(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 1, kInputDeviceObject, error);
    if (process == nullptr)
    {
        return false;
    }
    DeviceOf(*process, call.arguments[0]).acquired = false;
    return Succeed(result, dx::kDiOk, error);
}

// GetDeviceState(this, cbData, lpvData): the core's layout of what the host
// holds: keys by scan code and the mouse buttons, or nothing with no host.
bool GetDeviceState(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 3, kInputDeviceObject, error);
    if (process == nullptr)
    {
        return false;
    }
    if (call.arguments[2] == 0)
    {
        return Succeed(result, dx::kEPointer, error);
    }
    dx::InputSnapshot snapshot;
    const HostPresentation* presentation = call.services->Presentation();
    if (presentation != nullptr)
    {
        snapshot.keys = presentation->Input().scan_codes;
        snapshot.mouse_buttons = presentation->Input().mouse_buttons;
    }
    std::vector<std::uint8_t> state(call.arguments[1]);
    dx::ComposeDeviceState(DeviceOf(*process, call.arguments[0]).kind, snapshot, state);
    return com::WriteBytes(call, call.arguments[2], state, error) && Succeed(result, dx::kDiOk, error);
}

// SetDataFormat(this, lpdf): accepted, as the Windows facade accepts it; the
// device state layout is the core's.
bool SetDataFormat(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return MethodProcess(call, result, 2, kInputDeviceObject, error) != nullptr && Succeed(result, dx::kDiOk, error);
}

// SetCooperativeLevel(this, hwnd, dwFlags): kept on the device.
bool SetCooperativeLevel(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 3, kInputDeviceObject, error);
    if (process == nullptr)
    {
        return false;
    }
    InputDeviceState& device = DeviceOf(*process, call.arguments[0]);
    device.window = call.arguments[1];
    device.cooperative_flags = call.arguments[2];
    return Succeed(result, dx::kDiOk, error);
}

// IDirectInputDeviceA in vtable order (dinput.h).
constexpr com::Method kDeviceMethods[] = {
    {"QueryInterface", 3, &DeviceQueryInterface},
    {"AddRef", 1, &com::AddRef},
    {"Release", 1, &com::Release},
    {"GetCapabilities", 2, &GetCapabilities},
    {"EnumObjects", 4, &UnimplementedExport},
    {"GetProperty", 3, &UnimplementedExport},
    {"SetProperty", 3, &UnimplementedExport},
    {"Acquire", 1, &Acquire},
    {"Unacquire", 1, &Unacquire},
    {"GetDeviceState", 3, &GetDeviceState},
    {"GetDeviceData", 5, &UnimplementedExport},
    {"SetDataFormat", 2, &SetDataFormat},
    {"SetEventNotification", 2, &UnimplementedExport},
    {"SetCooperativeLevel", 3, &SetCooperativeLevel},
    {"GetObjectInfo", 4, &UnimplementedExport},
    {"GetDeviceInfo", 2, &UnimplementedExport},
    {"RunControlPanel", 3, &UnimplementedExport},
    {"Initialize", 4, &UnimplementedExport},
};

// ---------------------------------------------------------------------------
// IDirectInputA
// ---------------------------------------------------------------------------

bool QueryInterface(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return AnswerQueryInterface(call, result, kDirectInputObject, &dx::IsDirectInputInterface, error);
}

// CreateDevice(this, rguid, lplpDirectInputDevice, pUnkOuter): the system
// keyboard or mouse, holding a reference to this object. Another device is
// not modelled.
bool CreateDevice(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 4, kDirectInputObject, error);
    if (process == nullptr)
    {
        return false;
    }
    const std::uint32_t out = call.arguments[2];
    if (out == 0)
    {
        return Succeed(result, dx::kEPointer, error);
    }
    dx::Guid instance{};
    if (!com::ReadGuid(call, call.arguments[1], &instance, error))
    {
        return false;
    }
    const std::optional<dx::InputDeviceKind> kind = dx::DeviceKindOf(instance);
    if (!kind.has_value())
    {
        return Fail(error, CallName(call) + " has no model of device " + com::FormatGuid(instance));
    }
    auto state = std::make_shared<InputDeviceState>();
    state->kind = *kind;
    GuestComObject object;
    object.kind = kInputDeviceObject;
    object.parent = call.arguments[0];
    object.state = state;
    const std::uint32_t device =
        com::CreateObject(call, *process, kModule, kDirectInputDevice, kDeviceMethods, object, error);
    if (device == 0)
    {
        return false;
    }
    process->com().AddRef(call.arguments[0]);
    if (!com::WriteWord(call, out, device, error))
    {
        process->com().Release(*process, device);
        return false;
    }
    return Succeed(result, dx::kDiOk, error);
}

// IDirectInputA in vtable order (dinput.h).
constexpr com::Method kDirectInputMethods[] = {
    {"QueryInterface", 3, &QueryInterface},
    {"AddRef", 1, &com::AddRef},
    {"Release", 1, &com::Release},
    {"CreateDevice", 4, &CreateDevice},
    {"EnumDevices", 5, &UnimplementedExport},
    {"GetDeviceStatus", 2, &UnimplementedExport},
    {"RunControlPanel", 3, &UnimplementedExport},
    {"Initialize", 3, &UnimplementedExport},
};

// DirectInputCreateA(hinst, dwVersion, lplpDirectInput, punkOuter): the
// facade object, whatever version the guest asks for, as the Windows facade
// answers.
bool DirectInputCreateA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (result == nullptr || call.arguments.size() != 4)
    {
        return Fail(error, result == nullptr ? "dinput result is null"
                                             : "dinput DirectInputCreateA argument shape is invalid");
    }
    *result = {};
    GuestProcess* process = call.services == nullptr ? nullptr : call.services->Process();
    if (process == nullptr)
    {
        return Fail(error, "dinput DirectInputCreateA needs the guest process");
    }
    const std::uint32_t out = call.arguments[2];
    if (out == 0)
    {
        return Succeed(result, dx::kEPointer, error);
    }
    GuestComObject object;
    object.kind = kDirectInputObject;
    const std::uint32_t direct_input =
        com::CreateObject(call, *process, kModule, kDirectInput, kDirectInputMethods, object, error);
    if (direct_input == 0)
    {
        return false;
    }
    if (!com::WriteWord(call, out, direct_input, error))
    {
        process->com().Release(*process, direct_input);
        return false;
    }
    return Succeed(result, dx::kDiOk, error);
}

}  // namespace

GuestModuleDescriptor MakeDinputModuleDescriptor()
{
    GuestModuleDescriptor descriptor;
    descriptor.name = std::string(kModule);
    descriptor.aliases = {"dinput"};
    descriptor.exports.push_back(com::MakeExport("DirectInputCreateA", 4, &DirectInputCreateA));
    com::AddMethods(&descriptor, kDirectInput, kDirectInputMethods);
    com::AddMethods(&descriptor, kDirectInputDevice, kDeviceMethods);
    return descriptor;
}

}  // namespace re2dj::hle::modules
