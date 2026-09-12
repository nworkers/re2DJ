# 작업 로그: 실제 창 입력과 4:3 비율 보정

## 결과

Windows host shell과 SDL/OpenGL presentation 경계를 수정했습니다.

- host/guest 양쪽 WndProc에서 `Alt+1`, `Alt+2`, `Alt+3`을 처리합니다.
- host/guest 양쪽에서 client double-click fullscreen 전환을 처리합니다.
- windowed `WM_SIZING`은 non-client frame을 제외한 client 영역을 4:3으로 보정합니다.
- fullscreen 또는 비정형 client 영역에서는 OpenGL이 중앙 4:3 viewport와 검정 여백을 사용합니다.
- profile 설정과 원본 게임 실행 파일, 논리 640x480 render target은 변경하지 않았습니다.

## 원인 판단

기존 runtime probe는 guest HWND에 직접 메시지를 보내므로 실제 포커스가 host 또는 SDL WndProc에 있는 경우를 검증하지 못했습니다. 입력 정책을 host WndProc까지 확장했습니다.

잔상 문제는 `ez2d2m`과 `ez2dj4th` 양쪽에서 공통으로 사용하는 retained-frame D3D 경계의 explicit clear forwarding 누락으로 분류되어 있으며, profile 누락으로 분류하지 않습니다. 이번 작업은 그와 별개로 fullscreen stretch와 window resize 비율을 수정했습니다.

## 검증

- Windows x86 Debug build: 성공
- `ctest --test-dir build/windows-x86 -C Debug --output-on-failure -E re2dj_windows_vfs_runtime_probe`: 4/4 성공
- `re2dj_opengl_blend_probe.exe`: 9 pixel checks, 0 failures
- `re2dj_windows_vfs_runtime_probe.exe --vfs-enumeration-only`: 성공
- 전체 runtime probe: 창 mode 적용 trace에서 15회의 `mode-applied` 전환을 확인했으며, 기존 장치/오디오 lifecycle 대기 구간에서 20초 제한으로 중단했습니다. 제한 시간 때문에 probe 종료 코드는 확보하지 못했습니다.
- 실제 `ez2d2m` launcher: 보호/장치 경계에서 GUI 창 생성 전에 대기하여 실제 키 입력과 화면 캡처는 확인하지 못했습니다.
- Linux x64 build 재검증: 기존 CMake cache가 `/mnt/e/...` 경로로 생성되어 Windows 경로에서 재사용할 수 없었으므로 실행하지 못했습니다.

## 변경 파일

- `src/platform/windows/host_window_shell.cpp`
- `src/graphics/sdl3_opengl_backend.cpp`
- `src/tools/windows_vfs_runtime_probe/main.cpp`
- `ARCHITECTURE.md`
- `docs/analysis/ez2d2m-ez2dj4th-graphics-clear.md`
- `docs/design/20260912-259-window-input-aspect-ratio.md`
- `docs/work-orders/20260912-259-window-input-aspect-ratio.md`

## English

# Work Log: Actual Window Input and 4:3 Aspect-Ratio Correction

## Result

The Windows host-shell and SDL/OpenGL presentation boundaries were updated.

- Both host and guest WndProcs handle `Alt+1`, `Alt+2`, and `Alt+3`.
- Both host and guest paths handle client double-click fullscreen toggles.
- Windowed `WM_SIZING` corrects the client area to 4:3 after excluding the non-client frame.
- Fullscreen or non-shaped client areas use a centered 4:3 OpenGL viewport with black bars.
- Profiles, the original game executable, and the logical 640x480 render target were not changed.

## Cause classification

The existing runtime probe sent messages directly to the guest HWND, so it did not cover real focus on the host or SDL WndProc. The input policy was extended to the host WndProc.

The afterimage issue remains classified as a missing explicit-clear forwarding operation at the shared retained-frame D3D boundary used by both `ez2d2m` and `ez2dj4th`, not as a profile omission. This task separately fixes fullscreen stretching and window-resize aspect ratio.

## Verification

- Windows x86 Debug build: passed
- `ctest --test-dir build/windows-x86 -C Debug --output-on-failure -E re2dj_windows_vfs_runtime_probe`: 4/4 passed
- `re2dj_opengl_blend_probe.exe`: 9 pixel checks, 0 failures
- `re2dj_windows_vfs_runtime_probe.exe --vfs-enumeration-only`: passed
- Full runtime probe: its window-mode trace recorded 15 `mode-applied` transitions before the existing device/audio lifecycle wait; it was stopped at a 20-second limit, so the probe exit code was not obtained.
- Actual `ez2d2m` launcher: waited at the protection/device boundary before creating a GUI window, so real key input and a screen capture could not be verified.
- Linux x64 build recheck: not run because the existing CMake cache was generated with the `/mnt/e/...` path and cannot be reused from the Windows path.

## Changed files

- `src/platform/windows/host_window_shell.cpp`
- `src/graphics/sdl3_opengl_backend.cpp`
- `src/tools/windows_vfs_runtime_probe/main.cpp`
- `ARCHITECTURE.md`
- `docs/analysis/ez2d2m-ez2dj4th-graphics-clear.md`
- `docs/design/20260912-259-window-input-aspect-ratio.md`
- `docs/work-orders/20260912-259-window-input-aspect-ratio.md`
