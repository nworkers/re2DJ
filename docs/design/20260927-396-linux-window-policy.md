# 작업 396 설계 — Linux 창 크기와 단축키 / Task 396 design — the Linux window's size and shortcuts

선행: [작업 395 설계](20260927-395-run-until-closed.md)

## 배경 / Background

사용자가 Linux 창의 기본 크기와 크기 변경 단축키를 Windows 제품과 같게 맞춰 달라고 요청했다. Linux 창은 게임 해상도 그대로(640×480) 열렸고, 크기를 바꿀 수 없었으며, 제목의 FPS도 0.0에 머물렀다. Windows 제품(`window_mode.cpp`, `host_window_shell.cpp`)의 정책은 다음과 같다.

| 항목 / Item | Windows 제품 / Windows product |
| --- | --- |
| 기본 크기 / default size | 게임 해상도의 2배, 작업 영역 가운데 / *twice the display, centred in the work area* |
| 창 형태 / frame | `WS_OVERLAPPEDWINDOW`, 크기 조절 가능 / *resizable* |
| Alt+1/2/3(숫자 줄·숫자패드) / *(top row or keypad)* | 배율 1·2·3, 키 반복 무시 / *scale 1, 2, 3; auto-repeat ignored* |
| 왼쪽 더블클릭 / left double click | 모니터 크기 테두리 없는 전체 화면 전환 / *toggle monitor-sized borderless fullscreen* |
| 전체 화면 중 배율 / scale while fullscreen | 저장만 하고, 창으로 돌아올 때 적용 / *stored, applied on returning to the window* |
| `--fullscreen` / `--windowed` | 시작 모드. 기본은 profile 값 / *starting mode; the profile's default otherwise* |
| 제목 / title | 1초 이상 모은 flip으로 계산한 FPS / *FPS over at least a second of presents* |

*The user asked for the Linux window's default size and size shortcuts to match the Windows product. The Linux window opened at the display's own size (640×480), could not be resized, and its title stayed at FPS 0.0. The Windows product's policy is tabulated above.*

## 결정 / Decisions

1. **공용 정책.** `re2dj/graphics/window_policy.h`에 정책을 둔다. 내용은 배율 상수(기본 2, 범위 1~3), `IsWindowScale`, 제목 FPS용 `FrameRateMeter`다. Windows `window_mode.cpp`는 기본 배율과 범위 판정에 이 상수를 쓴다. Windows의 FPS 계산은 present 간격 통계와 얽혀 있어 이번에는 그대로 둔다.
   ***Shared policy.** `re2dj/graphics/window_policy.h` holds the scale constants (default 2, range 1 to 3), `IsWindowScale`, and `FrameRateMeter` for the title's FPS. Windows' `window_mode.cpp` uses the constants for its default scale and range check. Windows' own FPS computation, tied to its present interval statistics, is left as it is for now.*
2. **backend 창 조작.** `Sdl3OpenGlBackend`에 다음을 더한다. 모두 backend가 직접 만든 창에만 쓴다. Windows는 자기 HWND를 감싸므로 해당하지 않는다.
   - 생성 옵션 `resizable`, `centered`. 구조체 끝에 두어, 필드를 순서대로 나열해 초기화하는 Windows facade가 깨지지 않게 한다.
   - `ResizeWindow`: 크기를 바꾸고 다시 가운데로 옮긴다. 가운데 정렬은 요청이며, Wayland처럼 window system이 거절해도 실패로 보지 않는다([작업 435](20261003-435-wayland-window-position.md)).
   - `SetFullscreen`: SDL3의 desktop fullscreen을 쓴다. 테두리 없이 데스크톱 해상도 그대로다.
   - `SetTitle`.

   ***Backend window controls.** `Sdl3OpenGlBackend` gains the following, for a window the backend makes itself; Windows wraps its own HWND and is unaffected:*
   - *The creation options `resizable` and `centered`, placed last so the Windows facade, which lists the fields in order, keeps its meaning.*
   - *`ResizeWindow`, which resizes and centres the window again; centring is a request, and a window system refusing it, as Wayland does, is not a failure ([task 435](20261003-435-wayland-window-position.md)).*
   - *`SetFullscreen`, using SDL3's desktop fullscreen: borderless, at the desktop's resolution.*
   - *`SetTitle`.*
3. **Linux 창.** `LinuxHostPresentation`이 정책을 적용한다.
   - 렌더 타깃은 게임 해상도로 만들고, 창은 열자마자 배율 크기로 키운다.
   - 이벤트 관찰자(작업 395)로 들어온 Alt+1/2/3을 처리한다. 키 반복이나 Alt 없는 키는 무시한다.
   - 왼쪽 버튼의 짝수 번째 클릭(SDL의 `clicks`)을 더블클릭으로 본다. Windows가 `WM_LBUTTONDBLCLK`를 주는 시점과 같다.
   - 창이 받아들이지 못한 모드는 되돌린다. Windows와 같다.
   - flip마다 FPS를 재어 제목을 갱신한다.
   - `--fullscreen`/`--windowed`는 이제 Linux에서도 받는다.

   ***The Linux window.** `LinuxHostPresentation` applies the policy:*
   - *The render target has the display's size, and the window grows to the scaled size as soon as it opens.*
   - *Alt+1/2/3 arrive through the event observer (Task 395); auto-repeats and keys without Alt are ignored.*
   - *Every second click of the left button (SDL's `clicks`) is a double click, where Windows sends `WM_LBUTTONDBLCLK`.*
   - *A mode the window will not take is undone, as on Windows.*
   - *Each flip feeds the FPS in the title.*
   - *`--fullscreen`/`--windowed` are now accepted on Linux too.*
4. **크기 단위.** Linux 창 크기는 SDL의 창 좌표다. WSLg에서는 픽셀과 1:1이다. 배율이 있는 Linux 데스크톱에서는 SDL 규칙을 따르고, 이 경우는 확인하지 않았다(미확정).
   ***Units.** The Linux window's size is in SDL window coordinates, one to one with pixels under WSLg. On a scaled Linux desktop it follows SDL's rules; that case is unverified.*

## 범위 밖 / Out of scope

- Windows의 OSD(`HandleOsdWindowMessage`)와 커서 표시 규칙. / *Windows' OSD (`HandleOsdWindowMessage`) and cursor rules.*
- 더블클릭과 Alt 조합을 guest 입력에서 빼는 일. Linux host 입력은 아직 연결되지 않았다. / *Keeping the double click and Alt combinations out of guest input; Linux host input is not connected yet.*
