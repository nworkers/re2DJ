# ez2d2m / ez2dj4th 그래픽 clear 경로 분석

## 조사 범위

이 문서는 `ez2d2m`과 `ez2dj4th` 실행 중 수집한 DirectDraw/Direct3D trace와 현재 HLE 코드를 비교해, 화면 잔상처럼 보이는 출력의 원인을 기록한다. 원본 실행 파일과 원본 자산은 저장하지 않는다.

## 확인됨

### 공통 presentation 방식

두 실행의 primary surface 생성 trace는 다음 descriptor를 기록한다.

```text
caps=0x00002218:back_buffers=1
```

이는 `DDSCAPS_PRIMARYSURFACE`, `DDSCAPS_FLIP`, `DDSCAPS_COMPLEX`, `DDSCAPS_VIDEOMEMORY`가 포함된 flip chain 요청이다. 현재 facade는 이 descriptor를 읽어 `RootFacade::presentation_retains_frames`를 true로 설정한다. 따라서 profile에 별도의 retained-frame 항목이 없어도 두 실행은 동일한 공통 경로를 선택한다.

### 명시적 clear 호출

- `ez2d2m`: `LegacyDeviceClear` trace에서 `flags=0x00000001`(`D3DCLEAR_TARGET`)과 검정에서 점차 밝아지는 `D3DCOLOR`를 전달한다.
- `ez2dj4th`: `LegacyDeviceClear` trace에서 `flags=0x00000003`(target와 depth) 및 검정색 `D3DCOLOR`를 전달한다.
- 두 실행 모두 clear 호출이 첫 draw보다 먼저 발생한다.
- 기존 `LegacyDeviceClear` 구현은 trace만 남기고 logical RGB565 render target에는 아무 작업도 하지 않았다.

### HLE 원인

retained-frame backend는 flip chain의 부분 갱신을 보존하기 위해 `Draw`의 프레임 시작에서 color buffer를 암묵적으로 지우지 않는다. 그러므로 guest의 explicit clear를 backend에 전달하지 않으면 이전 색상 내용이 다음 프레임에 남는다. screenshot에서 보이는 수평 streak와 잔상은 이 상태와 일치한다.

`ez2d2m`과 `ez2dj4th` profile은 모두 `hle_d3d3=true`이지만, graphics retention을 선택하는 별도 profile 값은 없다. 따라서 이번 원인은 profile 누락이 아니라 공통 D3D facade의 clear forward 누락으로 분류한다.

## 수정됨

Task 257에서 다음 경로를 추가했다.

- `D3DCOLOR`를 RGB565로 변환한다.
- 현재 device render target이 presentation surface인 전체 대상 `D3DCLEAR_TARGET`을 backend `ClearRenderTarget`으로 전달한다.
- 첫 draw 전에 backend가 아직 없으면 마지막 clear 색상을 root에 보관하고 첫 draw 직전에 적용한다.
- 초기화 전 전체 화면 `DDBLT_COLORFILL`도 같은 pending/immediate 경계를 사용한다.
- CPU surface backing이 있는 대상은 동일한 RGB565 값으로 채워 lock/readback 상태를 맞춘다.

## 추정

**추정:** screenshot의 하얀 영역과 수평으로 번진 cyan/검정 선은 retained logical target이 새 frame의 명시적 clear를 받지 못한 상태에서 부분 draw가 누적된 결과일 가능성이 높다. trace와 코드 경로는 이 설명을 지지하지만, 사용자의 새 build 재실행 화면으로 최종 시각 일치를 확인해야 한다.

## 미확정

- `rect_count`가 0이 아닌 부분 `D3DRECT` clear가 실제 게임 화면에서 사용되는지는 확인하지 않았다.
- presentation surface가 아닌 별도 render target에 대한 D3D clear의 전체 의미는 아직 backend에 연결하지 않았다.
- 새 build에서 두 실행의 잔상 제거 여부에 대한 사용자 시각 확인은 남아 있다.

## English

### Scope

This document compares DirectDraw/Direct3D traces from `ez2d2m` and `ez2dj4th` with the current HLE code and records the cause of output that looks like a retained afterimage. Original executables and assets are not stored.

### Confirmed

#### Shared presentation mode

Both runs record the following primary-surface descriptor:

```text
caps=0x00002218:back_buffers=1
```

This requests a flip chain containing `DDSCAPS_PRIMARYSURFACE`, `DDSCAPS_FLIP`, `DDSCAPS_COMPLEX`, and `DDSCAPS_VIDEOMEMORY`. The facade reads this descriptor and sets `RootFacade::presentation_retains_frames` to true. The two runs therefore share the same path even though neither profile has a separate retained-frame setting.

#### Explicit clear calls

- `ez2d2m` records `flags=0x00000001` (`D3DCLEAR_TARGET`) with black and progressively brighter gray `D3DCOLOR` values.
- `ez2dj4th` records `flags=0x00000003` (target and depth) with black `D3DCOLOR`.
- Both runs issue the clear before their first draw.
- The previous `LegacyDeviceClear` implementation only traced the call and did not update the logical RGB565 render target.

#### HLE cause

The retained-frame backend intentionally skips its implicit frame-start color clear so that partial updates in a flip chain survive. If the guest's explicit clear is not forwarded to the backend, previous color content remains in the next frame. The horizontal streaks and afterimage in the screenshot are consistent with this state.

Both profiles use `hle_d3d3=true`, but neither has a separate profile value selecting graphics retention. The issue is therefore classified as a missing clear-forwarding operation in the shared D3D facade, not a profile omission.

### Fixed

Task 257 adds the following path:

- Convert `D3DCOLOR` to RGB565.
- Forward a full-target `D3DCLEAR_TARGET` to `ClearRenderTarget` when the device render target is the presentation surface.
- Preserve the latest clear color in the root when the clear arrives before backend creation, then apply it before the first draw.
- Send full-surface `DDBLT_COLORFILL` through the same pending/immediate boundary.
- Fill CPU surface backing with the same RGB565 value when it exists, keeping lock/readback state coherent.

### Inferred

**Inferred:** The white regions and horizontal cyan/black streaks in the screenshot are likely the result of partial draws accumulating on a retained logical target that never received the frame's explicit clear. The trace and code path support this explanation, but final visual agreement awaits a rerun with the new build.

### Unresolved

- It is not confirmed whether a nonzero `rect_count` partial `D3DRECT` clear is used by either game.
- The full semantics of clears targeting a separate non-presentation render target are not yet connected to the backend.
- User-visible confirmation that the afterimage is gone in both runs remains pending.

## 2026-09-12 입력 및 표시 비율 후속 확인

**확인됨:** 이전 창 정책 probe의 `Alt+1/2/3`과 double-click 검사는 실제 키보드 포커스 경로가 아니라 guest HWND에 `SendMessageA`를 직접 보낸 합성 검사였다. 따라서 실제 실행에서 SDL이 wrapping한 HWND 또는 top-level host가 메시지를 받는 경우는 별도 보강이 필요했다.

**수정됨:** Task 259는 guest subclass와 host WndProc 양쪽에서 scale shortcut과 fullscreen toggle을 처리하고, windowed `WM_SIZING`에서 client 4:3을 유지한다. SDL/OpenGL `Present`도 fullscreen framebuffer를 먼저 검정색으로 지운 뒤 논리 4:3 viewport만 사용한다.

**실행 증거:** 새 Windows x86 runtime probe는 창 mode 적용 trace를 남기며 15회의 `mode-applied` 전환과 window-lifetime target을 기록한 뒤, 기존 장치/오디오 lifecycle 대기 구간에서 제한 시간에 도달했다. 이 실행의 pass/fail 종료 코드는 제한 시간 때문에 확보하지 못했다. 실제 ez2d2m 제품 실행은 보호/장치 경계에서 창 생성 전 대기하여 GUI 입력을 직접 조작하지 못했다.

**미확정:** 새 build에서 두 제품의 화면 잔상 제거와 fullscreen letterbox가 실제 화면 캡처로 최종 확인되지는 않았다. 원인 분류는 계속 공통 D3D clear forwarding 누락으로 유지하며, 입력/비율 수정은 별도 표시 정책 문제로 기록한다.

## 2026-09-12 follow-up: input and presentation aspect ratio

**Confirmed:** The earlier window-policy probe synthesized `Alt+1/2/3` and double-click checks by calling `SendMessageA` directly on the guest HWND. It did not cover an actual keyboard-focus route where SDL's wrapped HWND or the top-level host receives the message.

**Fixed:** Task 259 handles scale shortcuts and fullscreen toggles in both the guest subclass and host WndProc, and preserves a 4:3 client during windowed `WM_SIZING`. SDL/OpenGL `Present` now clears the fullscreen framebuffer to black and uses only a logical 4:3 viewport.

**Execution evidence:** The new Windows x86 runtime probe recorded 15 `mode-applied` transitions and window-lifetime targets before reaching the existing device/audio lifecycle timeout. Its pass/fail exit code was not obtained because of that timeout. The actual ez2d2m product run stopped before creating a GUI window at its protection/device boundary, so direct GUI input could not be exercised.

**Unresolved:** Removal of the afterimage and final fullscreen letterbox behavior remain unconfirmed by a user-visible capture from both products. The root-cause classification remains the shared D3D clear-forwarding omission; input and aspect-ratio changes are separate presentation-policy fixes.

## 2026-09-12 Alt+3 종료 trace 분석

**확인됨:** 제공된 `ez2dj4th` launcher handoff는 `outcome success`와 `preparation_status reached=true`를 기록한다. 별도 graphics trace에서는 창 mode 재적용 뒤 `window-lifetime:event=watcher-exit`가 `valid=1:visible=0`으로 기록된다. 이는 CHD의 FAT32-LBA volume, 확인된 `EZ2DJ/EZ2DJ.EXE` 경로, profile 준비 실패가 아니라 host lifetime watcher가 창 mode 전환 중의 숨김 구간을 종료로 오인한 증거다.

**수정됨:** host style 적용 시 `WS_VISIBLE`을 유지하고, mode transition 동안 동기식 `Flip` 검사와 비동기 watcher가 `visible=false`를 즉시 종료로 사용하지 않도록 했다. 전환 밖의 숨김은 1초 연속 관찰 뒤에만 종료하며, HWND가 무효화된 경우는 즉시 종료한다. CHD-backed VFS read-only 경로와 profile 값은 변경하지 않았다.

**미확정:** 실제 제품에서 새 build로 `Alt+3`을 누른 뒤의 사용자 화면과 종료 여부는 재실행 확인이 필요하다. 다만 제공된 trace에 대해서는 종료 원인이 profile/HLE asset 준비가 아닌 Win32 host window lifetime race로 분류되었다.

## 2026-09-12 Alt+3 exit trace analysis

**Confirmed:** The supplied `ez2dj4th` launcher handoff records `outcome success` and `preparation_status reached=true`. A separate graphics trace records `window-lifetime:event=watcher-exit` with `valid=1:visible=0` after window-mode reapplication. This is evidence of the host lifetime watcher mistaking a transient hidden interval during mode change for close, not a FAT32-LBA CHD issue, a problem with the confirmed `EZ2DJ/EZ2DJ.EXE` path, or profile preparation failure.

**Fixed:** Host style application preserves `WS_VISIBLE`, and synchronous `Flip` checks plus the asynchronous watcher no longer use `visible=false` as an immediate termination condition during a mode transition. Hiding outside a transition must persist for one second before termination; an invalid HWND still terminates immediately. The CHD-backed read-only VFS path and profile values are unchanged.

**Unresolved:** A user-visible rerun with the new build is still required to confirm the final screen and process behavior after pressing `Alt+3`. For the supplied trace, however, the exit is classified as a Win32 host-window lifetime race rather than profile or HLE asset preparation failure.
