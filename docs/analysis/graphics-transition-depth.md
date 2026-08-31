# 장면 전환·깊이 정렬 분석

## 확인 상태와 근거

이 문서는 2026-08-30 작업 096의 최신 Win32 ddraw trace와 SDL3/OpenGL backend 대조 결과를 기록한다. 원본 자산은 저장소에 추가하지 않았으며, 모든 실행 근거는 사용자가 제공한 HDD 경로에서 얻었다.

### 확인됨

- SDL3/OpenGL backend는 기존에 depth buffer를 요청하지 않았고, frame 첫 draw에서 `GL_DEPTH_TEST`를 무조건 끄며 색상 버퍼만 지웠다.
- Windows Direct3D facade는 `ZENABLE`, `ZWRITEENABLE`, `ZFUNC`를 ddraw 진단에 기록했지만 공용 draw state로 전달하지 않았다.
- canonical 실행 `20260830-104631-572`의 `LateDraw` 600여 건은 모두 guest `zenable=0`, `zwrite=0`, `zfunc=0`이었다. 따라서 해당 실행만으로 실제 게임 장면의 Z-enabled draw는 확인되지 않는다.
- 같은 실행의 초기 transition은 background texture draw 다음 `texture=0`, full-screen bounds `0,0,640,480`, 검정 diffuse, alpha `0x0c`, `0x18`, `0x24` … quad를 반복했다. guest `blend=0`인 동안 새 진단은 이 후보를 `effectiveblend=1`, `SRCALPHA`, `INVSRCALPHA`, `fadecompat=1`로 표시했다.
- 작업 096 수정 뒤 실행은 수백 회 draw/present를 graphics failure 없이 유지했고, 창 닫기 후 child process가 종료됐다.
- 최종 실행 `20260830-105546-182`에서는 `dstblend=4`가 반복되었고, 기존 구현이 이를 `unsupported Direct3D3 alpha blend factor`로 거절해 64회의 draw 실패가 기록됐다. 이 값은 Direct3D `D3DBLEND_INVSRCOLOR`이며 `GL_ONE_MINUS_SRC_COLOR`로 매핑해야 한다.
- 수정 후 실행 `20260830-110037-388`에서는 동일 blend 상태가 더 이상 거절되지 않았고, `unsupported Direct3D3 alpha blend factor` 0회, `LateDraw` 512회, fade marker 41회를 기록했다.

### 추정

- alpha blend가 꺼진 검정 full-screen quad는 Direct3D의 일반 의미라면 alpha를 무시하고 불투명 검정을 쓸 수 있으므로, 관찰된 빠른 black-out 또는 깜빡임과 관련 있을 가능성이 있다.
- scene ordering 문제의 일부는 depth 상태 누락으로 설명될 수 있다. 다만 현재 canonical trace는 Z를 명시적으로 켜지 않아 draw 순서 의존 가능성을 배제하지 못한다.

### 미확정

- 원본 Direct3D driver가 해당 alpha-blend-disabled quad를 실제로 어떻게 rasterize했는지.
- 모든 장면 전환이 위의 full-screen black quad 패턴을 사용하는지.
- 원본이 어떤 장면에서 depth test/write를 켜는지, 또는 항상 호출 순서로 정렬하는지.
- 사용자 화면에서 작업 096 보정 후 모든 깜빡임·fade-out·z 정렬 문제가 해소되었는지.

## 구현 경계

공용 `LegacyFixedFunctionState`에 depth test/write/compare를 추가하고, SDL context에 16-bit depth buffer를 요청한다. backend는 `glDepthFunc`와 `glDepthMask`를 draw마다 적용하고 frame 시작에 color/depth를 함께 clear한다. Windows facade는 Direct3D compare enum과 `D3DBLEND_INVSRCOLOR`/`D3DBLEND_INVSRCALPHA`를 매핑한다.

fade 호환성은 texture가 없고, 네 정점 triangle strip이 논리 화면을 덮으며, 모든 RGB가 검정이고 alpha가 0과 255 사이로 같은 경우에만 적용한다. 이 보정은 상태 진단에 별도로 기록되며 일반 geometry의 blend 의미를 바꾸지 않는다.

---

# Scene-transition and depth-ordering analysis

## Status and evidence

This document records the comparison of the latest Win32 ddraw trace and the SDL3/OpenGL backend in Task 096 on 2026-08-30. Original assets remain outside the repository and evidence comes from the user-supplied HDD path.

### Confirmed

- The SDL3/OpenGL backend previously requested no depth buffer, unconditionally disabled `GL_DEPTH_TEST` on the first draw of each frame, and cleared only color.
- The Windows Direct3D facade logged `ZENABLE`, `ZWRITEENABLE`, and `ZFUNC` but did not pass them into the shared draw state.
- All 600-plus `LateDraw` records in canonical run `20260830-104631-572` have guest `zenable=0`, `zwrite=0`, and `zfunc=0`; an actual Z-enabled product scene is therefore not proven by this run.
- Early transition frames repeatedly draw an untextured full-screen black quad after the background texture, with alpha values such as `0x0c`, `0x18`, and `0x24` while guest `blend=0`. The corrected run marks this narrow candidate as `effectiveblend=1`, `SRCALPHA`, `INVSRCALPHA`, and `fadecompat=1`.
- The corrected run sustains hundreds of draws and presents without graphics failure, and the child exits after window close.

### Inferred

- With alpha blending disabled, a black full-screen quad can ignore its vertex alpha under ordinary Direct3D semantics, which may explain an abrupt black-out or flicker.
- Some z-order symptoms may be caused by the missing depth state, but the current trace does not enable Z, so dependence on draw order cannot be ruled out.
- The same final run shows a second independent rendering gap: repeated `dstblend=4` draws are rejected until Direct3D `INVSRCOLOR` is mapped to `GL_ONE_MINUS_SRC_COLOR`.
- After the mapping, corrected run `20260830-110037-388` records zero unsupported-blend failures across 512 `LateDraw` entries and 41 fade markers.

### Unresolved

- The exact rasterization performed by the original Direct3D driver for the alpha-blend-disabled quad.
- Whether every scene transition uses this full-screen black-quad pattern.
- Which original scenes enable depth testing/writes, if any.
- Whether all user-visible flicker, fade-out, and z-order defects disappear after Task 096.

## Implementation boundary

The shared `LegacyFixedFunctionState` now carries depth test/write/compare. SDL requests a 16-bit depth buffer; the backend applies `glDepthFunc` and `glDepthMask` for every draw and clears color plus depth at frame start. The Windows facade maps Direct3D comparison values and `D3DBLEND_INVSRCOLOR`/`D3DBLEND_INVSRCALPHA`.

Fade compatibility is limited to an untextured four-vertex triangle strip covering the logical screen with uniform black RGB and alpha strictly between zero and 255. The correction is reported separately and does not change blend semantics for general geometry.
