# 작업 381 작업 로그 — DirectX core 3단계: 표면 / Task 381 work log — DirectX core phase 3: surfaces

설계: [20260926-381-directx-surfaces.md](../design/20260926-381-directx-surfaces.md)
작업 지시서: [20260926-381-directx-surfaces.md](../work-orders/20260926-381-directx-surfaces.md)

## 진행 / Progress

Windows facade의 `RootCreateSurface`와 표면 메서드를 읽고, 표면의 종류·크기·caps를 정하는 규칙을 core의 `PlanCreateSurface`로 옮겼다. SDK와 비교하는 `static_assert`가 core 상수 둘을 바로잡았다.

- `DDCAPS2_NOPAGELOCKREQUIRED`는 `0x800`이다.
- `DDERR_CANNOTATTACHSURFACE`는 `MAKE_DDHRESULT(10)` = `0x8876000A`다.

*The rules in the Windows facade's `RootCreateSurface` and surface methods that decide a surface's kind, size, and caps moved into the core's `PlanCreateSurface`. The `static_assert`s against the SDK corrected two core constants: `DDCAPS2_NOPAGELOCKREQUIRED` is `0x800`, and `DDERR_CANNOTATTACHSURFACE` is `MAKE_DDHRESULT(10)` = `0x8876000A`.*

Linux에서는 표면 픽셀을 guest `VirtualAlloc` 메모리에 두었다. 640×480 표면이 단위 테스트의 가짜 guest 메모리에 들어가지 않아, 그 크기를 4 MiB로 늘렸다. 실제 4th는 다음 순서로 진행한 뒤 `IDirect3D7::CreateDevice`에서 멈췄다.

1. `CreateSurface`로 flip 주 표면과 back buffer를 만든다.
2. `GetAttachedSurface(BACKBUFFER)`로 back buffer를 얻는다.
3. `AddRef`를 부른다.
4. `QueryInterface(IID_IDirect3D7)`를 부른다.

*On Linux the surface pixels live in guest `VirtualAlloc` memory; the unit tests' fake guest memory was raised to 4 MiB to hold 640×480 surfaces. The real 4th then went through these steps and stopped at `IDirect3D7::CreateDevice`:*

1. *`CreateSurface` makes the flipping primary and its back buffer.*
2. *`GetAttachedSurface(BACKBUFFER)` returns the back buffer.*
3. *`AddRef`.*
4. *`QueryInterface(IID_IDirect3D7)`.*

## 변경 / Changes

- **core**:
  - `directdraw_surface.h/.cpp`: `SurfaceKind`, `SurfaceShape`, `SurfacePlan`, `PlanCreateSurface`, `IsRgb565Format`, `Rgb565Pitch`, `SurfaceDescription`, `CheckAttachment`, `QueryAttachment`.
  - HRESULT 6개, DDSCAPS 5개, DDSD 2개, `IID_IDirectDrawSurface7`.

  ***Core:***
  - *`directdraw_surface.h/.cpp`: `SurfaceKind`, `SurfaceShape`, `SurfacePlan`, `PlanCreateSurface`, `IsRgb565Format`, `Rgb565Pitch`, `SurfaceDescription`, `CheckAttachment`, `QueryAttachment`.*
  - *Six HRESULTs, five DDSCAPS, two DDSD flags, and `IID_IDirectDrawSurface7`.*
- **Windows**:
  - `RootCreateSurface`가 plan을 따른다(GDI backing은 픽셀이 있는 표면만).
  - `AddAttachedSurface`, `GetAttachedSurface`, `GetSurfaceDesc`, RGB565 형식, pitch가 core를 쓴다.
  - SDK 값 검사를 추가했다.

  ***Windows:***
  - *`RootCreateSurface` follows the plan, with GDI backing only for surfaces with pixels.*
  - *`AddAttachedSurface`, `GetAttachedSurface`, `GetSurfaceDesc`, the RGB565 format, and the pitch use the core.*
  - *More SDK value checks.*
- **Linux**:
  - `GuestComState::HeldReferences`/`ReleaseResources`.
  - `ddraw_surface7.cpp`(`SurfaceState`, `CreateSurfaces`, 구현한 메서드 9개).
  - `IDirectDraw7::CreateSurface`.

  ***Linux:***
  - *`GuestComState::HeldReferences`/`ReleaseResources`.*
  - *`ddraw_surface7.cpp` (`SurfaceState`, `CreateSurfaces`, nine implemented methods).*
  - *`IDirectDraw7::CreateSurface`.*
- **단위 테스트**:
  - `directx_surface_test.cpp`: 4th의 표면 세 종류, 거절, 설명·pitch·attach 조회.
  - ddraw: flip 주 표면과 back buffer, depth attach, texture 형식 거절, 모든 참조를 놓은 뒤 메모리 반환. vtable export는 87개다.

  ***Unit tests:***
  - *`directx_surface_test.cpp`: the 4th's three surface kinds, refusals, and descriptions, pitch, and attachment queries.*
  - *ddraw: the flipping primary and back buffer, the depth attachment, the texture format refusal, and memory returned once every reference is released; there are 87 vtable exports.*

## 검증 / Validation

| 항목 / Item | 결과 / Result |
| --- | --- |
| Windows x86 build, CTest | exit 0, 우리 코드 경고·오류 없음, 6/6 / exit 0, no warnings or errors from this project, 6/6 |
| Windows 실제 4th, 변경 전(`854f600`)·후 각 30초 / real 4th on Windows, pre-change (`854f600`) and post-change, 30 s each | 가린 값: 시각·스레드 id·창 handle·PID·frame 시간. 제외한 줄: `present-interval`, 창 수명·제목 갱신 줄(시간 통계). 남은 1,410줄은 같다. 표면 관련 줄(`CreateSurface`, `GetSurfaceDesc`, attach)은 양쪽 모두 218줄이다. / *Masked: timestamps, thread ids, window handles, PIDs, frame times. Excluded: `present-interval`, window lifetime and title-update lines (timing statistics). The remaining 1,410 lines match, including 218 surface lines (`CreateSurface`, `GetSurfaceDesc`, attachments) on each side.* |
| Linux x64·x86 build, CTest | 경고·오류 없음, 각각 3/3 / no warnings or errors, 3/3 each |
| Linux probe, 기존 진단 네 개 / probes and the four diagnostics | 이전과 같음 / as before |
| 실제 4th, Linux 두 폭 / real 4th, both Linux widths | 호출 1,828번이며, 주소를 정규화하면 두 폭이 같다. 호출 순서: `#1824 CreateSurface` → DD_OK, `#1825 GetAttachedSurface` → DD_OK, `#1826 AddRef` → 3, `#1827 QueryInterface(IID_IDirect3D7)`. `#1828 IDirect3D7::CreateDevice`에서 멈춘다. Hardlock 요청 수는 이전과 같다. / *1,828 calls, identical on both widths after address normalization. The calls run `#1824 CreateSurface` → DD_OK, `#1825 GetAttachedSurface` → DD_OK, `#1826 AddRef` → 3, `#1827 QueryInterface(IID_IDirect3D7)`, then stop at `#1828 IDirect3D7::CreateDevice`. The Hardlock request totals are unchanged.* |

## 다음 / Next

4단계는 장치다. `IDirect3D7::CreateDevice`와 장치 상태(render state, viewport, 행렬)를 core로 옮긴다. 남은 표면 메서드는 게임이 도달하는 대로 더한다(`Lock`, `GetDC`와 Linux GDI DC, `Blt`, `Flip`).

*Phase 4, the device: `IDirect3D7::CreateDevice` and device state (render states, viewport, matrices) in the core. The remaining surface methods (`Lock`, `GetDC` with a Linux GDI DC, `Blt`, `Flip`) follow as the game reaches them.*
