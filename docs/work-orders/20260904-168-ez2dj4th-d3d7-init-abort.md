# Task 168: EZ2DJ 4th D3D7 초기화 조기 중단 지점 특정

## 작업 목표

`IDirectDraw7` / `IDirect3D7` HLE facade 연결 이후 게스트가 `IDirect3D7::EnumDevices` 직후 초기화를 포기하는 지점을 실행 증거로 특정합니다. 어느 guard에서 이탈하는지, 그리고 열거 데이터의 어느 속성이 거부되는지를 확정해 `IDirectDraw7::SetDisplayMode`(`RVA 0x00010a6f`)를 다시 도달 가능하게 만드는 근거를 확보합니다.

## 선행 문서

- [Task 168 설계](../design/20260904-168-ez2dj4th-d3d7-init-abort.md)
- [Task 167 작업 로그](../work-logs/20260904-167-ez2dj4th-default-hle-io-ports.md)
- [Task 166 작업 로그](../work-logs/20260904-166-direct3d7-com-facade.md)
- [Task 163 작업 로그](../work-logs/20260903-163-ez2dj4th-guard-failure-source.md)
- [4th Hardlock 런타임 분석](../analysis/ez2dj4th-hardlock-runtime.md)

## 구현 범위

1. **guard 진입 추적 재조준.** `kNullContextEntryPoints`와 Task 162의 guard 이탈 앵커(`0x00011714`, `0x00011738`, `0x00011838`, `0x000106d2`, `0x00010975`, `0x00010a6f`, `0x00010a72`, `0x000107d9`)를 현재 HLE 빌드에서 다시 관측하도록 앵커 테이블을 정리합니다. 새 CLI 옵션은 추가하지 않고 기존 `--null-context-entry-trace` 경로를 재사용합니다.
2. **DX7 facade 호출 원장.** `directdraw7_com_facade.cpp`와 `direct3d7_com_facade.cpp`의 진단 출력을 `g_re2dj_graphics_trace_path`(`.ddraw.log`) 기록으로 옮깁니다. 호출된 vtable 메서드 이름·주요 인자·반환 `HRESULT`를 모두 남기고, 파일 열기와 기록 헬퍼는 두 파일이 공유하도록 별도 단위로 분리합니다.
3. **열거 데이터 기록.** `EnumDisplayModes`가 콜백에 넘긴 `DDSURFACEDESC2`(width·height·bpp·refresh)와 `EnumDevices`가 넘긴 `D3DDEVICEDESC7`(`deviceGUID`·`dwDevCaps`·주요 caps)을 게스트 콜백 반환값과 함께 기록합니다.
4. **대화상자 문구 포착 (사용자 승인 완료).** `MessageBoxA` / `MessageBoxW` 경계를 추가해 caption과 text를 진단 로그에 남기고 기본 응답을 즉시 반환합니다.
5. **열거 데이터 확장 (사용자 결정: 진단과 같은 작업에서 수행).** `EnumDisplayModes`가 320x240부터 1024x768까지 16·24·32비트를 열거하고, `D3DDEVICEDESC7`의 `dpcTriCaps`·`dpcLineCaps`·`dwTextureOpCaps`·블렌드 스테이지 등을 채우며, RGB·HAL·T&L HAL 세 장치를 DirectX 7 표준 이름으로 열거하도록 확장합니다.
6. **문서 갱신.** 설계·작업 로그와 `docs/analysis/ez2dj4th-hardlock-runtime.md`를 관측 결과로 갱신하고, 확인됨 / 추정 / 미확정을 구분해 표기합니다.

## 비범위

- `0x00acd708 + 0x11c` field 직접 주입 또는 게스트 코드 patch.
- Hardlock 응답 material 변경.
- `direct3d3_com_facade`(DirectX 6 경로)의 동작 변경.
- Linux · Web 호스트 경로 변경.

## 최소 검증

```powershell
cmd /c scripts\build_win32.bat
.\build\windows-x86\bin\Debug\re2dj_unit_tests.exe
.\build\windows-x86\bin\Debug\re2dj_windows_product_loader_probe.exe
```

실제 CHD가 있으면 확장 idle 경계로 진단 실행합니다.

```powershell
.\build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe `
  --hdd $env:TEMP\re2dj\chd\ez2dj4th --target ez2dj4th `
  --chd E:\MYWORK\Projects\re2DJ\roms\ez2dj4th\4thTrax.chd `
  --target-executable EZ2DJ/EZ2DJ.EXE `
  --hle-vfs --hle-dynamic-vfs --hle-d3d3 --hle-io-ports --hle-message-box `
  --device-mock-lptdi --device-mock-lptdi-path-prefix '\\.\FEnteDev' `
  --device-mock-wts-console-session `
  --null-context-entry-trace --diagnostic-idle-timeout 60000
```

제품 기본 경로도 함께 확인합니다.

```powershell
.\build\windows-x86\bin\Debug\re2dj.exe ez2dj4th
```

## 자기 검증 기준

- guard 진입 추적에서 `0x00010a6f` hit 수로 `SetDisplayMode` 도달 여부가 확인 사실이 되어야 합니다.
- `.ddraw.log`가 detached 제품 실행에서도 생성되어, 이전에 `OutputDebugStringA`로만 보이던 호출 순서가 그대로 남아야 합니다.
- 로그에는 원본 자산 내용과 Hardlock secret 값을 기록하지 않습니다.

---

# Task 168: EZ2DJ 4th D3D7 Initialization Early-Abort Localization

## Objective

Localize, with execution evidence, where the guest abandons initialization immediately after `IDirect3D7::EnumDevices` now that the `IDirectDraw7` / `IDirect3D7` HLE facades are attached. Establish which guard exits and which enumerated attribute is rejected, producing the basis for making `IDirectDraw7::SetDisplayMode` (`RVA 0x00010a6f`) reachable again.

## Preceding documents

- [Task 168 design](../design/20260904-168-ez2dj4th-d3d7-init-abort.md)
- [Task 167 work log](../work-logs/20260904-167-ez2dj4th-default-hle-io-ports.md)
- [Task 166 work log](../work-logs/20260904-166-direct3d7-com-facade.md)
- [Task 163 work log](../work-logs/20260903-163-ez2dj4th-guard-failure-source.md)
- [4th Hardlock runtime analysis](../analysis/ez2dj4th-hardlock-runtime.md)

## Implementation scope

1. **Retarget the guard entry trace.** Point the anchor table at `kNullContextEntryPoints` plus Task 162's guard exit anchors (`0x00011714`, `0x00011738`, `0x00011838`, `0x000106d2`, `0x00010975`, `0x00010a6f`, `0x00010a72`, `0x000107d9`) under the current HLE build. Add no new CLI option; reuse the existing `--null-context-entry-trace` path.
2. **DX7 facade call ledger.** Move the diagnostic output of `directdraw7_com_facade.cpp` and `direct3d7_com_facade.cpp` to the `g_re2dj_graphics_trace_path` (`.ddraw.log`) sink. Record every called vtable method's name, key arguments, and returned `HRESULT`, and factor the file-open and write helpers into one unit shared by both files.
3. **Record the enumerated data.** Log the `DDSURFACEDESC2` (width, height, bpp, refresh) handed to the `EnumDisplayModes` callback and the `D3DDEVICEDESC7` (`deviceGUID`, `dwDevCaps`, key caps) handed to the `EnumDevices` callback, each with the guest callback's return value.
4. **Capture dialog text (user approved).** Add a `MessageBoxA` / `MessageBoxW` boundary that records the caption and text into the diagnostic log and returns a default answer immediately.
5. **Expand the enumerated data (user decision: same task as the diagnosis).** Enumerate 320x240 through 1024x768 at 16, 24, and 32 bits from `EnumDisplayModes`; fill `D3DDEVICEDESC7`'s `dpcTriCaps`, `dpcLineCaps`, `dwTextureOpCaps`, and blend-stage fields; and enumerate the RGB, HAL, and T&L HAL devices under their standard DirectX 7 names.
6. **Update documentation.** Update the design, the work log, and `docs/analysis/ez2dj4th-hardlock-runtime.md` with the observations, marking each statement confirmed, inferred, or unresolved.

## Out of scope

- Direct injection into `0x00acd708 + 0x11c`, or patching guest code.
- Changing Hardlock response material.
- Changing `direct3d3_com_facade` (the DirectX 6 path).
- Changing the Linux or Web host paths.

## Minimum verification

Build, unit tests, and the product loader probe as in the PowerShell block above. When the real CHD is available, run the diagnostic with the extended idle boundary, then confirm the product default path with `re2dj.exe ez2dj4th`.

## Self-check criteria

- The guard entry trace's hit count at `0x00010a6f` must turn "`SetDisplayMode` was not reached" into a confirmed fact.
- `.ddraw.log` must be produced by the detached product run too, preserving the call order that previously appeared only through `OutputDebugStringA`.
- The logs must not contain original asset contents or Hardlock secret values.
