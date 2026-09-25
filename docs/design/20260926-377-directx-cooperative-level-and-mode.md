# 작업 377 설계 — DirectX core 2단계: 협조 수준과 표시 모드 / Task 377 design — DirectX core phase 2: cooperative level and display mode

선행: [작업 374 설계](20260926-374-shared-directx-core.md)

## 배경 / Background

작업 374 뒤 Linux 실행은 `IDirectDraw7::SetCooperativeLevel(hwnd, 0x813)`에서 멈췄다. Windows facade의 규칙은 다음과 같다. `SetCooperativeLevel`은 창이 없으면 `DDERR_INVALIDPARAMS`다. 창이 있으면 Win32 창 모드 정책(`ApplyRe2djWindowMode`)을 적용하고, 실패하면 `DDERR_GENERIC`이다. `SetDisplayMode`는 640×480×16만 받고, 다른 모드는 `DDERR_UNSUPPORTEDMODE`다.

*After Task 374 the Linux run stopped at `IDirectDraw7::SetCooperativeLevel(hwnd, 0x813)`. The Windows facade's rules: `SetCooperativeLevel` is `DDERR_INVALIDPARAMS` without a window, otherwise applies the Win32 window-mode policy (`ApplyRe2djWindowMode`), with `DDERR_GENERIC` if that fails; `SetDisplayMode` accepts only 640×480×16 and answers other modes with `DDERR_UNSUPPORTEDMODE`.*

## 결정 / Decisions

1. **core (`directdraw_display.h`).** `DirectDrawDisplay`는 guest HWND, 협조 flags, 표시 모드를 담는다. `SetCooperativeLevel`, `IsSupportedDisplayMode`, `SetDisplayMode`가 위 규칙을 가진다. 호스트가 자기 창에 무엇을 하는지는 `HostWindowPolicy` callback으로 받는다. 정책이 실패하면 상태는 그대로다. `kCurrentDisplayMode`는 뜻에 맞게 `kDefaultDisplayMode`(처음 모드이자 유일하게 받는 모드)로 이름을 바꾼다. `DDERR_GENERIC`, `DDERR_UNSUPPORTEDMODE`를 ABI 상수에 더하고, Windows에서 SDK 값과 비교한다.
   ***Core (`directdraw_display.h`):** `DirectDrawDisplay` holds the guest HWND, the cooperative flags, and the display mode; `SetCooperativeLevel`, `IsSupportedDisplayMode`, and `SetDisplayMode` hold the rules above, with the host's own window work passed in as a `HostWindowPolicy` callback whose failure leaves the state unchanged. `kCurrentDisplayMode` is renamed `kDefaultDisplayMode` (the initial and only accepted mode) to say what it is. `DDERR_GENERIC` and `DDERR_UNSUPPORTEDMODE` join the ABI constants, checked against the SDK on Windows.*
2. **Windows.** `RootFacade`의 `width`/`height`/`bits_per_pixel`을 `DirectDrawDisplay display`로 바꾼다. Win32 호출에 쓰는 typed `HWND window`는 그대로 둔다. `SetCooperativeLevel`은 `ApplyRe2djWindowMode`를 정책으로 넘기고, 성공하면 전처럼 FPS 측정을 초기화한다. `IDirectDraw7::GetDisplayMode`는 `LegacyRootDisplay`로 root의 모드를 읽는다. 받는 모드가 하나뿐이므로 결과는 전과 같다.
   ***Windows:** `RootFacade`'s `width`/`height`/`bits_per_pixel` become `DirectDrawDisplay display`, keeping the typed `HWND window` for Win32 calls; `SetCooperativeLevel` passes `ApplyRe2djWindowMode` as the policy and resets the FPS measurement on success as before; `IDirectDraw7::GetDisplayMode` reads the root's mode through `LegacyRootDisplay`, which gives the same result as before since only one mode is accepted.*
3. **Linux.** `GuestComObject`에 module별 상태 slot(`GuestComState`, `StateAs<T>()`)을 더한다. DirectDraw 객체는 `DirectDrawState`(display)를 가진다. `SetCooperativeLevel`의 호스트 정책은 guest 창이 있는지만 본다. Linux에는 아직 표시 창이 없기 때문이다. `SetDisplayMode`와 `GetDisplayMode`는 객체의 display를 쓴다.
   ***Linux:** `GuestComObject` gains a per-module state slot (`GuestComState`, `StateAs<T>()`), and a DirectDraw object carries a `DirectDrawState` (its display). `SetCooperativeLevel`'s host policy only requires the guest window to exist, as Linux has no presentation window yet; `SetDisplayMode` and `GetDisplayMode` use the object's display.*
4. **그 밖의 Linux 경계.** 게임이 모드를 정하기 전에 부르는 `user32!SetRect`를 측정값대로 구현한다. NULL이면 FALSE이고, 뒤집힌 좌표도 그대로 저장하며, last error는 바뀌지 않는다.
   ***Other Linux boundaries:** `user32!SetRect`, which the game calls before setting the mode, as measured: FALSE for NULL, reversed coordinates stored unchanged, and the last error untouched.*

## 범위 밖 / Out of scope

- Windows 정책은 guest 창의 style을 바꾼다(`0x04cf0000` → `0x14cf0000` 등). Linux 창 정책과 그에 따른 guest가 보는 style 차이는 5단계(Linux 표시)에서 정한다. / *The Windows policy restyles the guest window (`0x04cf0000` → `0x14cf0000` and so on); Linux's window policy, and the style difference the guest sees, are settled in phase 5 (Linux presentation).*
- 3단계, 표면(`CreateSurface`). / *Phase 3, surfaces (`CreateSurface`).*
