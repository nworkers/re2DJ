# #15 설계: ez2dj3rd의 창 모드 DirectDraw (in-process 러너)

이슈: [#15](https://github.com/reexec/re2DJ/issues/15) · 이전: [설계 217](20260906-217-ez2dj3rd-windowed-primary-blt-present.md)(주입 방식의 창 모드 primary Blt)

## 배경

3rd 덤프의 `EZ2DJ.INI`는 `"FullScreen" = 0`이라, 게임은 `SetCooperativeLevel(hwnd, DDSCL_NORMAL | DDSCL_FPUSETUP)`로 창 모드 DirectDraw를 씁니다. 백 버퍼 없는 primary와 오프스크린 렌더 타깃을 만들고, 매 프레임 `GetClientRect`·`ClientToScreen`으로 얻은 사각형에 오프스크린을 primary로 `Blt`합니다. 주입 방식 시절에는 실제 Windows user32와 Windows facade가 이 경로를 처리했습니다(설계 216·217). in-process 러너의 facade에서는 primary로의 `Blt`가 이미 프레임을 표시하지만(`PresentFromBlit`), 아래가 빠져 있어 시작 1초 안에 멈췄습니다.

1. `user32!GetClientRect`, `user32!ClientToScreen`: resolve-only.
2. `IDirectDraw7::CreateClipper`, `IDirectDrawSurface7::SetClipper`·`GetClipper`: 미구현.

## 결정

* **user32**: `GetClientRect`는 창 모델의 클라이언트 영역을 원점 기준(`0, 0, 폭, 높이`)으로 돌려줍니다. 게스트가 요청한 640×480이며, 호스트 창의 배율이나 전체 화면과 무관합니다(호스트가 그림을 맞춰 그림). `ClientToScreen`은 `ScreenToClient`의 역입니다. 실패 규칙은 이웃 함수와 같게 두되, Windows 11에서 재지 않았다고 주석에 적습니다.
* **clipper**: 새 객체 종류 `kClipperObject`와 `IDirectDrawClipper` vtable(`ddraw_clipper.cpp`). `CreateClipper`(DX7·DX6 공용), `SetHWnd`·`GetHWnd`를 구현합니다. 호스트 창은 게스트의 유일한 창이고 겹치는 창이 없으므로 clip list는 모델링하지 않습니다(`GetClipList`·`SetClipList` 등은 부르면 멈추는 미구현으로 남김).
* **surface**: `SetClipper`는 clipper의 참조를 잡고 이전 것을 놓으며, 서피스가 사라질 때 함께 놓습니다(`HeldReferences`). `GetClipper`는 참조를 더해 돌려주고, 없으면 `DDERR_NOCLIPPERATTACHED`입니다.

## 검증

* 단위 테스트: `GetClientRect`·`ClientToScreen`(정상, 없는 창, null), clipper 생성·인자 오류·`SetHWnd`/`GetHWnd`, `SetClipper`/`GetClipper`와 참조 수, 분리 시 해제.
* 실제 실행: 3rd를 Linux x64·x86에서 실행해 로고·타이틀·모드 선택·곡 플레이와 창 닫기 종료를 확인합니다.

---

# #15 Design: ez2dj3rd's windowed DirectDraw (in-process runner)

Issue: [#15](https://github.com/reexec/re2DJ/issues/15) · Earlier: [design 217](20260906-217-ez2dj3rd-windowed-primary-blt-present.md) (the injection-era windowed primary Blt)

## Background

The 3rd dump's `EZ2DJ.INI` has `"FullScreen" = 0`, so the game uses windowed DirectDraw through `SetCooperativeLevel(hwnd, DDSCL_NORMAL | DDSCL_FPUSETUP)`: it makes a primary with no back buffer and an offscreen render target, and every frame blits the offscreen surface onto the primary at the rectangle `GetClientRect` and `ClientToScreen` give. In the injection days the real Windows user32 and the Windows facade served this path (designs 216 and 217). In the in-process runner's facade a blit onto the primary already presents the frame (`PresentFromBlit`), but the run stopped within a second for want of: (1) `user32!GetClientRect` and `user32!ClientToScreen`, resolve-only; (2) `IDirectDraw7::CreateClipper` and `IDirectDrawSurface7::SetClipper`/`GetClipper`, unimplemented.

## Decisions

* **user32**: `GetClientRect` gives the window model's client area from its own origin (`0, 0, width, height`), the 640×480 the guest asked for whatever the host window's scale or fullscreen (the host fits the picture). `ClientToScreen` is `ScreenToClient`'s inverse. Failures follow the neighbouring functions, with a comment that Windows 11 was not measured for these.
* **Clipper**: a new object kind `kClipperObject` and an `IDirectDrawClipper` vtable (`ddraw_clipper.cpp`), with `CreateClipper` (DX7 and DX6 alike), `SetHWnd` and `GetHWnd`. The host window is the guest's only window and nothing overlaps it, so clip lists are not modelled (`GetClipList`, `SetClipList` and the rest stay unimplemented and stop a run that calls them).
* **Surface**: `SetClipper` holds the clipper's reference and lets go of the previous one, releasing it with the surface (`HeldReferences`); `GetClipper` returns it with a new reference, or `DDERR_NOCLIPPERATTACHED`.

## Verification

* Unit tests: `GetClientRect` and `ClientToScreen` (success, unknown window, null), clipper creation, argument errors and `SetHWnd`/`GetHWnd`, and `SetClipper`/`GetClipper` with reference counts and release on detach.
* Real runs: 3rd on Linux x64 and x86 through the logo, title, mode select and a song, ending when the window is closed.
