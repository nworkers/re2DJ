# 작업 382 설계 — DirectX core 4단계: 장치 / Task 382 design — DirectX core phase 4: the device

선행: [작업 381 설계](20260926-381-directx-surfaces.md)

## 배경 / Background

작업 381 뒤 Linux 실행은 `IDirect3D7::CreateDevice(HAL, back buffer)`에서 멈췄다. Windows facade는 DirectX 6 장치(`DeviceFacade`)에 상태를 두고, DirectX 7 장치는 그 구현을 그대로 쓴다. 규칙은 다음과 같다.

- `CreateDevice`는 열거한 세 장치 class(RGB, HAL, T&L HAL)만 받는다. render target은 `DDSCAPS_3DDEVICE` 표면이어야 한다. 아니면 `DDERR_INVALIDOBJECT`다.
- 새 장치는 초기 상태를 갖는다.
  - 반시계 방향 culling, `ONE`/`ZERO` blending.
  - stage 0: texture×diffuse modulate, point filter, wrap.
  - world·view·projection은 단위 행렬.
- 상태 배열의 범위를 벗어난 index는 `DDERR_INVALIDPARAMS`다. 크기는 render state 256, light state 256, texture stage 8×64, transform 32다.
- 장면 안에서 `BeginScene`을 다시 부르면 `D3DERR_SCENE_IN_SCENE`이다. 장면 밖에서 `EndScene`을 부르면 `D3DERR_SCENE_NOT_IN_SCENE`이다.
- viewport는 폭과 높이가 있어야 한다. 설정하기 전에 읽으면 `DDERR_NOTFOUND`다.
- `SetRenderTarget`은 같은 DirectDraw의 표면이면 받는다.
- `SetMaterial`은 `D3D_OK`만 답한다.

*After Task 381 a Linux run stopped at `IDirect3D7::CreateDevice(HAL, back buffer)`. The Windows facade keeps device state on its DirectX 6 device (`DeviceFacade`), which the DirectX 7 device reuses, with these rules:*

- *`CreateDevice` takes only the three enumerated device classes (RGB, HAL, T&L HAL) and a `DDSCAPS_3DDEVICE` render target, else `DDERR_INVALIDOBJECT`.*
- *A new device starts in an initial state:*
  - *counter-clockwise culling and `ONE`/`ZERO` blending;*
  - *stage 0 modulating texture by diffuse, with point filtering and wrapping;*
  - *identity world, view, and projection.*
- *An index outside a state table is `DDERR_INVALIDPARAMS`. The tables are 256 render states, 256 light states, 8×64 texture stage states, and 32 transforms.*
- *`BeginScene` inside a scene is `D3DERR_SCENE_IN_SCENE`, and `EndScene` outside one is `D3DERR_SCENE_NOT_IN_SCENE`.*
- *A viewport needs a width and height, and reading one before it is set is `DDERR_NOTFOUND`.*
- *`SetRenderTarget` takes any surface of the same DirectDraw object.*
- *`SetMaterial` answers `D3D_OK`.*

## 결정 / Decisions

1. **core (`direct3d_device.h`).** `DeviceState`는 위 상태 배열과 material, viewport, 장면 여부를 guest의 index 그대로 담는다. `InitialDeviceState`, `IsEnumeratedDevice`, `CheckCreateDevice`, 그리고 상태 메서드 규칙(`Set/GetRenderState`, `Set/GetLightState`, `Set/GetTextureStageState`, `Set/GetTransform`, `BeginScene`/`EndScene`, `Set/GetViewport`)을 core에 둔다. guest 포인터 검사는 각 facade가 한다. ABI에는 `D3DMATRIX`, `D3DVIEWPORT7`, `D3DMATERIAL7`, 장면 HRESULT 둘, 상태 index와 초기값 상수, `IID_IDirect3DDevice7`를 더한다.
   ***Core (`direct3d_device.h`):** `DeviceState` holds the tables above, the material, the viewport, and whether a scene is open, indexed by the guest's own numbers. `InitialDeviceState`, `IsEnumeratedDevice`, `CheckCreateDevice`, and the state methods' rules (`Set/GetRenderState`, `Set/GetLightState`, `Set/GetTextureStageState`, `Set/GetTransform`, `BeginScene`/`EndScene`, `Set/GetViewport`) live in the core; each facade checks the guest's pointers. The ABI gains `D3DMATRIX`, `D3DVIEWPORT7`, `D3DMATERIAL7`, the two scene HRESULTs, the state indices and initial values, and `IID_IDirect3DDevice7`.*
2. **Windows.** `DeviceFacade`의 상태 필드 일곱 개를 `re2dj::directx::DeviceState state` 하나로 바꾼다. 바뀌는 필드는 render·light·texture stage state, transform, viewport, viewport 여부, 장면 여부다. `CreateDevice`와 상태 메서드는 core를 쓴다. `D3DMATRIX`는 `CopyToCore`/`CopyFromCore`로 옮긴다. 진단 기록(render state, texture stage state 보고)은 그대로 둔다. 새 구조체와 상수는 SDK와 `static_assert`로 비교한다.
   ***Windows:** `DeviceFacade`'s seven state fields become one `re2dj::directx::DeviceState state`: the render, light, and texture stage states, the transforms, the viewport, whether a viewport is set, and whether a scene is open. `CreateDevice` and the state methods use the core, with `D3DMATRIX` moved by `CopyToCore`/`CopyFromCore`. The diagnostic records (render-state and texture-stage-state reports) stay as they are, and the new structures and constants are checked against the SDK with `static_assert`.*
3. **Linux (`ddraw_device7.cpp`).** `IDirect3DDevice7` 객체는 core `DeviceState`와 render target을 갖는다. render target의 참조와, parent인 DirectDraw 객체의 참조를 잡는다. 구현하는 메서드는 다음과 같다.
   - `QueryInterface`(IUnknown·자기 자신만), `AddRef`, `Release`
   - `GetCaps`, `EnumTextureFormats`
   - `BeginScene`, `EndScene`
   - `SetRenderTarget`, `GetRenderTarget`
   - `SetTransform`, `GetTransform`, `SetViewport`, `GetViewport`
   - `SetMaterial`(장치에 보관)
   - `SetRenderState`, `GetRenderState`, `SetTextureStageState`, `GetTextureStageState`

   나머지 slot은 불리면 멈춘다. `IDirect3D7::CreateDevice`가 core 규칙으로 장치를 만든다.

   ***Linux (`ddraw_device7.cpp`):** an `IDirect3DDevice7` object holds a core `DeviceState` and its render target, with references to that target and to its parent DirectDraw object. It implements:*
   - *`QueryInterface` (IUnknown and itself only), `AddRef`, `Release`*
   - *`GetCaps`, `EnumTextureFormats`*
   - *`BeginScene`, `EndScene`*
   - *`SetRenderTarget`, `GetRenderTarget`*
   - *`SetTransform`, `GetTransform`, `SetViewport`, `GetViewport`*
   - *`SetMaterial` (kept on the device)*
   - *`SetRenderState`, `GetRenderState`, `SetTextureStageState`, `GetTextureStageState`*

   *The other slots stop when called. `IDirect3D7::CreateDevice` makes the device under the core's rules.*

## 범위 밖 / Out of scope

- 5단계: `Clear`, 그리기, `SetTexture`, `Flip`, Linux 창에 표시하기. / *Phase 5: `Clear`, drawing, `SetTexture`, `Flip`, and presenting in the Linux window.*
- Windows DirectX 7 장치의 `SetMaterial`은 지금처럼 보관하지 않고 미구현으로 기록한다. Windows는 lighting을 그리지 않기 때문이다. / *The Windows DirectX 7 device's `SetMaterial` still records itself as unimplemented without keeping the material, as Windows draws no lighting.*
