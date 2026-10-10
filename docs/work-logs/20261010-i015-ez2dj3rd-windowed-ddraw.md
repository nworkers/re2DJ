# #15 작업 로그 — ez2dj3rd의 창 모드 DirectDraw / #15 work log — ez2dj3rd's windowed DirectDraw

설계: [20261010-i015-ez2dj3rd-windowed-ddraw.md](../design/20261010-i015-ez2dj3rd-windowed-ddraw.md) · 지시서: [20261010-i015-ez2dj3rd-windowed-ddraw.md](../work-orders/20261010-i015-ez2dj3rd-windowed-ddraw.md)

## 2026-10-10

- **원인**: 3rd(`roms/ez2dj3rd/ez2dj3rd.chd`)는 시작 1초 안에 `user32.dll!GetClientRect`(resolve-only)에서 멈췄다. 변경 전 v0.0.68 Release에서도 같았다. 덤프의 `EZ2DJ.INI`가 `"FullScreen" = 0`이고, API 기록에서 `SetCooperativeLevel(hwnd, 0x808)` → `GetDisplayMode` → `GetClientRect` 순서를 확인했다.
- **1단계**: user32 `GetClientRect`(클라이언트 영역, 없는 창은 `ERROR_INVALID_WINDOW_HANDLE`, null 사각형은 `ERROR_NOACCESS`)와 `ClientToScreen`(`ScreenToClient`의 역)을 구현하고 resolve-only 목록에서 뺐다. 다음 실행은 `IDirectDraw7::CreateClipper`에서 멈췄다.
- **2단계**: `ddraw_clipper.cpp`(`kClipperObject`, `IDirectDrawClipper` 9개 메서드 중 `AddRef`·`Release`·`GetHWnd`·`SetHWnd` 구현), `CreateClipperOf`(DX7·DX6, `pUnkOuter`는 `CLASS_E_NOAGGREGATION`, null 출력은 `DDERR_INVALIDPARAMS`), 서피스 `SetClipper`·`GetClipper`(참조를 잡고 서피스와 함께 놓음, `DDERR_NOCLIPPERATTACHED`). `abi.h`에 `kDdErrNoClipperAttached`(0x887600CD)·`kClassENoAggregation`(0x80040110).
- **결과**: 그다음 실행에서 3rd가 멈추지 않았다. 매 프레임 오프스크린을 primary로 `Blt`(사각형 `0,0,640,480`)하며 기존 `PresentFromBlit`이 화면에 표시한다.

  *Cause: 3rd (`roms/ez2dj3rd/ez2dj3rd.chd`) stopped within a second at `user32.dll!GetClientRect` (resolve-only), as on the unchanged v0.0.68 Release; the dump's `EZ2DJ.INI` has `"FullScreen" = 0`, and the API record shows `SetCooperativeLevel(hwnd, 0x808)`, `GetDisplayMode`, `GetClientRect`. Step 1: implemented user32 `GetClientRect` (the client area; an unknown window `ERROR_INVALID_WINDOW_HANDLE`, a null rectangle `ERROR_NOACCESS`) and `ClientToScreen` (`ScreenToClient`'s inverse) and dropped them from the resolve-only list; the next run stopped at `IDirectDraw7::CreateClipper`. Step 2: `ddraw_clipper.cpp` (`kClipperObject`; of `IDirectDrawClipper`'s nine methods `AddRef`, `Release`, `GetHWnd` and `SetHWnd`), `CreateClipperOf` (DX7 and DX6; `pUnkOuter` gives `CLASS_E_NOAGGREGATION`, a null out pointer `DDERR_INVALIDPARAMS`), and surface `SetClipper` and `GetClipper` (holding the reference and releasing it with the surface; `DDERR_NOCLIPPERATTACHED`), with `kDdErrNoClipperAttached` (0x887600CD) and `kClassENoAggregation` (0x80040110) in `abi.h`. Result: the next run did not stop; every frame blits the offscreen surface onto the primary (rectangle `0,0,640,480`) and the existing `PresentFromBlit` shows it.*

## 검증 / Verification

- 단위 테스트: `user32_module_test`에 `GetClientRect`·`ClientToScreen`, `ddraw_module_test`에 `CheckClipper`와 메서드 수 316. x64·x86 Debug(경고를 오류로)와 clang: 빌드, CTest 각 5개 통과, checks 6,351, failures 0.
- 실제 실행: Linux x64 Debug에서 사용자가 3rd를 실행해 로고·타이틀·모드 선택을 지나 곡(`redocean`, 배경음 17.3MB를 927ms에 미리 읽음)을 플레이했고 잘 동작한다고 확인했다. 약 1분 45초 뒤 창 닫기로 `host window closed`, 경고·오류 없음. Linux x86 Debug(`egl-wayland` v1 지정) 40초 실행도 모드 선택까지 경고·오류 없이 진행했다.
- Windows x86은 push 뒤 CI로 확인한다.

  *Unit tests: `GetClientRect` and `ClientToScreen` in `user32_module_test`, and `CheckClipper` with a method count of 316 in `ddraw_module_test`; x64 and x86 Debug (warnings as errors) and clang build, pass 5 CTest tests each and report 6,351 checks with 0 failures. Real runs: on Linux x64 Debug the user ran 3rd through the logo, title and mode select and played a song (`redocean`, its 17.3 MB music prefetched in 927 ms), confirming it works; after about 1 min 45 s closing the window gave `host window closed` with no warning or error. A 40-second Linux x86 Debug run (`egl-wayland` v1 pinned) reached mode select with no warning or error. Windows x86 is checked by CI after the push.*
