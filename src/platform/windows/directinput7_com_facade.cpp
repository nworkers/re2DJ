#define NOMINMAX
#define DIRECTINPUT_VERSION 0x0700
#define CINTERFACE
#include <windows.h>
#include <dinput.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>

#include "directinput7_com_facade.h"
#include "re2dj/directx/directinput.h"
#include "runtime_log.h"

namespace re2dj::platform::windows
{
namespace
{

namespace dx = re2dj::directx;

// The shared core's DirectInput ABI against the SDK.
static_assert(sizeof(DIDEVCAPS) == sizeof(dx::DiDevCaps));
static_assert(sizeof(DIMOUSESTATE) == sizeof(dx::DiMouseState));
static_assert(offsetof(DIMOUSESTATE, rgbButtons) == offsetof(dx::DiMouseState, buttons));
static_assert(static_cast<std::uint32_t>(DI_OK) == dx::kDiOk);
static_assert(static_cast<std::uint32_t>(DIERR_INVALIDPARAM) == dx::kDiErrInvalidParam);
static_assert(static_cast<std::uint32_t>(DIERR_NOAGGREGATION) == dx::kDiErrNoAggregation);
static_assert(DIDC_ATTACHED == dx::kDidcAttached);
static_assert(DISCL_NONEXCLUSIVE == dx::kDisclNonExclusive);
static_assert(DISCL_FOREGROUND == dx::kDisclForeground);

dx::Guid CoreGuid(const GUID& guid)
{
    dx::Guid core;
    std::memcpy(core.data(), &guid, sizeof(guid));
    return core;
}

using DeviceKind = dx::InputDeviceKind;

struct HleDirectInputDeviceObject
{
    IDirectInputDeviceA iface;
    IDirectInputDeviceAVtbl vtbl;
    std::atomic<ULONG> ref_count{1};
    DeviceKind kind{DeviceKind::kKeyboard};
    bool acquired{false};
    HWND hwnd{nullptr};
    DWORD coop_flags{0};
};

struct HleDirectInputObject
{
    IDirectInputA iface;
    IDirectInputAVtbl vtbl;
    std::atomic<ULONG> ref_count{1};
    DWORD version{0x0700};
};

// IDirectInputDeviceA methods

HRESULT STDMETHODCALLTYPE DeviceQueryInterface(IDirectInputDeviceA* self, REFIID riid, LPVOID* ppvObj)
{
    if (ppvObj == nullptr)
    {
        return E_POINTER;
    }
    if (dx::IsDirectInputDeviceInterface(CoreGuid(riid)))
    {
        *ppvObj = self;
        self->lpVtbl->AddRef(self);
        return S_OK;
    }
    *ppvObj = nullptr;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE DeviceAddRef(IDirectInputDeviceA* self)
{
    auto* obj = reinterpret_cast<HleDirectInputDeviceObject*>(self);
    return ++obj->ref_count;
}

ULONG STDMETHODCALLTYPE DeviceRelease(IDirectInputDeviceA* self)
{
    auto* obj = reinterpret_cast<HleDirectInputDeviceObject*>(self);
    const ULONG remaining = --obj->ref_count;
    if (remaining == 0)
    {
        delete obj;
    }
    return remaining;
}

HRESULT STDMETHODCALLTYPE DeviceGetCapabilities(IDirectInputDeviceA* self, LPDIDEVCAPS lpDIDevCaps)
{
    (void)self;
    if (lpDIDevCaps != nullptr && lpDIDevCaps->dwSize >= sizeof(DIDEVCAPS))
    {
        lpDIDevCaps->dwFlags = DIDC_ATTACHED;
    }
    return DI_OK;
}

HRESULT STDMETHODCALLTYPE DeviceEnumObjects(IDirectInputDeviceA* self,
                                           LPDIENUMDEVICEOBJECTSCALLBACKA lpCallback,
                                           LPVOID pvRef,
                                           DWORD dwFlags)
{
    (void)self;
    (void)lpCallback;
    (void)pvRef;
    (void)dwFlags;
    return DI_OK;
}

HRESULT STDMETHODCALLTYPE DeviceGetProperty(IDirectInputDeviceA* self,
                                           REFGUID rguidProp,
                                           LPDIPROPHEADER pdiph)
{
    (void)self;
    (void)rguidProp;
    (void)pdiph;
    return DI_OK;
}

HRESULT STDMETHODCALLTYPE DeviceSetProperty(IDirectInputDeviceA* self,
                                           REFGUID rguidProp,
                                           LPCDIPROPHEADER pdiph)
{
    (void)self;
    (void)rguidProp;
    (void)pdiph;
    return DI_OK;
}

HRESULT STDMETHODCALLTYPE DeviceAcquire(IDirectInputDeviceA* self)
{
    auto* obj = reinterpret_cast<HleDirectInputDeviceObject*>(self);
    obj->acquired = true;
    if (obj->kind == DeviceKind::kKeyboard)
    {
        re2dj::platform::windows::WriteRuntimeLog(re2dj::platform::windows::RuntimeLogChannel::kRuntime, "re2dj:hle:IDirectInputDevice::Acquire:Keyboard\n");
    }
    else
    {
        re2dj::platform::windows::WriteRuntimeLog(re2dj::platform::windows::RuntimeLogChannel::kRuntime, "re2dj:hle:IDirectInputDevice::Acquire:Mouse\n");
    }
    return DI_OK;
}

HRESULT STDMETHODCALLTYPE DeviceUnacquire(IDirectInputDeviceA* self)
{
    auto* obj = reinterpret_cast<HleDirectInputDeviceObject*>(self);
    obj->acquired = false;
    return DI_OK;
}

HRESULT STDMETHODCALLTYPE DeviceGetDeviceState(IDirectInputDeviceA* self,
                                              DWORD cbData,
                                              LPVOID lpvData)
{
    if (lpvData == nullptr)
    {
        return E_POINTER;
    }
    auto* obj = reinterpret_cast<HleDirectInputDeviceObject*>(self);
    static std::atomic<bool> s_first_keyboard_poll{false};
    if (obj->kind == DeviceKind::kKeyboard && !s_first_keyboard_poll.exchange(true))
    {
        re2dj::platform::windows::WriteRuntimeLog(re2dj::platform::windows::RuntimeLogChannel::kRuntime, "re2dj:hle:IDirectInputDevice::GetDeviceState:Keyboard:first_poll\n");
    }
    // What the host holds, by DirectInput scan code; the core lays it out.
    dx::InputSnapshot snapshot;
    if (obj->kind == DeviceKind::kKeyboard)
    {
        const auto hold = [&snapshot](unsigned scancode) {
            if (scancode > 0 && scancode < snapshot.keys.size())
            {
                snapshot.keys.set(scancode);
            }
        };
        for (int vk = 1; vk < 256; ++vk)
        {
            if ((GetAsyncKeyState(vk) & 0x8000) != 0)
            {
                hold(MapVirtualKeyA(static_cast<UINT>(vk), MAPVK_VK_TO_VSC));
                switch (vk)
                {
                case VK_UP:
                    hold(0xC8);
                    break;
                case VK_DOWN:
                    hold(0xD0);
                    break;
                case VK_LEFT:
                    hold(0xCB);
                    break;
                case VK_RIGHT:
                    hold(0xCD);
                    break;
                case VK_RETURN:
                    hold(0x1C);
                    break;
                case VK_CONTROL:
                case VK_LCONTROL:
                    hold(0x1D);
                    break;
                case VK_RCONTROL:
                    hold(0x9D);
                    break;
                case VK_SHIFT:
                case VK_LSHIFT:
                    hold(0x2A);
                    break;
                case VK_RSHIFT:
                    hold(0x36);
                    break;
                case VK_MENU:
                case VK_LMENU:
                    hold(0x38);
                    break;
                case VK_RMENU:
                    hold(0xB8);
                    break;
                default:
                    break;
                }
            }
        }
    }
    else
    {
        snapshot.mouse_buttons[0] = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        snapshot.mouse_buttons[1] = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
        snapshot.mouse_buttons[2] = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
    }
    dx::ComposeDeviceState(obj->kind, snapshot, std::span<std::uint8_t>(static_cast<std::uint8_t*>(lpvData), cbData));
    return DI_OK;
}

HRESULT STDMETHODCALLTYPE DeviceGetDeviceData(IDirectInputDeviceA* self,
                                             DWORD cbObjectData,
                                             LPDIDEVICEOBJECTDATA rgdod,
                                             LPDWORD pdwInOut,
                                             DWORD dwFlags)
{
    (void)self;
    (void)cbObjectData;
    (void)rgdod;
    (void)dwFlags;
    if (pdwInOut != nullptr)
    {
        *pdwInOut = 0;
    }
    return DI_OK;
}

HRESULT STDMETHODCALLTYPE DeviceSetDataFormat(IDirectInputDeviceA* self, LPCDIDATAFORMAT lpdf)
{
    (void)self;
    (void)lpdf;
    re2dj::platform::windows::WriteRuntimeLog(re2dj::platform::windows::RuntimeLogChannel::kRuntime, "re2dj:hle:IDirectInputDevice::SetDataFormat\n");
    return DI_OK;
}

HRESULT STDMETHODCALLTYPE DeviceSetEventNotification(IDirectInputDeviceA* self, HANDLE hEvent)
{
    (void)self;
    (void)hEvent;
    return DI_OK;
}

HRESULT STDMETHODCALLTYPE DeviceSetCooperativeLevel(IDirectInputDeviceA* self,
                                                   HWND hwnd,
                                                   DWORD dwFlags)
{
    auto* obj = reinterpret_cast<HleDirectInputDeviceObject*>(self);
    obj->hwnd = hwnd;
    obj->coop_flags = dwFlags;
    char msg[128] = {};
    std::snprintf(msg, sizeof(msg), "re2dj:hle:IDirectInputDevice::SetCooperativeLevel:hwnd=%p:flags=0x%08lx\n",
                  static_cast<void*>(hwnd), dwFlags);
    re2dj::platform::windows::WriteRuntimeLog(re2dj::platform::windows::RuntimeLogChannel::kRuntime, msg);
    return DI_OK;
}

HRESULT STDMETHODCALLTYPE DeviceGetObjectInfo(IDirectInputDeviceA* self,
                                             LPDIDEVICEOBJECTINSTANCEA pdidoi,
                                             DWORD dwObj,
                                             DWORD dwHow)
{
    (void)self;
    (void)pdidoi;
    (void)dwObj;
    (void)dwHow;
    return DI_OK;
}

HRESULT STDMETHODCALLTYPE DeviceGetDeviceInfo(IDirectInputDeviceA* self,
                                             LPDIDEVICEINSTANCEA pdidi)
{
    (void)self;
    (void)pdidi;
    return DI_OK;
}

HRESULT STDMETHODCALLTYPE DeviceRunControlPanel(IDirectInputDeviceA* self,
                                               HWND hwndOwner,
                                               DWORD dwFlags)
{
    (void)self;
    (void)hwndOwner;
    (void)dwFlags;
    return DI_OK;
}

HRESULT STDMETHODCALLTYPE DeviceInitialize(IDirectInputDeviceA* self,
                                          HINSTANCE hinst,
                                          DWORD dwVersion,
                                          REFGUID rguid)
{
    (void)self;
    (void)hinst;
    (void)dwVersion;
    (void)rguid;
    return DI_OK;
}

void InitDeviceVtbl(IDirectInputDeviceAVtbl* vtbl)
{
    vtbl->QueryInterface = DeviceQueryInterface;
    vtbl->AddRef = DeviceAddRef;
    vtbl->Release = DeviceRelease;
    vtbl->GetCapabilities = DeviceGetCapabilities;
    vtbl->EnumObjects = DeviceEnumObjects;
    vtbl->GetProperty = DeviceGetProperty;
    vtbl->SetProperty = DeviceSetProperty;
    vtbl->Acquire = DeviceAcquire;
    vtbl->Unacquire = DeviceUnacquire;
    vtbl->GetDeviceState = DeviceGetDeviceState;
    vtbl->GetDeviceData = DeviceGetDeviceData;
    vtbl->SetDataFormat = DeviceSetDataFormat;
    vtbl->SetEventNotification = DeviceSetEventNotification;
    vtbl->SetCooperativeLevel = DeviceSetCooperativeLevel;
    vtbl->GetObjectInfo = DeviceGetObjectInfo;
    vtbl->GetDeviceInfo = DeviceGetDeviceInfo;
    vtbl->RunControlPanel = DeviceRunControlPanel;
    vtbl->Initialize = DeviceInitialize;
}

IDirectInputDeviceA* CreateHleDirectInputDevice(DeviceKind kind)
{
    auto* obj = new HleDirectInputDeviceObject();
    InitDeviceVtbl(&obj->vtbl);
    obj->iface.lpVtbl = &obj->vtbl;
    obj->kind = kind;
    return &obj->iface;
}

// IDirectInputA methods

HRESULT STDMETHODCALLTYPE DiQueryInterface(IDirectInputA* self, REFIID riid, LPVOID* ppvObj)
{
    if (ppvObj == nullptr)
    {
        return E_POINTER;
    }
    if (dx::IsDirectInputInterface(CoreGuid(riid)))
    {
        *ppvObj = self;
        self->lpVtbl->AddRef(self);
        return S_OK;
    }
    *ppvObj = nullptr;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE DiAddRef(IDirectInputA* self)
{
    auto* obj = reinterpret_cast<HleDirectInputObject*>(self);
    return ++obj->ref_count;
}

ULONG STDMETHODCALLTYPE DiRelease(IDirectInputA* self)
{
    auto* obj = reinterpret_cast<HleDirectInputObject*>(self);
    const ULONG remaining = --obj->ref_count;
    if (remaining == 0)
    {
        delete obj;
    }
    return remaining;
}

HRESULT STDMETHODCALLTYPE DiCreateDevice(IDirectInputA* self,
                                        REFGUID rguid,
                                        LPDIRECTINPUTDEVICEA* lplpDirectInputDevice,
                                        LPUNKNOWN pUnkOuter)
{
    (void)self;
    (void)pUnkOuter;
    if (lplpDirectInputDevice == nullptr)
    {
        return E_POINTER;
    }
    // The core names the system keyboard and mouse; this facade has always
    // given anything else a keyboard.
    const DeviceKind kind = dx::DeviceKindOf(CoreGuid(rguid)).value_or(DeviceKind::kKeyboard);
    if (kind == DeviceKind::kMouse)
    {
        re2dj::platform::windows::WriteRuntimeLog(re2dj::platform::windows::RuntimeLogChannel::kRuntime, "re2dj:hle:IDirectInput::CreateDevice:SysMouse\n");
    }
    else if (dx::DeviceKindOf(CoreGuid(rguid)).has_value())
    {
        re2dj::platform::windows::WriteRuntimeLog(re2dj::platform::windows::RuntimeLogChannel::kRuntime, "re2dj:hle:IDirectInput::CreateDevice:SysKeyboard\n");
    }
    *lplpDirectInputDevice = CreateHleDirectInputDevice(kind);
    return DI_OK;
}

HRESULT STDMETHODCALLTYPE DiEnumDevices(IDirectInputA* self,
                                       DWORD dwDevType,
                                       LPDIENUMDEVICESCALLBACKA lpCallback,
                                       LPVOID pvRef,
                                       DWORD dwFlags)
{
    (void)self;
    (void)dwDevType;
    (void)lpCallback;
    (void)pvRef;
    (void)dwFlags;
    return DI_OK;
}

HRESULT STDMETHODCALLTYPE DiGetDeviceStatus(IDirectInputA* self, REFGUID rguidInstance)
{
    (void)self;
    (void)rguidInstance;
    return DI_OK;
}

HRESULT STDMETHODCALLTYPE DiRunControlPanel(IDirectInputA* self, HWND hwndOwner, DWORD dwFlags)
{
    (void)self;
    (void)hwndOwner;
    (void)dwFlags;
    return DI_OK;
}

HRESULT STDMETHODCALLTYPE DiInitialize(IDirectInputA* self, HINSTANCE hinst, DWORD dwVersion)
{
    (void)self;
    (void)hinst;
    (void)dwVersion;
    return DI_OK;
}

void InitDirectInputVtbl(IDirectInputAVtbl* vtbl)
{
    vtbl->QueryInterface = DiQueryInterface;
    vtbl->AddRef = DiAddRef;
    vtbl->Release = DiRelease;
    vtbl->CreateDevice = DiCreateDevice;
    vtbl->EnumDevices = DiEnumDevices;
    vtbl->GetDeviceStatus = DiGetDeviceStatus;
    vtbl->RunControlPanel = DiRunControlPanel;
    vtbl->Initialize = DiInitialize;
}

}  // namespace
}  // namespace re2dj::platform::windows

extern "C" __declspec(dllexport) HRESULT WINAPI
Re2djHleDirectInputCreateA(HINSTANCE hinst,
                           DWORD dwVersion,
                           void** direct_input,
                           IUnknown* punkOuter)
{
    (void)hinst;
    (void)punkOuter;
    if (direct_input == nullptr)
    {
        return E_POINTER;
    }
    re2dj::platform::windows::WriteRuntimeLog(re2dj::platform::windows::RuntimeLogChannel::kRuntime, "re2dj:hle:DirectInputCreateA\n");
    auto* obj = new re2dj::platform::windows::HleDirectInputObject();
    re2dj::platform::windows::InitDirectInputVtbl(&obj->vtbl);
    obj->iface.lpVtbl = &obj->vtbl;
    obj->version = dwVersion;
    *direct_input = &obj->iface;
    return DI_OK;
}
