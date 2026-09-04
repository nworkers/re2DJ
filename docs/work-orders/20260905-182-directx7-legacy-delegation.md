# Task 182: DirectX 7 facade의 DirectX 6 구현 위임

## 작업 목표

`IDirectDraw7`, `IDirectDrawSurface7`, `IDirect3D7`, `IDirect3DDevice7`의 vtable을 DirectX 6 구현에 위임해, EZ2DJ 4th의 그리기 호출이 실제 픽셀과 `Sdl3OpenGlBackend`에 도달하게 합니다. 아직 위임되지 않은 슬롯은 조용히 성공하지 않고 자신을 기록해, 남은 간극을 한 번의 실행으로 열거합니다.

## 선행 문서

- [Task 182 설계](../design/20260905-182-directx7-legacy-delegation.md)
- [Task 166 IDirect3D7 / IDirectDraw7 COM Facade 분리 설계](../design/20260904-166-direct3d7-com-facade.md)
- [Task 181 작업 로그](../work-logs/20260904-181-hardlock-exit-attribution-log.md)
- [Task 179 작업 로그](../work-logs/20260904-179-direct3d7-vertex-buffer-facade.md)

## 구현 범위

1. **내부 경계 헤더 추가.** `src/platform/windows/directdraw_legacy_interop.h`에 DirectX 6 vtable 접근자, root 생성 진입점, 버전 중립 헬퍼를 선언합니다. 공개 ABI가 아니라 facade 사이의 내부 경계입니다.

2. **DirectX 6 facade의 vtable 매개변수화.** `RootFacade`가 `surface_vtable`과 `device_vtable`을 들고, `RootCreateSurface`와 `D3dCreateDevice`가 그 값을 심게 합니다. null이면 DirectX 6 기본값을 씁니다. DirectX 6 경로의 동작은 바뀌지 않아야 합니다.

3. **버전 중립 기능 보강.** DirectX 6 구현이 비어 있던 슬롯 가운데 4th가 실제로 부르는 것을 공용 계층에 구현합니다: `EnumDisplayModes`, `EnumSurfaces`, `GetDisplayMode`, `WaitForVerticalBlank`, `GetAvailableVidMem`, `TestCooperativeLevel`, 표면 `Lock`/`Unlock`, 디바이스 `Clear`, viewport 상태. 두 경로가 함께 씁니다.

4. **`DDSCAPS_ZBUFFER` 표면 수용.** 4th가 만드는 Z 버퍼 표면을 거절하지 않고 받아들입니다. 깊이 버퍼 픽셀은 백엔드가 관리하므로 표면 객체는 서술자만 보관합니다.

5. **DirectDraw 7 위임 vtable.** `directdraw7_com_facade.cpp`의 수용 스텁을 위임표대로 교체합니다. 재사용 슬롯은 DirectX 6 함수 포인터를 명시적으로 대입하고, 미구현 슬롯은 이름을 기록합니다.

6. **Direct3D 7 위임 vtable.** `direct3d7_com_facade.cpp`를 같은 방식으로 교체합니다. `SetTexture`/`GetTexture`는 표면과 텍스처 인터페이스 사이의 오프셋을 변환합니다.

7. **정점 버퍼 통합.** `IDirect3DVertexBuffer7`이 DirectX 6 정점 버퍼 객체를 쓰게 하여 `DrawPrimitiveVB`가 기존 그리기 경로에 닿게 합니다.

8. **표면 픽셀 형식 진단.** `CreateSurface` 기록에 `ddpfPixelFormat`을 남겨 4th 텍스처가 RGB565인지 확인합니다.

9. **빌드 배선과 문서.** 새 파일을 `CMakeLists.txt`에 추가하고, `ARCHITECTURE.md`와 `docs/analysis/`를 갱신한 뒤 작업 로그를 남깁니다.

## 비범위

- 조명, 재질, state block 구현.
- `IDirect3DDevice7::Load`를 통한 관리 텍스처 업로드.
- DirectX 6 경로의 파일 분할(설계 166의 `directdraw4_com_facade` 분리).
- 종료 원인(`.protect` 스텁) 조사.

## 최소 검증

```powershell
cmd /c scripts\build_win32.bat
.\build\windows-x86\bin\Debug\re2dj_unit_tests.exe
.\build\windows-x86\bin\Debug\re2dj_windows_product_loader_probe.exe
```

DirectX 6 회귀 확인입니다.

```powershell
.\build\windows-x86\bin\Debug\re2dj.exe --hdd .\roms\ez2dj1stse --target ez2dj1stse --run
```

4th 실행입니다.

```powershell
$common = '--hdd', "$env:TEMP\re2dj\chd\ez2dj4th", '--target', 'ez2dj4th',
  '--chd', 'roms\ez2dj4th\4thTrax.chd',
  '--target-executable', 'EZ2DJ/EZ2DJ.EXE',
  '--hle-vfs', '--hle-dynamic-vfs', '--hle-d3d3', '--hle-io-ports',
  '--device-mock-lptdi', '--device-mock-lptdi-path-prefix', '\\.\FEnteDev',
  '--device-mock-wts-console-session', '--diagnostic-idle-timeout', '60000'
.\build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe @common --null-context-entry-trace
```

## 자기 검증 기준

- 재사용 슬롯에는 DirectX 7 전용 함수가 없어야 합니다. 있으면 등급이 잘못 분류된 것입니다.
- 1st SE 실행 화면이 이전과 같아야 합니다.
- 4th `.ddraw.log`에 `IDirectDrawSurface7::Flip`이 나타나야 합니다.
- 남은 `not-implemented` 줄이 다음 작업 범위를 결정합니다.

---

# Task 182: Delegating The DirectX 7 Facade To The DirectX 6 Implementation

## Goal

Point the `IDirectDraw7`, `IDirectDrawSurface7`, `IDirect3D7`, and `IDirect3DDevice7` vtables at the DirectX 6 implementation so EZ2DJ 4th's drawing calls reach real pixels and `Sdl3OpenGlBackend`. Slots that are not yet delegated must record themselves instead of succeeding silently, so one run enumerates the remaining gap.

## Scope

Add an internal interop header; parameterize the DirectX 6 root with the surface and device vtables it installs; fill the version-neutral methods the DirectX 6 path left empty; accept `DDSCAPS_ZBUFFER` surfaces; replace both DirectX 7 acceptance-stub files with delegating vtables; unify the vertex buffer object; log the surface pixel format; update the build wiring and documentation.

## Out Of Scope

Lighting, materials, and state blocks; managed-texture upload through `Load`; splitting the DirectX 6 file as design 166 proposes; the exit investigation.

## Verification

Build with no warnings, run the unit tests and the product loader probe, confirm the 1st SE DirectX 6 run is unchanged, then run 4th and check that the graphics trace reaches `IDirectDrawSurface7::Flip` and that the guest window is no longer black.
