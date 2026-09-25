# 작업 373 설계 — DirectDraw 진입 / Task 373 design — DirectDraw entry

선행: [작업 372 설계](20260926-372-guest-callbacks-and-window.md), [4th 그래픽 경로 분석](../analysis/ez2dj4th-graphics-path.md)

## 배경 / Background

작업 372 뒤 실제 4th는 window를 만들고 `DirectDrawEnumerateExA(callback, NULL, 7)`에서 멈췄다. 게임은 열거 callback 안에서 `DirectDrawCreateEx`로 `IDirectDraw7`을 만든다. Windows 경로의 분석(§1)이 확인한 대로, 그다음 `QueryInterface(IID_IDirect3D7)`로 Direct3D를 얻는다. DirectDraw와 Direct3D의 COM 객체는 게스트가 vtable을 통해 부르므로, Linux facade가 COM 객체를 게스트 메모리에 만들 방법이 먼저 있어야 한다.

*After Task 372 the real 4th created its window and stopped at `DirectDrawEnumerateExA(callback, NULL, 7)`. Inside the enumeration callback it creates an `IDirectDraw7` with `DirectDrawCreateEx`, then asks `QueryInterface(IID_IDirect3D7)` for Direct3D, as the Windows path's analysis (§1) confirmed. The guest calls DirectDraw and Direct3D COM objects through their vtables, so the Linux facade first needs a way to make COM objects in guest memory.*

## 측정 / Measurements

이 host(Windows 11, 32비트 PowerShell)에서 `DirectDrawEnumerateExA`를 측정했다. / *`DirectDrawEnumerateExA` measured on this host (Windows 11, 32-bit PowerShell):*

| flags | callback 인자 / callback arguments |
| --- | --- |
| 0 | GUID NULL, 설명 `"주 디스플레이 드라이버"`(CP949), 이름 `"display"`, 모니터 NULL / GUID NULL, description `"주 디스플레이 드라이버"` (CP949), name `"display"`, monitor NULL |
| 7 | 위 항목, 그다음 모니터마다 GUID `{67685559-3106-11D0-B971-00AA00342F9F}`+n, 그래픽 카드 이름, `"\\.\DISPLAYn"`, HMONITOR / the above, then per monitor GUID `{67685559-…}`+n, the graphics card's name, `"\\.\DISPLAYn"`, and its HMONITOR |

callback이 FALSE를 돌려주면 열거가 멈추고, 결과는 모두 `DD_OK`이며 last error는 바뀌지 않는다.

*A FALSE callback result stops the enumeration; every result is `DD_OK`, with the last error untouched.*

## 결정 / Decisions

1. **facade COM 객체(`GuestComObjects`).** `GuestProcess`가 가진다. 객체는 vtable 포인터 하나짜리 process heap block이다. 나머지 상태는 facade가 block 주소로 관리한다. 인터페이스의 vtable은 처음 쓸 때 한 번 만든다. 그 facade module의 export 가운데 `"<인터페이스>::<메서드>"` 이름을 vtable 순서대로 찾아, 그 thunk 주소로 채워 process heap에 둔다. 그래서 메서드 호출도 일반 import처럼 dispatch되고 API log에 `ddraw.dll!IDirectDraw7::GetCaps` 같은 이름으로 남는다. 구현하지 않은 메서드는 자기 이름을 대고 멈춘다. `AddRef`/`Release`는 공용이다. `Release`가 0이 되면 block을 해제한다.
   ***Facade COM objects (`GuestComObjects`):** owned by `GuestProcess`. An object is a process-heap block holding only its vtable pointer, with the rest kept by the facade under the block's address. An interface's vtable is built once, on first use, from the facade module's exports named `"<interface>::<method>"` in vtable order: their thunk addresses fill a process-heap table. Method calls therefore dispatch like any import and appear in the API log as, say, `ddraw.dll!IDirectDraw7::GetCaps`, and a method not modelled stops naming itself. `AddRef`/`Release` are shared, and `Release` frees the block at zero.*
2. **ddraw module.** `ddraw.dll`을 해석 전용 목록에서 분리한다.
   ***ddraw module:** `ddraw.dll` leaves the resolve-only list.*
   - `DirectDrawEnumerateExA`: 모니터 하나인 시스템이다. 주 드라이버 항목은 측정값 그대로 준다. `DDENUM_ATTACHEDSECONDARYDEVICES`면 `DISPLAY1` 항목을 이어서 준다. 이 항목의 GUID는 측정값이고, 설명은 host 그래픽 카드에 따라 달라지지 않도록 고정 이름 `"re2DJ Display Adapter"`를 쓴다. HMONITOR는 `GuestUser::PrimaryMonitor()`다. 분리된 장치나 표시 장치가 아닌 장치는 없다. 문자열과 GUID는 callback 동안만 process heap에 둔다. callback이 없거나 모르는 flag면 `DDERR_INVALIDPARAMS`다.
     *`DirectDrawEnumerateExA`: a one-monitor system. The primary driver entry is given exactly as measured, and `DDENUM_ATTACHEDSECONDARYDEVICES` adds `DISPLAY1` with the measured GUID. Its description is the fixed name `"re2DJ Display Adapter"`, so it does not depend on the host's graphics card, and its HMONITOR is `GuestUser::PrimaryMonitor()`. There are no detached or non-display devices. The strings and GUID sit in the process heap for the callback only; a missing callback or an unknown flag gives `DDERR_INVALIDPARAMS`.*
   - `DirectDrawCreateEx`: 장치는 NULL(주 표시 장치)과 `DISPLAY1` GUID만 다룬다. `IID_IDirectDraw7`이 아니면 `DDERR_INVALIDPARAMS`다. `DDCREATE_*`, 다른 장치, aggregation은 멈춘다.
     *`DirectDrawCreateEx`: the device is NULL (the primary display) or the `DISPLAY1` GUID; an interface other than `IID_IDirectDraw7` is `DDERR_INVALIDPARAMS`; `DDCREATE_*`, other devices, and aggregation stop.*
   - `IDirectDraw7`: 30개 메서드를 모두 export로 둔다. `QueryInterface`는 IUnknown과 IDirectDraw7에 자기 자신을 준다. 그 밖의 IID는 GUID를 이름으로 대고 멈춘다. `AddRef`/`Release`도 구현했다.
     *`IDirectDraw7`: all 30 methods are exports; `QueryInterface` answers IUnknown and IDirectDraw7 with the object itself and stops on any other IID, naming the GUID; `AddRef`/`Release` are implemented.*

## 범위 밖과 다음 결정 / Out of scope and the next decision

실제 4th는 `QueryInterface(IID_IDirect3D7)`에서 멈춘다. 이 뒤로는 Direct3D 7 장치, 표면, texture, 그리기가 이어진다. Windows 경로에는 이것을 구현한 host COM facade(약 6,700줄)가 있다. 이 facade는 DirectX 6 구현 위에 DirectX 7 vtable을 얹고 SDL3/OpenGL로 그리며, `windows.h`·`ddraw.h` 타입에 묶여 있다. Linux가 이것을 공용 core로 옮겨 쓸지(작업 360의 Hardlock처럼), 아니면 따로 구현할지는 다음 작업에서 정한다.

*The real 4th stops at `QueryInterface(IID_IDirect3D7)`, beyond which lie the Direct3D 7 device, surfaces, textures, and drawing. The Windows path has a host COM facade for these (about 6,700 lines): DirectX 7 vtables over a DirectX 6 implementation, drawing through SDL3/OpenGL, and tied to `windows.h`/`ddraw.h` types. Whether Linux moves it into a shared core (as Task 360 did for Hardlock) or implements its own is settled in the next task.*
