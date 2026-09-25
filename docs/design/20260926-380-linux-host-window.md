# 작업 380 설계 — Linux 호스트 창 / Task 380 design — Linux host window

선행: [작업 377 설계](20260926-377-directx-cooperative-level-and-mode.md), [작업 374 설계](20260926-374-shared-directx-core.md)

## 배경 / Background

Linux 실행은 게임 창을 `GuestUser` 안의 모델로만 만들고 호스트 화면에는 아무것도 띄우지 않았다. 계획상 실제 창은 DirectX core 5단계였다. 사용자가 진행 단계를 눈으로 보려고 창 생성을 먼저 하기로 했다(2026-09-26). WSL에는 WSLg(DISPLAY `:0`, Wayland)가 있다. 공용 SDL3/OpenGL backend의 `re2dj_opengl_blend_probe`는 두 폭 모두 통과한다(10/10).

*The Linux run modelled the game's window only inside `GuestUser` and showed nothing on the host; a real window was planned for DirectX core phase 5. The user chose (2026-09-26) to bring window creation forward to see progress. WSL has WSLg (DISPLAY `:0`, Wayland), and the shared SDL3/OpenGL backend's `re2dj_opengl_blend_probe` passes on both widths (10/10).*

## 결정 / Decisions

1. **HLE 인터페이스.** `hle::HostPresentation::ShowGuestWindow(guest_window, width, height, error)`를 둔다. 서비스는 `ImportCallServices::Presentation()`으로 얻고, 기본값은 null(표시하지 않는 host)이다. 기록 장식자도 이것을 넘긴다.
   ***HLE interface:** `hle::HostPresentation::ShowGuestWindow(guest_window, width, height, error)`, reached through `ImportCallServices::Presentation()` (null by default, for a host that shows nothing) and forwarded by the recording decorator.*
2. **ddraw `SetCooperativeLevel`.** 호스트 정책은 guest 창이 있어야 통과한다. presentation이 있으면 표시 모드 크기로 `ShowGuestWindow`를 부른다. 호스트가 창을 띄우지 못하면 guest에게 `DDERR_GENERIC`을 주지 않고 handler 실패로 멈춘다. 게스트 탓이 아닌 호스트의 실패이기 때문이다.
   ***ddraw `SetCooperativeLevel`:** the host policy needs the guest window to exist, then calls `ShowGuestWindow` at the display mode's size when there is a presentation; a host that cannot show the window stops the handler rather than tell the guest `DDERR_GENERIC` for a failure of the host's own.*
3. **Linux 구현.** `LinuxHostPresentation`(`include/re2dj/platform/linux/host_presentation.h`)은 첫 호출에서 공용 `Sdl3OpenGlBackend`로 창을 만든다. render target을 0으로 지우고 한 번 표시한다. 아직 그리는 것이 없으므로 검은 화면이다. guest 창 하나만 대표한다. `HoldUntilClosed()`는 SDL event를 직접 기다린다. `Present`는 event queue를 비우므로 닫기 요청을 삼킬 수 있기 때문이다.
   ***Linux implementation:** `LinuxHostPresentation` (`include/re2dj/platform/linux/host_presentation.h`) makes the window with the shared `Sdl3OpenGlBackend` on the first call, clears the render target to 0, and presents once — black, as nothing is drawn yet — standing for one guest window. `HoldUntilClosed()` waits on SDL events itself, since `Present` drains the queue and could swallow the close request.*
4. **창 제목.** 모든 host가 쓰는 header-only `WindowTitle(version, fps)`를 `re2dj/version.h`에 둔다. 형식은 `re2DJ v0.0.52 (Linux/x64 Debug) - Build <date> - SDL3 OpenGL - FPS : 0.0`이다. Windows `Re2djUpdateWindowTitle`도 이 함수를 쓴다. WSLg는 Windows 쪽 창 제목 끝에 `(Ubuntu-24.04)`를 붙인다.
   ***Window title:** the header-only `WindowTitle(version, fps)` in `re2dj/version.h`, `re2DJ v0.0.52 (Linux/x64 Debug) - Build <date> - SDL3 OpenGL - FPS : 0.0`, used by every host including Windows's `Re2djUpdateWindowTitle`. WSLg appends `(Ubuntu-24.04)` to the title on the Windows side.*
5. **수명과 `--hold-window`.** 창은 CLI가 process 수명으로 가진다. `OriginalRunEnvironment::presentation`으로 진단에 넘긴다. `--hold-window`를 주면 `main`이 결과를 모두 출력한 뒤, 어느 경로로 반환하든 사용자가 창을 닫을 때까지 창을 유지한다. 이 옵션이 없는 실행(자동 회귀 포함)은 창을 잠깐 띄웠다가 끝난다.
   ***Lifetime and `--hold-window`:** the CLI owns the window for the process's life and passes it to the diagnostic through `OriginalRunEnvironment::presentation`; with `--hold-window`, after `main` has reported everything, whichever way it returns, the window stays until the user closes it. Runs without the option, automated regressions included, show the window briefly and end.*
6. **CMake.** `re2dj_linux_native_backend`가 `re2dj_sdl3_opengl_backend`와 `SDL3::SDL3-static`을 link한다.
   ***CMake:** `re2dj_linux_native_backend` links `re2dj_sdl3_opengl_backend` and `SDL3::SDL3-static`.*

## 범위 밖 / Out of scope

- guest 창 style(Windows 정책은 `0x14cf0000`으로 바꿈), 창 크기 배율, 전체 화면, 입력·포커스와 message loop 연결. / *The guest window's style (the Windows policy changes it to `0x14cf0000`), window scaling, full screen, and wiring input, focus, and the message loop.*
- 그리기와 FPS 갱신(DirectX core 3–5단계). / *Drawing and FPS updates (DirectX core phases 3–5).*
