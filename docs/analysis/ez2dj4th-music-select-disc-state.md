# EZ2DJ 4th Music Select 원판 상태 분석

# EZ2DJ 4th Music Select Disc-State Analysis

## 확인된 사실 / Confirmed Facts

### 최신 사용자 실행 / Latest User Run

사용자가 직접 Music Select까지 진입한 `20260905-164819-234` 실행의 DDraw trace에서 중앙 artwork는 `texture=387`, 선택 ring은 `texture=279`입니다. 두 draw의 raw state에는 `D3DRENDERSTATE_CULLMODE=3` (`D3DCULL_CCW`)이 기록됩니다.

*In the user-driven `20260905-164819-234` run that reached Music Select, the DDraw trace identifies the center artwork as `texture=387` and the selection ring as `texture=279`. The raw state for both draws records `D3DRENDERSTATE_CULLMODE=3` (`D3DCULL_CCW`).*

따라서 OpenGL backend가 cull mode를 아직 적용하지 않는 것은 실제 Direct3D/OpenGL 상태 차이입니다. 다만 두 draw는 각각 하나의 동일 방향 triangle strip quad이고 원본 화면에도 표시되므로, 이 로그만으로 culling이 과노출의 원인이라고 확정할 수는 없습니다. “구현 차이”로는 승격하되, 현재 화면의 직접 원인은 미확정으로 유지합니다.

*The OpenGL backend's lack of cull-mode application is a confirmed Direct3D/OpenGL state difference. However, each core draw is one same-winding triangle-strip quad and is visible in the original screen, so this log alone cannot prove that culling causes the overexposure. It is promoted as an implementation gap while the direct cause remains unresolved.*

## 미확정 항목 / Unresolved Items

일반 `LateDraw` 진단은 frame 1475에 도달하기 전에 개수 상한에 도달했습니다. 이에 따라 다음 실행에서는 두 texture identity를 대상으로 다음 raw state를 별도 기록합니다.

- 정점별 position, reciprocal W, UV, diffuse ARGB
- stage-0 `TEXCOORDINDEX` (11), `TEXTURETRANSFORMFLAGS` (24)
- texture transform 0 matrix (transform state 16)
- cull, blend, depth raw state

*The ordinary `LateDraw` diagnostic reaches its count limit before frame 1475. The next run therefore records the following raw state separately for the two texture identities:*

- *per-vertex position, reciprocal W, UV, and diffuse ARGB;*
- *stage-0 `TEXCOORDINDEX` (11) and `TEXTURETRANSFORMFLAGS` (24);*
- *texture transform 0 matrix (transform state 16);*
- *raw cull, blend, and depth state.*

이 집중 trace는 렌더링 경로를 변경하지 않습니다. `textransformflags != 0` 또는 `texcoordindex != 0`이면 현재 backend가 texture-coordinate processing을 무시하는 것이 다음 구현 후보가 됩니다. 두 값이 기본값이고 정점 색/UV도 동일하면 color-key filtering 순서 또는 D3D fixed-function rasterization 차이를 별도 실험해야 합니다.

*This focused trace does not change rendering. In run `20260905-164819-234`, `textransformflags=0`, `texcoordindex=0`, the texture matrix is zero but inactive, and the UV ranges are exactly 0..1. Texture-coordinate transform is therefore not the current candidate. The remaining candidates include cull handling for other geometry, color-key filtering order, and Direct3D fixed-function rasterization.*

## 최신 집중 trace 결과 / Latest Focused Trace Result

**확인됨:** `20260905-164819-234.ddraw.log`에는 `MusicSelectDiscDraw`가 2048건 기록되었습니다. 중앙 artwork draw는 `frame=872`부터 `texture=387`, UV `0..1`, diffuse `0xffffffff`, `srcblend=2`, `dstblend=2`, `zenable=0`, `zwrite=0`으로 반복됩니다. ring draw는 `texture=279`이며 animation 중 diffuse가 `0x00000000`부터 증가하는 값으로 기록됩니다. 두 texture 모두 `cull=3`입니다.

*Confirmed: `20260905-164819-234.ddraw.log` contains 2048 `MusicSelectDiscDraw` records. Center artwork draws repeat from `frame=872` with `texture=387`, UV 0..1, diffuse `0xffffffff`, `srcblend=2`, `dstblend=2`, `zenable=0`, and `zwrite=0`. Ring draws use `texture=279` and have animated diffuse values increasing from `0x00000000`; both textures use `cull=3`.*

**확인됨:** `texcoordindex=0`과 `textransformflags=0`이므로 현재 원판은 정점 UV를 그대로 사용합니다. `texture=387`의 모든 정점은 `(127,96)`–`(383,352)`의 256×256 quad이고 diffuse는 모두 white입니다. 따라서 이번 로그에서는 UV transform, 정점 색상 누락, Z ordering이 원판 자체의 원인으로 보이지 않습니다.

*Confirmed: With `texcoordindex=0` and `textransformflags=0`, the disc uses vertex UVs directly. Every `texture=387` vertex belongs to the 256×256 quad `(127,96)`–`(383,352)` and has white diffuse. In this run, UV transform, missing vertex color, and Z ordering are not indicated as causes for the disc itself.*

## Culling 전달 구현 상태 / Culling Forwarding Implementation Status

**확인됨:** 후속 구현에서 공용 `LegacyFixedFunctionState`에 cull mode를 추가하고, Direct3D facade가 `D3DCULL_NONE/CW/CCW`를 전달하도록 했습니다. OpenGL backend는 triangle draw에만 back-face culling을 적용하며, 현재 shader의 guest top-left Y 변환을 보정하기 위해 `D3DCULL_CCW(3)`을 `glFrontFace(GL_CW)`로 매핑합니다. line draw와 Present pass에서는 culling을 끕니다.

*Confirmed: The follow-up implementation adds cull mode to `LegacyFixedFunctionState` and forwards `D3DCULL_NONE/CW/CCW` from the Direct3D facade. The OpenGL backend applies back-face culling only to triangle draws and maps the observed `D3DCULL_CCW(3)` to `glFrontFace(GL_CW)` to compensate for the current shader's guest top-left Y conversion. Culling is disabled for line draws and the Present pass.*

**사용자 확인:** culling 적용 후에도 화면이 같으며 상단 `select a music` 헤더에 가려져야 할 작은 디스크와 배경 광선이 보입니다. culling 전달은 구현 누락을 보완했지만 이 증상은 해소하지 못했습니다. 다음 분석은 성공 draw뿐 아니라 그 전에 거절된 마스크 draw를 포함합니다.

*User-confirmed: Culling leaves the screen unchanged; the small disc and background rays remain visible through the header where original hardware occludes them. Forwarding culling closes an implementation gap but does not resolve this symptom. The next analysis includes rejected mask draws, not only successful draws.*

## 헤더 합성과 거절된 draw / Header Composition and Rejected Draws

**확인됨:** `20260905-174233-086.ddraw.log` frame 1000에서 배경(`seq=16665`), 디스크(`16666`–`16670`), ring(`16671`), 헤더 artwork `texture=281`(`16672`) 순서로 성공 draw가 기록됩니다. 헤더 bounds는 `-2,-4,642,92`이며 ONE/ONE 가산 합성입니다. 하단 `texture=282/283`, 우측 `284/285`, `286/287`에는 ZERO/SRCCOLOR 마스크 후 ONE/ONE 그림 쌍이 있습니다. 성공 draw만 보면 헤더 앞의 대응 mask가 없습니다.

*Confirmed: Frame 1000 of `20260905-174233-086.ddraw.log` records successful draws in order: background (seq 16665), discs (16666–16670), ring (16671), and header artwork texture 281 (16672). The header covers -2,-4,642,92 and uses ONE/ONE addition. Bottom textures 282/283 and right-panel pairs 284/285 and 286/287 use ZERO/SRCCOLOR masks followed by ONE/ONE artwork. The successful-draw sequence lacks a corresponding mask before the header.*

**확인됨:** 같은 실행의 최초 draw 오류(`seq=967`, `texture=73`)는 SRCBLEND=9 / DESTBLEND=6에 대한 `unsupported Direct3D3 alpha blend factor`입니다. 이후 texture 76 반복 오류가 전역 실패 진단 64건을 소모합니다. 기존 `BuildFixedFunctionState`는 factor 9를 거절하고 backend 호출 전 반환하므로 해당 draw 전체가 생략되며 `LateDraw`에도 나타나지 않습니다. 따라서 기존 성공 draw 분석만으로 blend 구현이 정상이라고 판단할 수 없습니다.

*Confirmed: The first draw error in the same run (seq 967, texture 73) rejects SRCBLEND=9 / DESTBLEND=6. Repeated texture-76 failures consume the 64-entry global failure budget. The previous `BuildFixedFunctionState` rejects factor 9 before calling the backend, dropping the entire draw and its `LateDraw` record. Successful draws alone therefore cannot establish correct blend support.*

**추정:** 상단 헤더와 디스크 마스크도 이 미지원 factor 때문에 빠져 가산 그림만 남는 것이 현재 가장 강한 후보입니다. 기존 로그에는 Music Select의 개별 실패가 없으므로 `texture=280`의 실제 blend 값과 역할은 아직 확정하지 않습니다.

*Inferred: Unsupported destination-color factors dropping header/disc masks while leaving additive artwork is the strongest current candidate. The old log lacks individual Music Select failures, so texture 280's actual blend values and role are not yet established.*

**확정됨:** 사용자 실행 `20260905-185621-933`에서 중앙 마스크 `texture=250`과 상단 헤더 마스크 `texture=280`이 모두 `srcblend=9` (`DESTCOLOR`), `dstblend=6` (`INVSRCALPHA`), `reason=success`로 처리되었습니다. 두 마스크는 각각 중앙 artwork bounds `(127,96)-(383,352)`와 헤더 bounds `(-2,-93)-(642,3)`에 해당하며, 헤더 artwork `texture=281`도 frame 1298에서 성공했습니다. 이 실행의 `result=0x80004005`, `unsupported Direct3D3 alpha blend factor`, `draw-failure`는 0건입니다. 사용자가 제공한 화면에서도 원본처럼 헤더 뒤 디스크가 가려지고 과노출이 사라졌으므로, 목적지 색상 블렌드 미지원으로 mask draw 전체가 생략된 것이 이번 문제의 원인으로 확정됩니다.

*Confirmed: In user run `20260905-185621-933`, the center mask texture 250 and header mask texture 280 both use `srcblend=9` (DESTCOLOR) and `dstblend=6` (INVSRCALPHA) with `reason=success`. They cover the center-artwork bounds `(127,96)-(383,352)` and header bounds `(-2,-93)-(642,3)`; additive header artwork texture 281 also succeeds at frame 1298. The run has zero `result=0x80004005`, `unsupported Direct3D3 alpha blend factor`, and `draw-failure` records. The user's screen matches the original behavior: the header occludes the disc and the overexposure is gone. The missing destination-color blend support, which dropped the entire mask draw, is confirmed as the cause.*
