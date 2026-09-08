# 작업 로그: 플립 체인 버퍼 유지

## 한국어

### 관련 문서

- 설계: [플립 체인 버퍼 유지 설계](../design/20260909-236-flip-chain-retention.md)
- 작업 지시: [플립 체인 버퍼 유지](../work-orders/20260909-236-flip-chain-retention.md)
- 선행 작업: [프레임별 draw 요약 진단](20260908-234-frame-draw-summary.md), [페이드 블렌드 인자 보존](20260908-235-fade-blend-factors.md)

### 해결한 문제

사용자가 보고한 두 가지입니다.

1. Warning이 뜰 때의 깜빡임
2. StreetMix 로딩 화면이 너무 빨리 사라지고 검은 화면이 남는 문제

**두 문제의 원인은 하나입니다.** 게스트는 표시할 화면을 `DDBLT_COLORFILL`로 지우고 그 밖에는 지우지 않는데, 우리는 그 반대로 하고 있었습니다. 게스트의 지우기 요청은 표면의 시스템 메모리 사본에만 쓰고, 게스트가 지운 적 없는 render target은 프레임마다 검게 지웠습니다.

### 진단 경로

[작업 234](20260908-234-frame-draw-summary.md)의 프레임 요약에 프레임 표시 시간(`ms=`)을 더하고, `LateDraw` 진단 대상을 해당 구간으로 옮겨 draw별 텍스처·경계·블렌드를 확보했습니다.

StreetMix 구간 기록입니다.

| 프레임 | draws | 표시 시간 |
| --- | --- | --- |
| 1267–1291 | 5 | 각 ~16.6 ms, 합계 397 ms |
| 1292–1293 | 4 | 16 ms |
| 1294 | 33 | 직전 프레임이 **1033.76 ms** 표시됨 |

eyecatch 프레임의 draw 5개는 좌하단 `1player` sprite 4개와 640×480 배경(텍스처 116, `ONE / ZERO` 불투명)입니다. 프레임 1292–1293은 **sprite 4개만** 그립니다.

주 표면은 `caps=0x00002218`(`PRIMARYSURFACE | FLIP | COMPLEX | VIDEOMEMORY`), `back_buffers=1`로 만들어집니다. **확인됨.** 게스트는 버퍼 2개짜리 플립 체인을 만들고, 로딩 중에는 sprite만 다시 그린 뒤 flip합니다. 배경은 체인에 남아 있어야 하며, 사용자가 보낸 원본 캡처가 아트워크 위에 `1player`가 얹힌 모습인 것이 이와 일치합니다.

Warning 쪽은 부팅 추적의 순서가 답이었습니다.

```
seq=5  Blt(colorfill) frame=0
seq=6  Blt(colorfill) frame=0
seq=8  LateDraw       frame=0  texture=3  diffuse=0xffffffff
seq=11 Blt(colorfill) frame=0
seq=12 FrameDraws     frame=1  ms=0.00
```

**게스트는 Warning을 그린 뒤 화면을 지우고 flip합니다.** 마지막 colour fill이 render target에 닿지 않아 우리는 Warning을 전체 밝기로 표시했고, 그 프레임이 224 ms 동안 남았습니다. 이어서 페이드가 5%에서 다시 올라오므로 깜빡임으로 보입니다. 페이드 아웃 끝(프레임 202, 84 ms)도 같은 모양입니다.

### 코드 변경

| 항목 | 내용 |
| --- | --- |
| `Sdl3OpenGlWindowConfig::retain_between_frames` | 프레임을 넘겨 색을 유지할지 |
| `CreateRenderTarget` | 색 텍스처를 만들고 생성 시 한 번 검게 지움 |
| `Draw` | 유지면 깊이만, 아니면 깊이와 색을 지움 |
| `ClearRenderTarget` | RGB565 색으로 render target을 지우는 공개 진입점 |
| `RootFacade::presentation_retains_frames` | 주 표면에 `DDSCAPS_FLIP`이 있으면 참 |
| `RootFacade::presentation_surface` | 게스트가 표시하는 표면. 장치 생성과 `SetRenderTarget`에서 갱신 |
| `SurfaceBlt` colour fill | 표시 표면 전체를 채우면 백엔드 clear도 호출 |

부분 영역 colour fill은 백엔드를 건드리지 않습니다. 그것은 화면 지우기가 아니라 영역 갱신이고, 표면의 메모리 사본이 이미 담습니다.

깊이 버퍼는 어느 쪽이든 프레임마다 지웁니다. 깊이는 프레임을 넘어 읽히지 않는 임시 버퍼이고, 이 제품의 모든 draw가 `ZENABLE=0`입니다.

### 검증 — 결과

burst 캡처로 프레임 평균 밝기를 측정했습니다.

Warning 등장:

```
수정 전: 0 ×9 → (224 ms 전체 밝기 프레임) → 1 3 4 6 8 10 12 14
수정 후: 0 ×9 → 1 3 3 4 6 8 10 12 14 → 14 유지
```

Warning에서 로고로:

```
수정 후: 14 유지 → 13 11 9 7 5 3 1 1 0 0 → 1 2 2 6 6 7 10 11 14 15 16
```

**두 구간 모두 밝은 프레임 없이 단조롭게 변합니다.**

StreetMix 로딩:

```
수정 전: 44 45 → 2 47 8 14 22 34 74 85 89 → 2 ×20 (검은 화면 약 1초)
수정 후: 44 45 → 2 47 8 14 22 34 74 85 89 → 89 ×20 (아트워크 유지)
```

유지 구간의 캡처가 **STREET MIX 아트워크 위 좌하단 `1player`**로, 사용자가 보낸 원본 캡처와 같습니다.

### 3rd Trax 회귀와 그 수정

첫 구현은 버퍼 개수 기본값을 2로 두었습니다. 그 결과 3rd Trax의 title 화면 배경이 흰 줄무늬로 번졌고 사용자가 회귀로 보고했습니다. **최초 회귀 확인에서 이 차이를 애니메이션 위상 차이로 잘못 판단했습니다.** 평균 밝기만 비교하고 이미지를 자세히 보지 않은 것이 원인입니다.

3rd Trax의 추적은 다음과 같습니다.

| 항목 | 값 |
| --- | --- |
| 주 표면 | `caps=0x00000200`, `back_buffers=0`, `DDSCAPS_FLIP` 없음 |
| 표시 | 1280×960 offscreen 표면(`caps=0x2040`)을 주 표면으로 `Blt`, flags `DDBLT_WAIT` |
| `Flip` | 0회 |
| colour fill | 0회 |

**확인됨.** 이 제품은 표면 전체를 화면에 복사해 표시하므로 프레임을 넘어 이월되는 내용이 없습니다. 버퍼를 회전시키면 서로 다른 두 이미지를 번갈아 쓰게 되고, 색을 지우지 않으면 배경이 이전 프레임 위에 누적됩니다.

수정 후 3rd Trax의 title 화면이 변경 전과 같은 모습으로 돌아왔습니다. 1st SE와 4th는 `caps=0x2218`로 `DDSCAPS_FLIP`을 포함하므로 색을 유지합니다.

### 1st SE eyecatch 깜빡임과 버퍼 회전 철회

버퍼를 `back_buffers + 1`개 두고 회전시키는 첫 구현에서 사용자가 두 번째 회귀를 보고했습니다. StreetMix 선택 시 애니메이션이 깜빡인다는 것입니다.

burst 캡처의 프레임 평균 밝기가 근거였습니다.

```
회전 사용: 45 46 47 3 50 15 60 49 85 87 88 88 89 ...
```

`47 3 50 15 60 49`는 밝은 프레임과 어두운 프레임이 교대한다는 뜻입니다. **확인됨.** 두 버퍼가 서로 다른 이미지를 담아 30 Hz로 번갈아 표시됩니다. 게스트가 화면 일부만 다시 그리는 프레임에서 각 버퍼가 서로 다른 시점의 내용 위에 그리기 때문입니다.

버퍼 회전은 실제 back buffer의 내용 나이를 재현하지만 원본에 없는 증상을 만듭니다. 그래서 회전을 철회하고 버퍼 하나를 유지하는 쪽으로 바꿨습니다. 유지 대상이 하나이므로 프레임은 항상 직전 프레임 위에 그려지고 교대가 없습니다.

```
버퍼 하나: 46 2 4 7 12 18 35 61 85 89 89 90 90 ... 90
```

**전환이 단조롭고, eyecatch는 로딩 동안 그대로 유지됩니다.**

### 검증 — 시험과 회귀

- Windows x86 Release 전체 build 성공
- `re2dj_unit_tests.exe` → `checks: 1421, failures: 0`
- `re2dj_windows_product_loader_probe.exe` → 4개 항목 ok
- `re2dj_windows_vfs_runtime_probe.exe --vfs-enumeration-only` → exit 0
- 3rd·4th를 변경 전후로 같은 스크립트로 실행해 비교했습니다. 최종 수정 후 3rd의 title 화면 캡처가 변경 전 캡처와 같은 모습이며 번짐이 없습니다. 4th는 배경 회로 무늬가 선명하고 누적 흔적이 없습니다.
- 1st SE 최종 측정: Warning 페이드 인 `0 ×9 → 1 3 4 4 7 9 10 13 14 → 14 유지`, Warning에서 로고로 `14 유지 → 12 11 9 7 5 3 1 0 0 0 → 1 2 4 6 7 9 11 14 15`, StreetMix 전환 `46 → 2 4 7 12 18 35 61 85 89 → 90 유지`.

### 남은 과제

- 와이프 뒤에 LEVEL SELECT가 보이지 않고 검게 나옵니다. 원인과 두 버퍼 모델의 차이를 [LEVEL SELECT에서 eyecatch로 넘어가는 화면](../analysis/ez2dj1stse-level-select-eyecatch.md)에 정리했고, 사용자가 버퍼 1개 유지를 선택해 알려진 차이로 남깁니다.
- 게임플레이 화면 배경이 단색 위 실루엣으로 보입니다.
- 프로파일 쓰기(`WritePrivateProfileStringA`)의 overlay 정책.
- 정적 IAT 패치가 packed build에서 무효가 되는 다른 경계 점검.

## English

### Related documents

- Design: [Flip Chain Buffer Retention Design](../design/20260909-236-flip-chain-retention.md)
- Work order: [Flip Chain Buffer Retention](../work-orders/20260909-236-flip-chain-retention.md)
- Preceding tasks: [Per-frame Draw Summary Diagnostic](20260908-234-frame-draw-summary.md), [Preserving the Guest's Fade Blend Factors](20260908-235-fade-blend-factors.md)

### Problem solved

Both of the user's reports: the flicker as the Warning screen comes up, and the StreetMix loading screen vanishing too quickly and leaving a black screen.

**They have one cause.** The guest clears the screen it is about to present with `DDBLT_COLORFILL` and clears nothing otherwise, and we did the opposite — its clear request reached only the surface's system-memory copy, while the render target it never asked to clear was wiped black every frame.

### Diagnostic path

The frame summary from [task 234](20260908-234-frame-draw-summary.md) gained an on-screen time (`ms=`), and the `LateDraw` diagnostic was retargeted at the frames in question to obtain each draw's texture, bounds, and blend.

The eyecatch occupies frames 1267 to 1291 at `draws=5`, about 16.6 ms each and 397 ms in total; frames 1292 and 1293 drop to `draws=4`; and frame 1294's record reads `ms=1033.76`, meaning the previous frame stayed on screen for **1.03 seconds**. The eyecatch frame's five draws are the four `1player` sprite quads at the bottom left plus a 640×480 background, texture 116, opaque with `ONE / ZERO`. Frames 1292 and 1293 issue **only the four sprites**.

The primary is created as `caps=0x00002218` — `PRIMARYSURFACE | FLIP | COMPLEX | VIDEOMEMORY` — with `back_buffers=1`. **Confirmed.** The guest made a two-buffer flip chain and, while loading, redraws only the sprite before flipping, expecting the background to still be in the chain. The user's capture of the original, the artwork with `1player` over it, matches that.

For the Warning, the boot trace order was the answer: two colour fills, then the Warning draw at full brightness, then **a third colour fill**, then the flip. **The guest draws the Warning and then clears the screen before presenting.** That last fill never reached the render target, so we presented the Warning at full brightness and held it for 224 ms before the fade came back up from 5% — the flicker. The end of the fade-out behaves the same way, at 84 ms.

### Code change

`Sdl3OpenGlWindowConfig` gained `retain_between_frames`. `CreateRenderTarget` clears the colour texture once at creation, and `Draw` clears depth alone when frames are retained and both depth and colour when they are not. A new `ClearRenderTarget` clears the target to an RGB565 colour.

On the facade side, `RootFacade` records whether the primary carries `DDSCAPS_FLIP` and which surface the guest presents from, the latter updated at device creation and at `SetRenderTarget`. A `DDBLT_COLORFILL` covering that whole surface now also clears the backend target. A partial fill does not: it is a region update rather than a screen clear, and the surface's memory copy already carries it.

The depth buffer is cleared per frame either way — it is scratch that nothing reads across frames, and every draw in this title runs with `ZENABLE=0`.

### Verification — result

Burst captures measured mean frame brightness. The Warning used to come up as a 224 ms full-brightness frame before the fade restarted from 5%; it now steps `0` up through `1 3 3 4 6 8 10 12 14` and holds. The Warning-to-logo transition steps down `14 13 11 9 7 5 3 1 1 0 0` and the logo rises from `1`. **Neither end shows a bright frame any more.**

The StreetMix loading screen used to fade in to 89 and then drop to 2 for about a second; it now fades in to 89 and **holds at 89** through the load. The held frame is the STREET MIX artwork with `1player` at the bottom left — the same as the user's capture of the original.

### The 3rd Trax regression and its fix

The first implementation defaulted the buffer count to 2, which smeared 3rd Trax's title-screen background into white streaks; the user reported it as a regression. **The first regression check misread that difference as an animation-phase difference** — mean brightness was compared without looking closely at the images.

3rd Trax's trace shows a primary of `caps=0x00000200` with `back_buffers=0` and no `DDSCAPS_FLIP`, presentation by `Blt`ing a 1280×960 offscreen surface (`caps=0x2040`) onto that primary with `DDBLT_WAIT`, no `Flip` calls, and no colour fills.

**Confirmed.** This product presents by copying a whole surface over the screen, so nothing carries over between frames. Rotating buffers under it alternates two diverging images, and not clearing colour lets its background accumulate onto the previous frame.

Clearing colour again for a copy-presented guest returned 3rd Trax's title screen to what it looked like before the change. 1st SE and 4th declare `caps=0x2218`, which includes `DDSCAPS_FLIP`, so they keep their colour.

### The 1st SE eyecatch flicker, and withdrawing buffer rotation

The first implementation gave the target `back_buffers + 1` textures and rotated them, and the user reported a second regression: the animation flickers when StreetMix is selected.

Burst captures of mean frame brightness carried the evidence. With rotation the eyecatch transition read `45 46 47 3 50 15 60 49 85 87 88 88 89 …` — bright and dark frames alternating. **Confirmed.** The two buffers hold different images and are shown alternately at 30 Hz, because in a frame where the guest redraws only part of the screen each buffer builds on content from a different point in time.

Rotation reproduces the age of a real back buffer's contents but produces a symptom the original does not have, so it was withdrawn in favour of a single retained buffer. With one buffer every frame builds on the one immediately before it and nothing alternates: the same transition now reads `46 2 4 7 12 18 35 61 85 89 89 90 90 … 90` — **monotonic, and the eyecatch still holds through the load.**

### Verification — tests and regression

The full Windows x86 Release build succeeded, `re2dj_unit_tests.exe` reported `checks: 1421, failures: 0`, the product-loader probe reported all four items ok, and `re2dj_windows_vfs_runtime_probe.exe --vfs-enumeration-only` exited 0.

3rd and 4th were run with the same script before and after the change. With the final fix in place, 3rd's title screen looks the same as it did before the change, with no smearing, and 4th's circuit-pattern background is sharp with no sign of accumulation. The final 1st SE measurements are `0 ×9 → 1 3 4 4 7 9 10 13 14 → 14` for the Warning fade-in, `14 → 12 11 9 7 5 3 1 0 0 0 → 1 2 4 6 7 9 11 14 15` from the Warning to the logo, and `46 → 2 4 7 12 18 35 61 85 89 → 90` held for the StreetMix transition.

### Remaining work

- The area behind the eyecatch wipe is black rather than showing LEVEL SELECT. The cause and the difference between the two buffer models are recorded in [the LEVEL SELECT to eyecatch transition](../analysis/ez2dj1stse-level-select-eyecatch.md); the user chose to keep the single retained buffer, so it stays as a known difference.
- The gameplay background renders as silhouettes over a flat colour.
- The overlay policy for profile writes such as `WritePrivateProfileStringA`.
- Auditing other boundaries whose static IAT patch is overwritten on a packed build.
