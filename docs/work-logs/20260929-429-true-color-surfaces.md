# 작업 429 작업 로그 — 32비트 트루컬러 표면 선택 옵션 / Task 429 work log — an optional 32-bit true-color surface mode

설계: [20260929-429-true-color-surfaces.md](../design/20260929-429-true-color-surfaces.md) · 지시서: [20260929-429-true-color-surfaces.md](../work-orders/20260929-429-true-color-surfaces.md) · 분석: [graphics-color-depth.md](../analysis/graphics-color-depth.md)

## 2026-09-29

### 구현 / Implementation

- 공용 코어를 추가했다.
  - `graphics/color_depth.*`: `ColorDepth`, 이름 해석, 프로세스 스위치.
  - `graphics/true_color.*`: 넓힘·줄임, `TrueColorPlane`, 행·사각형 단위 복사·화해·채움.
  - `LegacyTextureView::true_color`.
- `Sdl3OpenGlBackend`를 바꿨다.
  - 렌더 타깃이 `GL_RGB565` ↔ `GL_RGB8`로 전환되며 내용을 옮긴다. `GL_RGB8`이 불완전하면 565로 남는다.
  - 32비트 렌더 타깃은 텍스처 색을 plane에서, 키를 565에서 가져온다.
  - 32비트 readback은 줄이고, write는 화해한다.
  - `ClearRenderTargetColor`, `render_target_depth()`, `true_color_unavailable()`을 더했다.
- Linux HLE를 바꿨다.
  - `GuestBitmap::true_color`와 GDI 네 경로(`StretchBlt`, `StretchDIBits`, `FillRect`, `DrawTextA`)의 동시 쓰기.
  - ddraw 표면의 plane 수명과 쓰기 경로(GetDC, Blt/BltFast, 색 채우기, Clear, Load, Unlock).
  - `HostPresentation::ClearTargetColor`.
  - `LinuxHostPresentation`에 OSD(백틱, 마우스, 정보 줄, "32-bit color")를 설치하고 색 깊이 변화를 로그에 남긴다.
- Windows DX6 facade를 바꿨다.
  - 32bpp DIB plane. 32비트 모드의 `GetDC`는 그 DIB를 DC에 선택하고, `ReleaseDC`는 `GdiFlush` 뒤 줄인다.
  - 복사·채움·Clear·Load·Unlock·텍스처 뷰를 plane에 맞췄다.
  - 대기 중 Clear가 32비트 색을 보존한다.
  - 그래픽 trace에 `color-depth:selected/render-target/rgb8-unavailable` 줄을 남긴다.
- 옵션을 연결했다: `TargetRunDefaults::color_depth`, 제품 `--color-depth 16|32`, launcher `--color-depth`, 주입 런타임 export `g_re2dj_color_depth`(첫 DirectDraw 객체에서 한 번 적용), Linux의 스위치 설정.
- OSD: 공용 `ui::AddColorDepthToggle`을 Windows `RegisterGameControls`와 Linux OSD가 쓴다.

*Implementation.*

- *Added the shared core:*
  - *`graphics/color_depth.*`: `ColorDepth`, name parsing, and the process switch;*
  - *`graphics/true_color.*`: widening and narrowing, `TrueColorPlane`, and row- and rectangle-level copy, reconcile, and fill;*
  - *`LegacyTextureView::true_color`.*
- *Changed `Sdl3OpenGlBackend`:*
  - *the render target switches between `GL_RGB565` and `GL_RGB8`, carrying its contents, and stays 565 when `GL_RGB8` is incomplete;*
  - *a 32-bit target takes texture colours from the plane and keys from 565;*
  - *32-bit readback narrows and writes reconcile;*
  - *added `ClearRenderTargetColor`, `render_target_depth()`, and `true_color_unavailable()`.*
- *Changed the Linux HLE:*
  - *`GuestBitmap::true_color` and dual writes on the four GDI paths (`StretchBlt`, `StretchDIBits`, `FillRect`, `DrawTextA`);*
  - *the ddraw surface plane's lifetime and write paths (GetDC, Blt/BltFast, colour fill, Clear, Load, Unlock);*
  - *`HostPresentation::ClearTargetColor`;*
  - *`LinuxHostPresentation` installs the OSD (backtick, mouse, information lines, "32-bit color") and logs colour-depth changes.*
- *Changed the Windows DX6 facade:*
  - *a 32bpp DIB plane; in 32-bit mode `GetDC` selects that DIB into the DC and `ReleaseDC` narrows it after `GdiFlush`;*
  - *copies, fills, Clear, Load, Unlock, and the texture view follow the plane;*
  - *a pending clear keeps its 32-bit colour;*
  - *the graphics trace records `color-depth:selected/render-target/rgb8-unavailable` lines.*
- *Wired the option: `TargetRunDefaults::color_depth`, the product's `--color-depth 16|32`, the launcher's `--color-depth`, the injected runtime export `g_re2dj_color_depth` (applied once at the first DirectDraw object), and the switch on Linux.*
- *The OSD: Windows `RegisterGameControls` and the Linux OSD both use the shared `ui::AddColorDepthToggle`.*

### 검증 / Verification

**빌드와 테스트 / Builds and tests** (경고는 오류로 처리 / warnings as errors)

| host | 결과 / Result |
|---|---|
| Windows x86 Debug | 빌드 성공. CTest 6개 통과. 단위 5,644 checks, 실패 0. `re2dj_opengl_true_color_probe` 13 checks, 실패 0. / *Build succeeds; all 6 CTest tests pass; 5,644 unit checks, 0 failures; `re2dj_opengl_true_color_probe` 13 checks, 0 failures.* |
| Linux x86 Debug (WSL) | 빌드 성공. 단위 5,641 checks, 실패 0. blend probe 10 checks, 실패 0. true-color probe 13 checks, 실패 0. / *Build succeeds; 5,641 unit checks, 0 failures; blend probe 10 checks, 0 failures; true-color probe 13 checks, 0 failures.* |
| Linux x64 Debug (WSL) | 빌드 성공. 단위 5,641 checks, 실패 0. blend probe 10 checks, 실패 0. true-color probe 13 checks, 실패 0. / *Build succeeds; 5,641 unit checks, 0 failures; blend probe 10 checks, 0 failures; true-color probe 13 checks, 0 failures.* |

- 단위 테스트는 두 부분이다. `true_color_test`는 65,536개 RGB565 값의 왕복, 화해, 키 있는 복사·같은 plane 안 복사, 사각형 연산, 옵션 이름·스위치·프로파일 기본값을 확인한다. `ddraw_module_test`의 `CheckTrueColorSurfaces`는 32비트 Clear, 24비트 DIB `StretchDIBits`, 텍스처 뷰의 plane, Lock/Unlock 화해, 키 있는 `BltFast`, 부분 색 채우기, 16비트에서 만든 표면의 지연 plane을 확인한다. `gdi_raster_test`에는 `kGdiXrgb8888` 변환을 더했다.
- **Windows blend probe의 기존 실패.** Windows x86 blend probe는 "white mask preserves background" 한 검사가 49,121,181로 실패한다(기대 64,128,192 ±8). backend 두 파일을 main 버전으로 되돌려 같은 probe를 빌드해도 값까지 똑같이 실패하므로, 이 작업의 회귀가 아니다. 2×1 마스크를 linear로 샘플링할 때 이 호스트 드라이버의 필터링 결과로 **추정**한다. 원인은 이 작업 범위 밖이다.
- **probe 출력.** Windows probe를 파이프로 실행하면 출력 버퍼가 비워지지 않은 채 끝나 아무것도 보이지 않았다. 두 probe의 `std::cout`을 unitbuf로 바꿨다.
- true-color probe는 처음에 "picture kept at 32 bits"에서 실패했다. 원인은 probe였다. 유지 모드가 꺼진 채 `Present` 뒤 새 프레임이 색을 지웠다. probe가 그 구간에서 유지 모드를 켜도록 고쳤다.

- *The unit tests come in two parts. `true_color_test` covers the round trip of all 65,536 RGB565 values, reconcile, keyed and same-plane copies, rectangle operations, and the option's names, switch, and profile defaults. `ddraw_module_test`'s `CheckTrueColorSurfaces` covers a 32-bit Clear, a 24-bit DIB through `StretchDIBits`, the plane in the texture view, Lock/Unlock reconcile, a keyed `BltFast`, a partial colour fill, and the deferred plane of a surface made in 16-bit mode. `gdi_raster_test` gains the `kGdiXrgb8888` conversions.*
- ***A pre-existing Windows blend-probe failure.** The Windows x86 blend probe fails one check, "white mask preserves background", with 49,121,181 (expected 64,128,192 ±8). Building the same probe against main's two backend files fails identically, value for value, so this task did not cause it. It is **inferred** to be this host driver's result for linearly sampling the 2×1 mask; the cause is outside this task.*
- ***Probe output.** Run through a pipe, the Windows probes printed nothing, because their output buffer was never flushed. Both probes now set `std::cout` to unitbuf.*
- *The true-color probe first failed "picture kept at 32 bits". The probe was at fault: with retention off, the frame after `Present` cleared the colour. The probe now enables retention for that step.*

**실제 실행 / Real runs**

이 작업 동안 Windows 데스크톱이 잠겨 있었다. 그래서 Windows 쪽 화면 캡처와 입력 주입이 불가능했다. Linux 화면은 WSLg의 X 서버에서 `XGetImage`로 직접 캡처했다(scratchpad의 `xgrab`, 저장소에 넣지 않음). 캡처는 scratchpad에만 둔다.

*The Windows desktop was locked throughout, so Windows-side screen capture and input injection were impossible. Linux frames were captured straight from WSLg's X server with `XGetImage` (`xgrab` in the scratchpad, not in the repository); captures stay in the scratchpad.*

- **Linux x86, 1st SE, `--color-depth 16`/`32`.** 로그에 `colour depth : 16-bit`/`32-bit`, `presentation: 16-bit colour`/`32-bit colour`가 남는다. 두 실행 모두 timeout으로 창이 닫힐 때까지 돌았다. 같은 AMUSE WORLD 로고 장면을 비교했다. 16비트에서는 금속 배경에 가로 띠와 색 얼룩이 보인다. 32비트에서는 부드러운 그라데이션이다. 16비트의 어두운 터널 장면에도 가로 띠가 보인다.
- **Linux x64, 1st SE, `--color-depth 32`.** 로그에 `presentation: 32-bit colour`가 남았다. 35초 캡처는 Linux x86 16비트 50초 캡처와 같은 프레임(로고와 왼쪽 위 플레어)이다. 왼쪽 아래 300×300 영역의 고유 색 수는 16비트 61개, 32비트 351개다. 가까운 다른 프레임(x86 32비트 35초, 플레어 없음)에서는 1,209개였다.
- **Linux x86, 4th, `--color-depth 32`.** 90초 동안 오류 없이 돌았고 `presentation: 32-bit colour`가 남았다. 창은 timeout으로 닫혔다.
- **Linux OSD.** XTest로 보낸 백틱에 OSD가 게임 화면 위로 떴다. 버전, `Target Profile : ez2dj1stse`, `Executable : Ez2DJ.exe`와 꺼진 "32-bit color"가 보였다. XTest 클릭은 체크박스를 바꾸지 못했다. `XQueryPointer`로 보면 X 포인터는 움직였으나, OSD의 hover 표시조차 없었다. 따라서 XTest 포인터 이벤트가 SDL에 닿지 않은 것으로 **추정**한다. **미확정:** 실제 마우스로 Linux OSD 토글을 눌러 실행 중에 전환되는지. 실행 중 전환 자체는 실제 GL에서 true-color probe가 확인했다(렌더 타깃 32→16→32 전환과 내용 유지).
- **Windows x86, 1st SE, `--color-depth 32`.** launcher 기록에 `"color_depth":32`가 있다. 그래픽 trace에는 `re2dj:hle:color-depth:selected=32`와 `re2dj:hle:color-depth:render-target=32`가 있고 실패 줄은 없다. 약 80초 뒤 프로세스를 종료했다. **미확정:** 잠금 때문에 Windows 화면과 Windows OSD 토글은 보지 못했다.

- ***Linux x86, 1st SE, `--color-depth 16`/`32`.** The log records `colour depth : 16-bit`/`32-bit` and `presentation: 16-bit colour`/`32-bit colour`; both runs lasted until the timeout closed the window. Compared on the same AMUSE WORLD logo scene, 16 bits shows horizontal bands and colour blotches across the metal background, and 32 bits a smooth gradient. The dark tunnel scene at 16 bits bands visibly as well.*
- ***Linux x64, 1st SE, `--color-depth 32`.** The log records `presentation: 32-bit colour`. The 35-second capture is the same frame (the logo with the top-left flare) as the Linux x86 16-bit 50-second one; the lower-left 300×300 area holds 61 distinct colours at 16 bits and 351 at 32 bits. A nearby different frame (x86 32-bit at 35 seconds, without the flare) held 1,209.*
- ***Linux x86, 4th, `--color-depth 32`.** Ran for 90 seconds without errors, recording `presentation: 32-bit colour`; the timeout closed the window.*
- ***The Linux OSD.** A backtick sent through XTest brought up the OSD over the game, with the version, `Target Profile : ez2dj1stse`, `Executable : Ez2DJ.exe`, and "32-bit color" unticked. XTest clicks did not change the checkbox: `XQueryPointer` showed the X pointer moving, but the OSD showed not even a hover, so XTest pointer events are **inferred** not to reach SDL. **Unresolved:** whether a real mouse on the Linux OSD toggle switches while running. The run-time switch itself was confirmed on real GL by the true-color probe (render target 32→16→32 with its contents kept).*
- ***Windows x86, 1st SE, `--color-depth 32`.** The launcher record shows `"color_depth":32`, and the graphics trace `re2dj:hle:color-depth:selected=32` and `re2dj:hle:color-depth:render-target=32`, with no failure lines. The process was ended after about 80 seconds. **Unresolved:** the lock kept the Windows picture and the Windows OSD toggle out of view.*

### 남은 일 / Follow-ups

- 잠기지 않은 데스크톱에서 두 host의 OSD 토글을 실제 마우스로 눌러 보고, Windows 32비트 화면을 캡처한다.
- Windows blend probe의 기존 실패 원인 조사.

- *On an unlocked desktop, press both hosts' OSD toggles with a real mouse and capture the Windows 32-bit picture.*
- *Investigate the pre-existing Windows blend-probe failure.*
