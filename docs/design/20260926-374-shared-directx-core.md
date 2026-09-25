# 작업 374 설계 — 공용 DirectX core와 첫 단계 / Task 374 design — shared DirectX core, first phase

선행: [작업 373 설계](20260926-373-directdraw-entry.md), [4th 그래픽 경로 분석](../analysis/ez2dj4th-graphics-path.md)

## 배경 / Background

작업 373 뒤 Linux 실행은 `IDirectDraw7::QueryInterface(IID_IDirect3D7)`에서 멈췄다. 이 뒤로는 Direct3D 7 장치, 표면, texture, 그리기가 이어진다. Windows 경로에는 이것을 구현한 host COM facade가 이미 있다(`direct3d3_com_facade.cpp` 4,595줄, `direct3d7_com_facade.cpp`, `directdraw7_com_facade.cpp` 등 약 6,700줄). 이 facade는 DirectX 6 구현 위에 DirectX 7 vtable을 얹고, `src/graphics`의 공용 그리기 계층과 SDL3/OpenGL backend로 그린다. 다만 COM 의미 계층은 `windows.h`·`ddraw.h` 타입과 host vtable에 묶여 있다. 사용자는 Linux를 따로 구현하지 않고, 이 의미 계층을 폭 중립 공용 core로 옮겨 두 host가 함께 쓰기로 정했다(2026-09-26). 작업 360의 Hardlock과 같은 방식이다.

*After Task 373 the Linux run stopped at `IDirectDraw7::QueryInterface(IID_IDirect3D7)`, beyond which lie the Direct3D 7 device, surfaces, textures, and drawing. The Windows path already implements them as a host COM facade (`direct3d3_com_facade.cpp` at 4,595 lines, `direct3d7_com_facade.cpp`, `directdraw7_com_facade.cpp`, about 6,700 lines in all): DirectX 7 vtables over a DirectX 6 implementation, drawing through `src/graphics`'s shared draw layer and the SDL3/OpenGL backend, but with the COM semantics tied to `windows.h`/`ddraw.h` types and host vtables. The user chose (2026-09-26) to move those semantics into a width-neutral shared core used by both hosts rather than implement Linux separately, as Task 360 did for Hardlock.*

## 단계 / Phases

한 번에 옮기지 않는다. Linux 실행이 닿는 순서대로 한 층씩 옮기고, 매 단계에서 Windows 실제 4th의 그래픽 기록이 변경 전과 같은지 확인한다.

*Not in one move: one layer at a time, in the order the Linux run reaches them, with each phase checking that the real 4th's graphics record on Windows is unchanged.*

1. **설명 (이 작업)**: guest ABI 구조체, caps, 장치·z-buffer·texture 형식·표시 모드 열거, 장치 식별자.
   ***Descriptions (this task):** guest ABI structures, caps, and the device, depth, texture-format, and display-mode enumerations, plus the device identifier.*
2. **협조 수준과 표시 모드**: `SetCooperativeLevel`, `SetDisplayMode`, Windows 창 모드 정책과의 경계.
   ***Cooperative level and display mode:** `SetCooperativeLevel`, `SetDisplayMode`, and the boundary with the Windows window-mode policy.*
3. **표면**: 설명, backing, attach, lock, `GetDC`/`ReleaseDC`. Linux에서는 GDI DC HLE가 함께 필요하다.
   ***Surfaces:** descriptions, backing, attachments, locking, and `GetDC`/`ReleaseDC`, which on Linux also needs GDI DC HLE.*
4. **장치 상태와 그리기 명령**: render state, transform, texture stage, viewport, `DrawPrimitive`에서 공용 draw command까지.
   ***Device state and draw commands:** render states, transforms, texture stages, the viewport, and `DrawPrimitive` down to the shared draw commands.*
5. **Linux 표시**: SDL3/OpenGL backend를 Linux에서 쓴다.
   ***Linux presentation:** the SDL3/OpenGL backend on Linux.*

## 결정 / Decisions

1. **`re2dj_directx` library.** `include/re2dj/directx/`와 `src/directx/`다. host OS header를 포함하지 않는다. `re2dj_core`가 PUBLIC으로, Windows injected runtime이 PRIVATE으로 link한다.
   ***The `re2dj_directx` library:** `include/re2dj/directx/` and `src/directx/`, with no host OS headers, linked PUBLIC by `re2dj_core` and PRIVATE by the Windows injected runtime.*
2. **guest ABI (`abi.h`).** 32비트 guest가 보는 `DDPIXELFORMAT`, `DDSCAPS2`, `DDSURFACEDESC2`, `DDCAPS`(DirectX 7, 380 byte), `D3DPRIMCAPS`, `D3DDEVICEDESC7`, `DDDEVICEIDENTIFIER2`를 고정 폭 필드로 정의한다. `DDDEVICEIDENTIFIER2`의 LARGE_INTEGER는 word 두 개로, 8-byte 정렬 padding은 명시적인 필드로 둔다. 그래서 host 폭과 compiler가 달라도 크기가 같다. 상수와 GUID도 여기 둔다.
   ***Guest ABI (`abi.h`):** the 32-bit guest's `DDPIXELFORMAT`, `DDSCAPS2`, `DDSURFACEDESC2`, `DDCAPS` (DirectX 7, 380 bytes), `D3DPRIMCAPS`, `D3DDEVICEDESC7`, and `DDDEVICEIDENTIFIER2` as fixed-width fields. `DDDEVICEIDENTIFIER2`'s LARGE_INTEGER is two words and its 8-byte alignment padding an explicit field, so the size holds across host widths and compilers. Constants and GUIDs live here too.*
3. **Windows 검증 (`directx_abi_windows.h`).** Windows adapter는 SDK 구조체와 core 구조체의 크기·주요 offset, 상수 값을 `static_assert`로 비교한다. 그다음 `CopyFromCore`로 복사한다. 첫 build에서 core의 `DDCAPS2_NOPAGELOCKREQUIRED` 값 오류(`0x4000`, 실제 `0x0800`)를 이 검사가 잡았다.
   ***Windows checks (`directx_abi_windows.h`):** the Windows adapter compares the SDK and core structures' sizes and key offsets, and the constants' values, with `static_assert`, then copies through `CopyFromCore`. On the first build it caught a wrong core value for `DDCAPS2_NOPAGELOCKREQUIRED` (`0x4000`; the SDK's is `0x0800`).*
4. **설명.** Windows facade가 채우던 값을 옮긴다. Windows facade가 `directdraw7_com_facade.cpp`와 `direct3d7_com_facade.cpp`로 나뉘어 있듯, DirectDraw 쪽(`directdraw_description.h`)과 Direct3D 쪽(`direct3d_description.h`)을 나눈다.
   ***Descriptions:** the values the Windows facade used to fill in, split into DirectDraw's (`directdraw_description.h`) and Direct3D's (`direct3d_description.h`) as the Windows facade divides `directdraw7_com_facade.cpp` from `direct3d7_com_facade.cpp`.*
   - `DirectDraw7Caps`(`DDCAPS2_CANRENDERWINDOWED` 포함), `DirectDraw4Caps` / *`DirectDraw7Caps` (with `DDCAPS2_CANRENDERWINDOWED`) and `DirectDraw4Caps`;*
   - `Direct3D7Devices`(RGB Emulation, HAL, T&L HAL), `DeviceDescription`, `CreatedDeviceDescription` / *`Direct3D7Devices` (RGB Emulation, HAL, T&L HAL), `DeviceDescription`, and `CreatedDeviceDescription`;*
   - `Depth16Format`, `Rgb565Format` / *`Depth16Format` and `Rgb565Format`;*
   - `DisplayModes`(5개 크기 × 16/24/32비트), `DisplayModeDescription`, 현재 모드 640×480×16, 60 Hz, 비디오 메모리 128 MiB / *`DisplayModes` (5 sizes × 16/24/32 bits), `DisplayModeDescription`, the current mode 640×480×16, 60 Hz, and 128 MiB of video memory;*
   - `DeviceIdentifier` / *`DeviceIdentifier`.*

   Windows의 `IDirect3D7::EnumDevices`/`EnumZBufferFormats`, `IDirect3DDevice7::GetCaps`/`EnumTextureFormats`, `IDirectDraw7::GetCaps`/`EnumDisplayModes`/`GetDisplayMode`/`GetMonitorFrequency`/`GetAvailableVidMem`/`GetDeviceIdentifier`, `IDirectDraw4::GetCaps`가 이것을 쓴다.
   *Windows's `IDirect3D7::EnumDevices`/`EnumZBufferFormats`, `IDirect3DDevice7::GetCaps`/`EnumTextureFormats`, `IDirectDraw7::GetCaps`/`EnumDisplayModes`/`GetDisplayMode`/`GetMonitorFrequency`/`GetAvailableVidMem`/`GetDeviceIdentifier`, and `IDirectDraw4::GetCaps` use them.*
6. **Linux 파일 구성.** facade module은 `ddraw.dll` 하나다. DirectX 7 title은 DirectDraw 객체에 `QueryInterface`해서만 Direct3D를 얻고, Direct3D DLL을 import하지 않기 때문이다. 대신 인터페이스별로 파일을 나눈다. `ddraw_module.cpp`는 진입점과 `IDirectDraw7`, `ddraw_direct3d7.cpp`는 `IDirect3D7`이다. `ddraw_interfaces.h`는 그 사이의 경계(객체 종류, `CreateDirect3D7`)다. COM 메서드 공용 도우미(this 검사, AddRef/Release, callback, guest 쓰기)는 `facade_com.h/.cpp`에 둔다. 이후 표면·장치도 각자의 파일로 더한다.
   ***Linux file layout:** the facade module stays one `ddraw.dll`, since a DirectX 7 title reaches Direct3D only through `QueryInterface` on a DirectDraw object and imports no Direct3D DLL, but the interfaces get a file each: `ddraw_module.cpp` for the entry points and `IDirectDraw7`, `ddraw_direct3d7.cpp` for `IDirect3D7`, and `ddraw_interfaces.h` for the boundary between them (object kinds, `CreateDirect3D7`). The COM method helpers (this checks, AddRef/Release, callbacks, guest writes) live in `facade_com.h/.cpp`; surfaces and devices will add files of their own.*
5. **Linux 동작.** `IDirectDraw7::QueryInterface(IID_IDirect3D7)`가 `IDirect3D7` 객체를 만든다. 이 객체는 Windows facade처럼 DirectDraw 객체의 참조를 잡는다(`GuestComObject::parent`, 해제될 때 부모도 해제). `IDirect3D7`의 `QueryInterface`/`AddRef`/`Release`/`EnumDevices`/`EnumZBufferFormats`/`EvictManagedTextures`와 `IDirectDraw7`의 설명 메서드 6개가 core 값을 쓴다. callback에 넘기는 문자열과 구조체는 callback 동안 process heap에 둔다. 의미는 Windows facade를 따른다. 예를 들어 `IDirect3D7`은 모르는 IID에 `E_NOINTERFACE`를 준다.
   ***Linux:** `IDirectDraw7::QueryInterface(IID_IDirect3D7)` makes an `IDirect3D7` that holds a reference to the DirectDraw object as the Windows facade's does (`GuestComObject::parent`, released after the child). `IDirect3D7`'s `QueryInterface`/`AddRef`/`Release`/`EnumDevices`/`EnumZBufferFormats`/`EvictManagedTextures` and six `IDirectDraw7` description methods answer from the core, with the strings and structures a callback receives placed in the process heap for its duration. The semantics are the Windows facade's (for instance `IDirect3D7` answers an unknown IID with `E_NOINTERFACE`).*
7. **그 밖의 Linux 경계.** 게임이 열거 중 부르는 `kernel32!lstrcpynA`와, 열거 뒤의 `user32!ShowCursor`를 측정값대로 구현한다. Linux 진단의 `ReadGuestString`도 `ReadGuestBytes`와 같은 범위(heap, VirtualAlloc 영역 포함)를 읽게 한다.
   ***Other Linux boundaries:** `kernel32!lstrcpynA`, which the game calls while enumerating, and `user32!ShowCursor` after it, as measured; the Linux diagnostic's `ReadGuestString` reads the same ranges as `ReadGuestBytes` (heap and VirtualAlloc pages included).*

## 범위 밖 / Out of scope

- 2–5단계. 다음은 `SetCooperativeLevel(hwnd, 0x813)`이다. / *Phases 2–5; next is `SetCooperativeLevel(hwnd, 0x813)`.*
- DirectX 6 facade(1st SE 등)의 `IDirect3D3` 열거·caps. `IDirectDraw4::GetCaps`만 옮겼다. / *The DirectX 6 facade's (1st SE and others) `IDirect3D3` enumerations and caps; only `IDirectDraw4::GetCaps` moved.*
