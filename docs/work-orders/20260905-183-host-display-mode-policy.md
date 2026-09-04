# Task 183: 호스트 표시 모드 불변 정책 구현

## 작업 목표

게스트의 표시 모드 변경 요청이 호스트 데스크탑 해상도에 도달하는 세 경로를 모두 막습니다. 실행 전후로 호스트 표시 모드가 동일해야 하며, 표시 형태는 창 모드와 현재 해상도를 유지하는 창 모드 전체화면 두 가지로 고정됩니다.

## 선행 문서

- [Task 183 설계](../design/20260905-183-host-display-mode-policy.md)
- [Task 182 작업 로그](../work-logs/20260905-182-directx7-legacy-delegation.md)
- [원본 실행 파일 구조 분석](../analysis/ez2dj-exe-structures.md)
- [Win32 실행 가이드](../guides/windows-x86-runtime.md)

## 구현 범위

1. **경계 분리.** `src/platform/windows/display_mode_boundary.{h,cpp}`를 새로 만들고 표시 모드 경계를 그곳으로 옮깁니다. 지금 `injected_runtime.cpp`에 있는 구현은 이 파일의 얇은 연결만 남깁니다. 독립적으로 이름 붙는 하위 시스템이므로 통합 지점에 누적하지 않습니다.

2. **모든 요청 흡수.** `Re2djHleChangeDisplaySettingsExA`가 요청 모양을 검사하지 않고 전부 흡수하도록 바꿉니다. 호스트 API를 부르지 않고 `DISP_CHANGE_SUCCESSFUL`을 돌려줍니다. `CDS_TEST`도 같습니다.

3. **비확장 진입점 추가.** `Re2djHleChangeDisplaySettingsA`를 같은 정책으로 추가합니다.

4. **요청 기록.** 흡수한 요청을 폭 제한 원장으로 남깁니다. device name 유무, `dmFields`, 폭·높이·색 깊이·주사율, flags를 남깁니다.

5. **정적 슬롯 설치를 무조건으로.** 런처가 `--hle-display-mode` 여부와 무관하게, 런타임이 주입된 모든 실행에서 두 이름의 IAT 슬롯을 우회시킵니다. 슬롯이 없으면 그대로 진행합니다. 기존 옵션은 받아들이되 동작을 바꾸지 않는 것으로 남깁니다.

6. **동적 해석 경로 추가.** `Re2djHleGetProcAddress`가 두 이름에 이 경계를 돌려주게 합니다. VFS 동적 resolver 플래그에 걸지 않습니다. 표시 정책은 VFS 정책과 무관합니다.

7. **문서 갱신.** `ARCHITECTURE.md`에 정책을 반영하고, `docs/guides/windows-x86-runtime.md`의 표시 관련 문구를 정정하며, `docs/analysis/`에 확인된 사실을 반영한 뒤 작업 로그를 남깁니다.

## 비범위

- `EnumDisplaySettingsA` 후킹. 읽기 전용이며 호스트를 바꾸지 않습니다.
- 창 모드 2배 확대가 데스크탑보다 커지는 경우의 처리.
- 이미 바뀐 데스크탑 해상도의 복구.
- DirectDraw `SetDisplayMode` / `SetCooperativeLevel` 동작 변경. 이미 정책을 지킵니다.

## 최소 검증

```powershell
cmd /c scripts\build_win32.bat
.\build\windows-x86\bin\Debug\re2dj_unit_tests.exe
.\build\windows-x86\bin\Debug\re2dj_windows_product_loader_probe.exe
.\build\windows-x86\bin\Debug\re2dj_windows_vfs_runtime_probe.exe
```

호스트 표시 모드를 실행 전후로 비교합니다.

```powershell
$before = Get-CimInstance Win32_VideoController | Select-Object CurrentHorizontalResolution, CurrentVerticalResolution, CurrentBitsPerPixel, CurrentRefreshRate
.\build\windows-x86\bin\Debug\re2dj.exe --hdd .\roms\ez2dj1stse --target ez2dj1stse --run
$after = Get-CimInstance Win32_VideoController | Select-Object CurrentHorizontalResolution, CurrentVerticalResolution, CurrentBitsPerPixel, CurrentRefreshRate
```

## 자기 검증 기준

- 실행 전후의 폭·높이·색 깊이·주사율이 모두 같아야 합니다.
- 1st SE 로그에 흡수 기록이 남아야 합니다. 기록이 없으면 경계가 설치되지 않은 것입니다.
- 화면이 이전과 같이 그려져야 합니다. 흡수가 게스트를 `PostQuitMessage` 분기로 보내면 안 됩니다.
- `--fullscreen` 실행 뒤에도 해상도가 같아야 합니다.

---

# Task 183: Host Display Mode Invariance

## Goal

Close all three paths by which a guest's display-mode change reaches the host desktop, so the host mode is identical before and after a run and the presentation stays either a window or a borderless window at the current desktop resolution.

## Scope

Move the boundary into `src/platform/windows/display_mode_boundary.{h,cpp}`; absorb every request rather than only a known shape; add the non-extended entry point; record what was asked for; install the static slots unconditionally whenever the runtime is injected; cover the dynamic resolution path; update the architecture document, the runtime guide, the analysis topic, and the work log.

## Out Of Scope

Hooking `EnumDisplaySettingsA`; the windowed 2x scale exceeding a small desktop; restoring a resolution an earlier run already changed; DirectDraw `SetDisplayMode` and `SetCooperativeLevel`, which already honour the policy.

## Verification

Build with no warnings, run the unit tests and both probes, then compare the host display mode before and after a 1st SE product run and confirm the picture is unchanged.
