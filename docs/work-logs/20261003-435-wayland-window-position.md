# 작업 435 작업 로그 — Wayland에서 창 가운데 정렬 / Task 435 work log — centring the window on Wayland

설계: [20261003-435-wayland-window-position.md](../design/20261003-435-wayland-window-position.md) · 지시서: [20261003-435-wayland-window-position.md](../work-orders/20261003-435-wayland-window-position.md)

## 2026-10-03

- **증상**(Ubuntu 26.04, Wayland 세션, Linux x64 Debug): 6th 자식이 257번째 호출 `IDirectDraw7::SetCooperativeLevel`에서 "cannot resize the SDL3 window: wayland cannot position non-popup windows"로 멈추고, launcher가 `ExitProcess(0)`으로 끝났다(실행 `20261003-005716-844`, `20261003-005717-667`).
  *Symptom (Ubuntu 26.04, Wayland session, Linux x64 Debug): the 6th child stopped at call 257, `IDirectDraw7::SetCooperativeLevel`, with "cannot resize the SDL3 window: wayland cannot position non-popup windows", and the launcher ended with `ExitProcess(0)` (runs `20261003-005716-844`, `20261003-005717-667`).*
- **수정**: `Sdl3OpenGlBackend::ResizeWindow`가 `SDL_SetWindowPosition`의 실패를 무시한다. 크기 변경 실패만 실패로 돌려준다.
  *Fix: `Sdl3OpenGlBackend::ResizeWindow` ignores a failed `SDL_SetWindowPosition` and fails only on a failed resize.*
- **검증**
  - `scripts/test_all.sh linux-x64-debug`(경고를 오류로): build 성공, CTest 4개 통과. 이 build는 작업 434의 코드도 포함하므로, 작업 434의 Linux x64 build와 테스트도 이 머신에서 다시 확인한 것이다.
  - 같은 Wayland 세션에서 `re2dj --hdd roms/ez2dj6th --target ez2dj6th --run`을 30초 시간 제한으로 실행(`20261003-010716-770`, `20261003-010717-600`): 6th 자식이 `SetCooperativeLevel`을 지나 창을 열고, 시간 제한까지 `IDirectDrawSurface7::Flip`을 반복했다(421,689호출, Hardlock transform 2회). 시간 제한의 종료로 창이 닫히고 launcher가 0으로 끝났다.
  - 화면 내용과 조작은 사용자 확인으로 남긴다.

  *Verification:*
  - *`scripts/test_all.sh linux-x64-debug` (warnings as errors): the build succeeds and 4 CTest tests pass. The build includes task 434's code, so task 434's Linux x64 build and tests were rechecked on this machine as well.*
  - *On the same Wayland session, `re2dj --hdd roms/ez2dj6th --target ez2dj6th --run` with a 30-second timeout (`20261003-010716-770`, `20261003-010717-600`): the 6th child passed `SetCooperativeLevel`, opened its window and repeated `IDirectDrawSurface7::Flip` until the timeout (421,689 calls, two Hardlock transforms). The timeout's termination closed the window and the launcher ended with 0.*
  - *The screen contents and interaction are left to the user.*
