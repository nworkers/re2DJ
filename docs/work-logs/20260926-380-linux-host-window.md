# 작업 380 작업 로그 — Linux 호스트 창 / Task 380 work log — Linux host window

설계: [20260926-380-linux-host-window.md](../design/20260926-380-linux-host-window.md)
작업 지시서: [20260926-380-linux-host-window.md](../work-orders/20260926-380-linux-host-window.md)

## 진행 / Progress

WSLg가 있음을 확인했다. blend probe는 두 폭 모두 통과했다. 빌드 디렉터리에 남아 있던 작업 379의 옛 실행 파일(IPC probe, rejection helper, `build/linux-x86-helper`)도 지웠다. 구현한 뒤 `--hold-window`로 실행하니 창이 떴다.

*WSLg was confirmed and the blend probe passed on both widths; stale binaries from Task 379 (the IPC probe, the rejection helper, `build/linux-x86-helper`) were removed from the build directories. After implementation, a run with `--hold-window` opened the window.*

창 확인에서 실수가 있었다. 처음에는 Windows 전체 화면을 캡처했다. 이어서 창 좌표로 화면을 잘랐는데, 창이 맨 앞에 오지 않아 다른 프로그램이 찍혔다. 두 이미지 모두 즉시 지웠다. 이후에는 제목으로 re2DJ 창만 찾아 `PrintWindow`로 그 창의 내용만 받았다. 화면 픽셀은 복사하지 않았다. 창 확인에는 이 방식만 쓴다.

*Checking the window went wrong at first: a full-desktop capture, then a screen crop at the window's rectangle that caught another program because the window was not in front. Both images were deleted at once. From then on only the re2DJ window, found by title, was captured through `PrintWindow`, which takes the window's own content without copying screen pixels; window checks use only this.*

WSLg의 Windows 쪽 창은 외부에서 보낸 `WM_CLOSE`를 무시했다. WSL 안에도 xdotool 같은 도구가 없었다. 그래서 닫기 버튼으로 hold가 풀리는지는 자동으로 확인하지 못했다. 사용자 확인 항목으로 남긴다. 확인용 실행은 `timeout`으로 끝냈다.

*WSLg's Windows-side window ignored an externally posted `WM_CLOSE`, and WSL has no tool such as xdotool, so releasing the hold with the close button could not be checked automatically; it is left for the user to confirm. The check runs ended through `timeout`.*

## 변경 / Changes

- **HLE**: `include/re2dj/hle/host_presentation.h`, `ImportCallServices::Presentation()`, 기록 장식자 전달, ddraw `SetCooperativeLevel`의 호스트 창 표시와 실패 시 정지. / *`include/re2dj/hle/host_presentation.h`, `ImportCallServices::Presentation()`, forwarding in the recording decorator, and ddraw `SetCooperativeLevel` showing the host window and stopping when it cannot.*
- **`re2dj/version.h`**: `WindowTitle(version, fps)`. Windows `window_mode.cpp`가 이것을 쓴다. / *`WindowTitle(version, fps)`, used by Windows `window_mode.cpp`.*
- **Linux**: `LinuxHostPresentation`, `NativeKernel32Diagnostic::SetPresentation/Presentation`, `OriginalRunEnvironment::presentation`. / *`LinuxHostPresentation`, `NativeKernel32Diagnostic::SetPresentation/Presentation`, and `OriginalRunEnvironment::presentation`.*
- **CLI**: `--hold-window`, 창의 process 수명 보관, `LinuxWindowHold`. / *`--hold-window`, keeping the window for the process's life, and `LinuxWindowHold`.*
- **CMake**: Linux backend가 SDL backend와 SDL3를 link한다. / *the Linux backend links the SDL backend and SDL3.*
- **단위 테스트**: 가짜 presentation으로 창 표시 요청(창 handle, 640×480), 모르는 창, 호스트 실패 시 정지를 검사한다. `WindowTitle` 형식도 검사한다. / ***Unit tests:** a fake presentation checks the show request (window handle, 640×480), an unknown window, and the stop on a host failure; plus the `WindowTitle` format.*

## 검증 / Validation

| 항목 / Item | 결과 / Result |
| --- | --- |
| x64 창 / window | 제목 `re2DJ v0.0.52 (Linux/x64 Debug) - Build Sep 26 2026 - SDL3 OpenGL - FPS : 0.0 (Ubuntu-24.04)`, 검은 640×480 클라이언트(`PrintWindow`로 확인), `--hold-window` 동안 유지 / title as shown, a black 640×480 client (checked through `PrintWindow`), kept while `--hold-window` holds |
| x86 창 / window | 제목 `re2DJ v0.0.52 (Linux/x86 Debug) - … (Ubuntu-24.04)`인 창이 열림(제목만 확인) / a window titled as shown opens (title checked only) |
| 닫기로 hold 해제 / close releases the hold | 자동 확인 못 함(WSLg가 외부 `WM_CLOSE`를 무시). 사용자 확인 필요 / not checked automatically (WSLg ignores an external `WM_CLOSE`); for the user to confirm |
| Linux x64·x86 build, CTest | 경고·오류 없음, 각각 3/3 / no warnings or errors, 3/3 each |
| Linux probe·진단 네 개 / probes and four diagnostics | 전과 같음 / as before |
| 실제 4th, Linux 두 폭 / real 4th, both widths | 호출 1,824번, `SetCooperativeLevel`이 창을 띄우고 DD_OK, `CreateSurface`에서 정지, 두 폭 같음 / 1,824 calls; `SetCooperativeLevel` opens the window and returns DD_OK, then a stop at `CreateSurface`, identical on both widths |
| Windows x86 build, CTest | exit 0, 우리 코드 경고·오류 없음, 6/6. VFS runtime probe가 실제 창 제목의 머리말을 확인 / exit 0, no warnings or errors from this project, 6/6; the VFS runtime probe checks the banner in the real window title |
