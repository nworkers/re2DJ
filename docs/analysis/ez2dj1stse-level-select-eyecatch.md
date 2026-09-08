# ez2dj1stse LEVEL SELECT에서 eyecatch로 넘어가는 화면

## 한국어

### 관측된 차이

원본 영상에서는 StreetMix eyecatch 아트워크가 오른쪽에서 밀려 들어오는 동안 **뒤에 LEVEL SELECT 화면이 남아** 있습니다. 우리 실행에서는 같은 자리가 검게 나옵니다.

### eyecatch가 그리는 것

eyecatch 구간의 프레임은 draw 5개뿐입니다.

| draw | 텍스처 | 크기 | 블렌드 | 비고 |
| --- | --- | --- | --- | --- |
| 1–4 | 59, 60, 61, 62 | 128×64 ~ 103×103 | 알파 | 좌하단 `1player` |
| 5 | 116 | 640×480 | `ONE / ZERO` 불투명 | 아트워크 |

아트워크의 화면 좌표가 프레임마다 왼쪽으로 이동합니다.

```
frame N+0  x = 641.0 → 1281.0   (화면 밖)
frame N+1  x = 640.7 → 1280.7
frame N+5  x = 636.3 → 1276.3
frame N+9  x = 598.5 → 1238.5
```

**확인됨.** 이것이 원본의 와이프입니다. 아직 덮이지 않은 영역은 게스트가 그리지 않으므로, 그 자리에 무엇이 보이는지는 전적으로 버퍼에 남아 있던 내용이 결정합니다.

### LEVEL SELECT가 그리는 것

LEVEL SELECT 프레임은 draw 150개입니다.

| 종류 | 개수 | 블렌드 |
| --- | --- | --- |
| 텍스처 있음 | 약 17 | `ONE / ZERO` 불투명 |
| 텍스처 있음 | 약 12 | `ONE / ONE` 가산 |
| 텍스처 없음 | 120 | `ONE / ONE` 가산, `0xff00009b` |

**전체 화면 사각형은 없습니다.** 가장 큰 것이 256×256입니다. 게스트는 불투명 draw 17개로 화면을 덮고 그 위에 가산 draw를 얹습니다.

게스트는 이 구간에서 화면을 지우지 않습니다. `DDBLT_COLORFILL`은 프레임 0–716과 2258 이후에만 나오고, 그 사이에는 한 번도 없습니다.

### 검은 화면의 출처

진단을 위해 render target을 present 직전에 되읽어 화면 중앙 행의 평균을 기록했습니다.

```
frame 2214 draws=150  row-mean=0x63
frame 2217 draws=150  row-mean=0x6f
frame 2218 draws=150  row-mean=0x76
frame 2219 draws=150  row-mean=0x72
frame 2220 draws=150  row-mean=0x00   ← 마지막 LEVEL SELECT 프레임
frame 2221 draws=5    row-mean=0x00   ← eyecatch 시작
```

프레임 2220을 만든 draw 목록은 직전 프레임과 사실상 같습니다. draw 수 150, 텍스처 26–27종, diffuse 분포와 블렌드 분포가 일치하며, 실패한 draw나 OpenGL 오류는 없습니다.

clear가 도는지도 확인했습니다. 프레임 시작 clear를 빨강, render target 생성 clear를 초록으로 임시 변경해 실행했을 때 **두 색 모두 화면에 나타나지 않았습니다.** 즉 이 구간에서 우리는 색 버퍼를 지우지 않습니다.

**확인됨.** 게스트 자신의 마지막 LEVEL SELECT 프레임이 검게 그려집니다. 같은 draw 목록이 검은 결과를 내는 원인은 그 draw들이 쓰는 텍스처의 내용입니다. 이 시점에 게스트가 eyecatch 자산을 적재하며 해당 표면을 비우고, 불투명 draw 17개가 비워진 내용을 화면 전체에 칠합니다.

### 두 버퍼 모델의 차이

게스트는 주 표면을 `caps=0x00002218`(`PRIMARYSURFACE | FLIP | COMPLEX | VIDEOMEMORY`), `back_buffers=1`로 만듭니다. 버퍼 2개짜리 플립 체인입니다.

| 모델 | eyecatch가 덮는 바탕 | 화면 |
| --- | --- | --- |
| 버퍼 1개 유지 | 검은 마지막 프레임 | 계속 검정 |
| 버퍼 2개 회전 | 프레임마다 번갈아 검정 / 직전 LEVEL SELECT | 30 Hz 교대 |

**추정.** 원본 하드웨어는 버퍼 2개 모델이므로 30 Hz로 교대합니다. 30 fps 영상은 한쪽 위상만 담으므로 LEVEL SELECT가 깨끗하게 남아 있는 것처럼 보입니다. 사용자가 보낸 원본 캡처가 그 위상입니다.

### 선택과 결정

[작업 236](../work-logs/20260909-236-flip-chain-retention.md)에서 버퍼 2개 회전을 구현했다가 되돌렸습니다. 사용자가 eyecatch 애니메이션 깜빡임을 회귀로 보고했기 때문입니다.

| 선택 | 얻는 것 | 잃는 것 |
| --- | --- | --- |
| 버퍼 1개 유지 | 교대 없음. 로딩 중 eyecatch 유지 | 와이프 뒤가 검정 |
| 버퍼 2개 회전 | 와이프 뒤에 LEVEL SELECT | 전환 구간 30 Hz 교대 |

두 증상은 같은 게스트 동작의 양면이라 한쪽만 고를 수 있습니다. 원본 하드웨어는 버퍼 2개 모델이지만, CRT에서 잔상으로 뭉개지던 30 Hz 교대가 현대 LCD에서는 깜빡임으로 드러납니다.

**결정.** 사용자가 버퍼 1개 유지를 선택했습니다. 와이프 뒤가 검게 보이는 것은 알려진 차이로 남깁니다. 이후에 다시 다룬다면 출발점은 두 모델 중 하나를 고르는 것이 아니라, 게스트가 마지막 LEVEL SELECT 프레임에서 비우는 표면을 언제 다시 채우는지 확인하는 쪽입니다.

## English

### The observed difference

In the original video the StreetMix eyecatch artwork slides in from the right while **the LEVEL SELECT screen stays visible behind it**. In our run that area is black.

### What the eyecatch draws

Each eyecatch frame issues five draws: the four `1player` sprite quads at the bottom left, between 128×64 and 103×103, and one 640×480 artwork quad drawn opaque with `ONE / ZERO`. The artwork's screen position moves left every frame — from `x = 641.0 … 1281.0`, entirely off screen, through `636.3`, `598.5`, and onward.

**Confirmed.** That is the wipe seen in the original. The guest draws nothing in the area the artwork has not yet covered, so what appears there is decided entirely by what the buffer already held.

### What LEVEL SELECT draws

Each LEVEL SELECT frame issues 150 draws: about 17 textured and opaque (`ONE / ZERO`), about 12 textured and additive (`ONE / ONE`), and 120 untextured additive quads at `0xff00009b`. **There is no full-screen quad** — the largest is 256×256. The guest covers the screen with the seventeen opaque draws and lays the additive ones over them.

It does not clear the screen through this stretch. `DDBLT_COLORFILL` appears only on frames 0 to 716 and from 2258 onward, never in between.

### Where the black comes from

A temporary readback of the render target just before presentation recorded the mean of the middle screen row. It held `0x63`, `0x6f`, `0x76`, `0x72` through LEVEL SELECT and then read `0x00` on the final LEVEL SELECT frame, before the eyecatch had drawn anything.

That frame's draw list is effectively identical to the one before it — 150 draws, 26 to 27 distinct textures, the same diffuse and blend distributions, no rejected draws and no OpenGL errors.

Whether any clear runs was checked too: with the frame-start clear temporarily set to red and the render-target creation clear to green, **neither colour appeared anywhere**. We do not clear the colour buffer through this stretch.

**Confirmed.** The guest's own last LEVEL SELECT frame renders black. The same draw list produces a black result because of what its textures now contain: the guest is loading the eyecatch assets at that moment and has blanked those surfaces, and the seventeen opaque draws paint the blanked content across the screen.

### What the two buffer models give

The guest creates its primary as `caps=0x00002218` — `PRIMARYSURFACE | FLIP | COMPLEX | VIDEOMEMORY` — with `back_buffers=1`, a two-buffer flip chain.

With one retained buffer the eyecatch composites over that black final frame, so the screen stays black behind the wipe. With two rotating buffers it composites alternately over the black frame and over the previous, intact LEVEL SELECT frame, so the screen alternates at 30 Hz.

**Inferred.** The original hardware has the two-buffer model and therefore alternates at 30 Hz. A 30 fps video records only one of the two phases, which is why LEVEL SELECT looks cleanly present behind the wipe in the capture the user supplied.

### The choice, and what was decided

[Task 236](../work-logs/20260909-236-flip-chain-retention.md) implemented two-buffer rotation and then withdrew it, because the user reported the eyecatch flicker as a regression.

Keeping one buffer avoids the alternation and holds the eyecatch through loading, but leaves the area behind the wipe black. Rotating two restores LEVEL SELECT behind the wipe at the cost of a 30 Hz alternation through the transition. The two symptoms are one guest behaviour seen two ways, so only one can be had: the original hardware uses the two-buffer model, but the alternation that a CRT smeared together is plainly visible on a modern LCD.

**Decided.** The user chose to keep the single retained buffer. The black area behind the wipe stays as a known difference. If this is picked up again, the starting point is not choosing between the two models but establishing when the guest refills the surfaces it blanks on that last LEVEL SELECT frame.
