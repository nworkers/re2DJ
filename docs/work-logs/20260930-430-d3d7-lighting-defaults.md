# 작업 430 작업 로그 — DX7 조명과 깊이 기본값 / Task 430 work log — DX7 lighting and depth defaults

설계: [20260930-430-d3d7-lighting-defaults.md](../design/20260930-430-d3d7-lighting-defaults.md) · 지시서: [20260930-430-d3d7-lighting-defaults.md](../work-orders/20260930-430-d3d7-lighting-defaults.md)

## 2026-09-30

- **작업 전 Win32 6th 상태**
  - 데모, 코인(3코인 1크레딧), 1P 시작, 모드 선택, 곡 선택, 플레이까지 진행했다.
  - 모드 선택 화면에서 모드별 그림만 빠졌다(사용자 보고, 사용자 확인).
  - 첫 실행에서는 시작 직후 화면이 어두웠는데, 이는 화면 전환 중에 캡처된 것이었다.

  *Win32 6th before the task:*
  - *It got through the demo, coins (three coins per credit), 1P start, mode select, music select, and play.*
  - *Only the per-mode pictures were missing from mode select (user report, confirmed with the user).*
  - *A dark screen right after start in the first run was a capture taken mid-transition.*
- **진단 실행**: launcher probe에 제품과 같은 옵션과 `--graphics-draw-diagnostics`를 주어 실행했다(`20260930-014820-412`). 모드 선택 구간에서 확인한 내용은 다음과 같다.
  - `DrawPrimitive` 12,743건 가운데 텍스처 241~273(`fvf=0x112`)만 `unsupported Direct3D3 depth comparison function`으로 실패했다.
  - 게임은 `ZENABLE` 0/1, `LIGHTING` 1, `AMBIENT` `0xffffffff`를 설정하고 `ZFUNC`는 설정하지 않았다.
  - 미구현 기록은 `IDirect3DDevice7::SetMaterial` 하나뿐이었다.

  *A diagnostic run: the launcher probe with the product's options plus `--graphics-draw-diagnostics` (`20260930-014820-412`). In mode select:*
  - *Of 12,743 `DrawPrimitive` calls, only textures 241–273 (`fvf=0x112`) failed, with `unsupported Direct3D3 depth comparison function`.*
  - *The game set `ZENABLE` 0/1, `LIGHTING` 1, and `AMBIENT` `0xffffffff`, but never `ZFUNC`.*
  - *The only unimplemented call recorded was `IDirect3DDevice7::SetMaterial`.*
- **측정**: `rs430.exe`로 새 장치 상태, `lit430.exe`로 광원 없는 조명 색을 측정했다. 결과는 설계의 표와 같다.
  *Measured with `rs430.exe` (a new device's state) and `lit430.exe` (the colour lighting gives with no light); results as in the design's tables.*
- **구현**
  - 공용 core: `InitialDeviceState`의 측정 초기값, `InitialDevice7State`, `UntransformedVertexColor`, `LegacyTransformState::vertex_color`.
  - Windows: DX7 장치 초기화, `SetMaterial`·`GetMaterial`, interop `LegacyDeviceSetMaterial`·`LegacyDeviceGetMaterial`.
  - Linux: DX7 장치 초기화, `GetMaterial`.

  *Implementation:*
  - *The shared core: the measured initial values in `InitialDeviceState`, `InitialDevice7State`, `UntransformedVertexColor`, and `LegacyTransformState::vertex_color`.*
  - *Windows: DX7 device initialisation, `SetMaterial` and `GetMaterial`, and the interop `LegacyDeviceSetMaterial` and `LegacyDeviceGetMaterial`.*
  - *Linux: DX7 device initialisation and `GetMaterial`.*
- **실행 확인**
  - Win32 6th: 모드 선택 화면의 가운데에 Ruby Mix, 왼쪽에 Remember 1st, 오른쪽에 Street Mix 로고가 나온다.
  - Windows 4th·5th: 타이틀 화면이 정상이다(각 30초·45초).

  *Runs:*
  - *Win32 6th: mode select now shows the Ruby Mix logo in the middle, Remember 1st on the left, and Street Mix on the right.*
  - *Windows 4th and 5th: their title screens are fine (30 and 45 seconds).*
- **남은 관찰**: 데모 플레이의 BGA 자리에 색 노이즈가 보인다. 원본 연출인지는 확인하지 못했다(TODO).
  *Still open: colour noise appears in the demo's BGA area; whether it is the original's own effect is unconfirmed (TODO).*
- **테스트 결과**(실패 0):
  - Windows x86: CTest 6개 통과, 단위 5,680 checks.
  - Linux x64·x86: CTest 4개 통과, 단위 5,677 checks.

  *Test results (no failures):*
  - *Windows x86: all 6 CTest tests pass, 5,680 unit checks.*
  - *Linux x64 and x86: all 4 CTest tests pass, 5,677 unit checks.*
