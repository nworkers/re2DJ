#define NOMINMAX
#define DIRECTINPUT_VERSION 0x0700
#define CINTERFACE
#include <windows.h>
#include <dinput.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "directinput7_com_facade.h"

namespace re2dj::platform::windows
{
namespace
{

constexpr GUID kGuidSysKeyboard = {
    0x6f1d2b61, 0xd5a0, 0x11cf, {0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
constexpr GUID kGuidSysMouse = {
    0x6f1d2b60, 0xd5a0, 0x11cf, {0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
constexpr GUID kIidDirectInputA = {
    0x89521360, 0xaa8a, 0x11cf, {0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
constexpr GUID kIidDirectInput7A = {
    0x579087a0, 0x6667, 0x11cf, {0x94, 0x41, 0x00, 0xaa, 0x00, 0x32, 0x40, 0xd7}};
constexpr GUID kIidDirectInputDeviceA = {
    0x5944e680, 0xc92e, 0x11cf, {0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
constexpr GUID kIidDirectInputDevice7A = {
    0x579087a2, 0x6667, 0x11cf, {0x94, 0x41, 0x00, 0xaa, 0x00, 0x32, 0x40, 0xd7}};
constexpr GUID kIidIUnknown = {
    0x00000000, 0x0000, 0x0000, {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};

bool IsEqualGuid(const GUID& a, const GUID& b)
{
    return std::memcmp(&a, &b, sizeof(GUID)) == 0;
}

enum class DeviceKind
{
    Keyboard,
    Mouse
};

struct HleDirectInputDeviceObject
{
    IDirectInputDeviceA iface;
    IDirectInputDeviceAVtbl vtbl;
    std::atomic<ULONG> ref_count{1};
    DeviceKind kind{DeviceKind::Keyboard};
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
    if (IsEqualGuid(riid, kIidIUnknown) ||
        IsEqualGuid(riid, kIidDirectInputDeviceA) ||
        IsEqualGuid(riid, kIidDirectInputDevice7A))
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
    if (obj->kind == DeviceKind::Keyboard)
    {
        OutputDebugStringA("re2dj:hle:IDirectInputDevice::Acquire:Keyboard\n");
    }
    else
    {
        OutputDebugStringA("re2dj:hle:IDirectInputDevice::Acquire:Mouse\n");
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
    std::memset(lpvData, 0, cbData);
    auto* obj = reinterpret_cast<HleDirectInputDeviceObject*>(self);
    static std::atomic<bool> s_first_keyboard_poll{false};
    if (obj->kind == DeviceKind::Keyboard && !s_first_keyboard_poll.exchange(true))
    {
        OutputDebugStringA("re2dj:hle:IDirectInputDevice::GetDeviceState:Keyboard:first_poll\n");
    }
    if (obj->kind == DeviceKind::Keyboard)
    {
        unsigned char* keys = static_cast<unsigned char*>(lpvData);
        for (int vk = 1; vk < 256; ++vk)
        {
            if ((GetAsyncKeyState(vk) & 0x8000) != 0)
            {
                const UINT scancode = MapVirtualKeyA(static_cast<UINT>(vk), MAPVK_VK_TO_VSC);
                if (scancode > 0 && scancode < cbData)
                {
                    keys[scancode] |= 0x80;
                }
                switch (vk)
                {
                case VK_UP:
                    if (0xC8 < cbData) keys[0xC8] |= 0x80;
                    break;
                case VK_DOWN:
                    if (0xD0 < cbData) keys[0xD0] |= 0x80;
                    break;
                case VK_LEFT:
                    if (0xCB < cbData) keys[0xCB] |= 0x80;
                    break;
                case VK_RIGHT:
                    if (0xCD < cbData) keys[0xCD] |= 0x80;
                    break;
                case VK_RETURN:
                    if (0x1C < cbData) keys[0x1C] |= 0x80;
                    break;
                case VK_CONTROL:
                case VK_LCONTROL:
                    if (0x1D < cbData) keys[0x1D] |= 0x80;
                    break;
                case VK_RCONTROL:
                    if (0x9D < cbData) keys[0x9D] |= 0x80;
                    break;
                case VK_SHIFT:
                case VK_LSHIFT:
                    if (0x2A < cbData) keys[0x2A] |= 0x80;
                    break;
                case VK_RSHIFT:
                    if (0x36 < cbData) keys[0x36] |= 0x80;
                    break;
                case VK_MENU:
                case VK_LMENU:
                    if (0x38 < cbData) keys[0x38] |= 0x80;
                    break;
                case VK_RMENU:
                    if (0xB8 < cbData) keys[0xB8] |= 0x80;
                    break;
                default:
                    break;
                }
            }
        }
    }
    else if (obj->kind == DeviceKind::Mouse)
    {
        if (cbData >= sizeof(DIMOUSESTATE))
        {
            auto* mouse = static_cast<DIMOUSESTATE*>(lpvData);
            if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0) mouse->rgbButtons[0] = 0x80;
            if ((GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0) mouse->rgbButtons[1] = 0x80;
            if ((GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0) mouse->rgbButtons[2] = 0x80;
        }
    }
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
    OutputDebugStringA("re2dj:hle:IDirectInputDevice::SetDataFormat\n");
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
    OutputDebugStringA(msg);
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
    if (IsEqualGuid(riid, kIidIUnknown) ||
        IsEqualGuid(riid, kIidDirectInputA) ||
        IsEqualGuid(riid, kIidDirectInput7A))
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
    DeviceKind kind = DeviceKind::Keyboard;
    if (IsEqualGuid(rguid, kGuidSysMouse))
    {
        kind = DeviceKind::Mouse;
        OutputDebugStringA("re2dj:hle:IDirectInput::CreateDevice:SysMouse\n");
    }
    else if (IsEqualGuid(rguid, kGuidSysKeyboard))
    {
        kind = DeviceKind::Keyboard;
        OutputDebugStringA("re2dj:hle:IDirectInput::CreateDevice:SysKeyboard\n");
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
    OutputDebugStringA("re2dj:hle:DirectInputCreateA\n");
    auto* obj = new re2dj::platform::windows::HleDirectInputObject();
    re2dj::platform::windows::InitDirectInputVtbl(&obj->vtbl);
    obj->iface.lpVtbl = &obj->vtbl;
    obj->version = dwVersion;
    *direct_input = &obj->iface;
    return DI_OK;
}
