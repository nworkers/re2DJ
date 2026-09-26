# 작업 382 작업 로그 — DirectX core 4단계: 장치 / Task 382 work log — DirectX core phase 4: the device

설계: [20260926-382-directx-device.md](../design/20260926-382-directx-device.md)
작업 지시서: [20260926-382-directx-device.md](../work-orders/20260926-382-directx-device.md)

## 진행 / Progress

Windows facade의 `D3dCreateDevice`와 장치 상태 메서드를 읽고, 초기 상태와 규칙을 core `DeviceState`로 옮겼다. SDK와 비교하는 `static_assert`가 장면 HRESULT를 바로잡았다. 처음에는 `MAKE_DDHRESULT(750/751)`로 적었으나, 실제 값은 `MAKE_DDHRESULT(760/761)`(`0x887602F8`/`0x887602F9`)이다.

*The Windows facade's `D3dCreateDevice` and device state methods were read, and their initial state and rules moved into the core's `DeviceState`. The `static_assert`s against the SDK corrected the scene HRESULTs: first written as `MAKE_DDHRESULT(750/751)`, they are `MAKE_DDHRESULT(760/761)` (`0x887602F8`/`0x887602F9`).*

Linux에서 `CreateDevice`가 통과하자 게임은 다음 순서로 진행했다.

1. `GetCaps`를 부른다.
2. depth 표면을 만들어 back buffer에 붙인다.
3. `SetRenderTarget`(같은 back buffer)에서 멈췄다.

`SetRenderTarget`/`GetRenderTarget`을 Windows 규칙대로 더하자, 게임은 이어서 다음을 부르고 글꼴 파일 두 개를 읽은 뒤 `user32!GetForegroundWindow`에서 멈췄다.

- `SetTransform`(view, projection)
- render state 10개
- texture stage state 7개
- `SetMaterial`
- `EnumTextureFormats` 두 번, `GetCaps`

render state와 값의 순서는 Windows 실행 기록과 같다.

*Once `CreateDevice` passed on Linux, the game went through these steps:*

1. *It called `GetCaps`.*
2. *It made a depth surface and attached it to the back buffer.*
3. *It stopped at `SetRenderTarget` (the same back buffer).*

*With `SetRenderTarget`/`GetRenderTarget` added under the Windows rules, it then called the following, read two font files, and stopped at `user32!GetForegroundWindow`:*

- *`SetTransform` (view, projection)*
- *ten render states*
- *seven texture stage states*
- *`SetMaterial`*
- *`EnumTextureFormats` twice, then `GetCaps`*

*The render states and their values come in the same order as in the Windows run's record.*

## 변경 / Changes

- **core**:
  - `direct3d_device.h/.cpp`: `DeviceState`, `IdentityMatrix`, `IsEnumeratedDevice`, `CheckCreateDevice`, `InitialDeviceState`, 상태·장면·viewport 규칙.
  - ABI: `D3dMatrix`, `D3dViewport7`, `D3dMaterial7`, 장면 HRESULT 2개, 상태 index·초기값 상수, `IID_IDirect3DDevice7`.

  ***Core:***
  - *`direct3d_device.h/.cpp`: `DeviceState`, `IdentityMatrix`, `IsEnumeratedDevice`, `CheckCreateDevice`, `InitialDeviceState`, and the state, scene, and viewport rules.*
  - *ABI: `D3dMatrix`, `D3dViewport7`, `D3dMaterial7`, two scene HRESULTs, state indices and initial values, and `IID_IDirect3DDevice7`.*
- **Windows**:
  - `DeviceFacade::state`로 상태를 모았다.
  - `CreateDevice`, 장면, render·light·texture stage state, transform, viewport가 core를 쓴다.
  - `CopyToCore`와 SDK 검사를 추가했다.

  ***Windows:***
  - *The state is gathered into `DeviceFacade::state`.*
  - *`CreateDevice`, scenes, render, light, and texture stage states, transforms, and the viewport use the core.*
  - *`CopyToCore` and the SDK checks are added.*
- **Linux**:
  - `ddraw_device7.cpp`: 구현한 메서드 18개.
  - `IDirect3D7::CreateDevice`.
  - `SurfaceShapeOf`.
  - `com::ReadBytes`/`ReadStruct`.

  ***Linux:***
  - *`ddraw_device7.cpp` with 18 implemented methods.*
  - *`IDirect3D7::CreateDevice`.*
  - *`SurfaceShapeOf`.*
  - *`com::ReadBytes`/`ReadStruct`.*
- **단위 테스트**:
  - `directx_device_test.cpp`: 생성 규칙, 초기 상태, index 한계, 장면, viewport.
  - ddraw: 생성 거절과 성공, 상태·장면·viewport·transform·render target, 해제 시 render target 참조 반환. vtable export는 136개다.

  ***Unit tests:***
  - *`directx_device_test.cpp`: the creation rules, the initial state, index limits, scenes, and the viewport.*
  - *ddraw: creation refusals and success; states, scenes, the viewport, transforms, and the render target; and the render target's reference returned on release. There are 136 vtable exports.*

## 검증 / Validation

| 항목 / Item | 결과 / Result |
| --- | --- |
| Windows x86 build, CTest | exit 0, 우리 코드 경고·오류 없음, 6/6 / exit 0, no warnings or errors from this project, 6/6 |
| Windows 실제 4th, 변경 전(`4532fa1`)·후 각 30초 / real 4th on Windows, pre-change (`4532fa1`) and post-change, 30 s each | 작업 381과 같은 방식으로 가리고 제외했다. 남은 1,410줄이 같다(render state, Clear, Flip 줄 43개 포함). / *Masked and excluded as in Task 381; the remaining 1,410 lines match, including 43 render-state, Clear, and Flip lines.* |
| Linux x64·x86 build, CTest | 경고·오류 없음, 각각 3/3 / no warnings or errors, 3/3 each |
| Linux probe, 기존 진단 네 개 / probes and the four diagnostics | 이전과 같음 / as before |
| 실제 4th, Linux 두 폭 / real 4th, both Linux widths | 호출 1,867번이며, 주소를 정규화하면 두 폭이 같다. `#1828 CreateDevice`부터 `#1859 GetCaps`까지 모두 DD_OK로 끝난다. 글꼴 파일(`fontkr.dat`, `fontEn.dat`)을 읽은 뒤 `#1867 user32!GetForegroundWindow`에서 멈춘다. Hardlock 요청 수는 이전과 같다. / *1,867 calls, identical on both widths after address normalization. Every call from `#1828 CreateDevice` to `#1859 GetCaps` returns DD_OK. The run reads the font files (`fontkr.dat`, `fontEn.dat`) and stops at `#1867 user32!GetForegroundWindow`. The Hardlock request totals are unchanged.* |

## 다음 / Next

`GetForegroundWindow`부터 이어지는 Win32 경계다. 이어서 5단계에서 그리기와 표시를 다룬다(`Clear`, `SetTexture`, 그리기, `Flip`, Linux 창).

*Next are the Win32 boundaries from `GetForegroundWindow`, then phase 5: drawing and presentation (`Clear`, `SetTexture`, draws, `Flip`, the Linux window).*
