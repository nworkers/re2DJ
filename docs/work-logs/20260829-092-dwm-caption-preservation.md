# Win32 DWM 캡션 보존 작업 로그

관련 설계: [Win32 DWM 캡션 보존](../design/20260829-092-dwm-caption-preservation.md)
작업 지시: [작업 092](../work-orders/20260829-092-dwm-caption-preservation.md)

## 한국어

### 결과

- 작업 091을 `main`에 squash merge해 version `0.0.15`, commit `4da5272`, local annotated tag `v0.0.15`를 만들고 이전 작업 브랜치를 삭제했다.
- windowed에서도 `DWMNCRP_ENABLED`를 유지하도록 변경했다.
- 명시적 DWM 활성화, SDL wrapping 뒤 WndProc 재배치, `DwmDefWindowProc`, 정상 `SetWindowText`, DWM 색상 보정만으로는 빈 caption이 해결되지 않았다.
- 표준 top-level host shell을 추가하고 원본 HWND를 1280×960 client child로 배치했다. SDL3/OpenGL은 원본 HWND를 계속 external rendering 대상으로 사용한다.
- host shell이 제목·icon·resize/fullscreen·close와 lifetime 감시를 소유한다. 일반 `WM_SETTEXT`/`WM_SETICON` overwrite는 차단하고 re2DJ host 갱신만 통과시킨다.
- 실제 host/guest는 per-monitor aware, DPI 144였지만 SDL 초기화 전 96-DPI frame 계산으로 outer가 `1296×999`여서 DWM title/icon 영역이 clip된 사실을 확인했다.
- SDL video 초기화 뒤 `GetDpiForWindow`와 `AdjustWindowRectExForDpi`로 window mode를 다시 적용해 outer `1302×1016`, host client 1280×960, guest client 1280×960을 확인했다.
- DPI 보정 캡처 `build/window-caption-dwm-post-sdl-dpi.png`에서 DWM 기본 icon, 전체 상태 제목과 Windows 11 frame이 한 번에 표시됐다. 캡처는 ignored build 산출물이다.
- guest/SDL close를 먼저 호출하는 실험은 termination 정체를 재현했다. 최종 host close는 guest를 파괴하지 않고 current-process `TerminateProcess`를 즉시 호출하며 실제 `ez2dj.exe`와 loader가 2초 안에 사라지고 exit code 0을 반환했다.
- 실험 중 종료 정체된 Debug DLL은 동일 build 디렉터리의 `*.locked-<pid>.dll` 진단 산출물로 옮겨 새 linker output과 분리했다. 저장소에는 포함되지 않는다.

### 검증

- 실제 제품: DWM enabled, icon/title/FPS 표시, 1280×960 host/guest client 확인
- 실제 제품: host `WM_CLOSE` 뒤 original process와 loader 종료 및 exit code 0 확인
- `cmake --build build\windows-x86 --config Debug`: 통과
- `ctest --test-dir build\windows-x86 -C Debug --output-on-failure`: 3/3 통과
- `cmake --build build\windows-x86 --config Release`: 통과
- `ctest --test-dir build\windows-x86 -C Release --output-on-failure`: 3/3 통과

### 남은 확인

사용자 환경에서 DWM 제목줄 layout과 resize/fullscreen 상호작용을 최종 확인한다.

## English

### Result

- Squash-merged Task 091 into `main`, creating version `0.0.15`, commit `4da5272`, local annotated tag `v0.0.15`, and deleting the old task branch.
- Windowed mode now keeps `DWMNCRP_ENABLED`.
- Explicit DWM enablement, post-SDL WndProc reordering, `DwmDefWindowProc`, normal `SetWindowText`, and DWM color correction alone did not restore the empty caption.
- Added a standard top-level host shell and placed the original HWND as its 1280x960 client child. SDL3/OpenGL continues using the original HWND as the external rendering target.
- The host shell owns title/icon, resize/fullscreen, close, and lifetime monitoring. It blocks ordinary `WM_SETTEXT`/`WM_SETICON` overwrites while allowing re2DJ host updates.
- The actual host and guest were per-monitor aware at DPI 144, but the pre-SDL 96-DPI frame calculation produced a `1296x999` outer size and clipped DWM title/icon content.
- Reapplying window mode after SDL video initialization with `GetDpiForWindow` and `AdjustWindowRectExForDpi` produced a `1302x1016` outer, 1280x960 host client, and 1280x960 guest client.
- DPI-corrected capture `build/window-caption-dwm-post-sdl-dpi.png` shows the default DWM icon, complete status title, and Windows 11 frame together. The capture is an ignored build artifact.
- Closing the guest/SDL path first reproduced termination stalling. Final host close leaves the guest intact and immediately invokes current-process `TerminateProcess`; the real `ez2dj.exe` and loader disappeared within two seconds with exit code zero.
- Debug DLLs locked by intentionally stalled experiments were moved to `*.locked-<pid>.dll` diagnostic artifacts in the same build directory, separating them from new linker output. They are not tracked.

### Verification

- Actual product: confirmed DWM enabled, icon/title/FPS, and 1280x960 host/guest clients
- Actual product: confirmed original process and loader exit with code zero after host `WM_CLOSE`
- `cmake --build build\windows-x86 --config Debug`: passed
- `ctest --test-dir build\windows-x86 -C Debug --output-on-failure`: 3/3 passed
- `cmake --build build\windows-x86 --config Release`: passed
- `ctest --test-dir build\windows-x86 -C Release --output-on-failure`: 3/3 passed

### Remaining validation

Final user confirmation remains for DWM title-bar layout and resize/fullscreen interaction in the user's environment.
