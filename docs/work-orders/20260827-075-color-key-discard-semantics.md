# 컬러키 discard 의미 복구 작업 지시

관련 설계: [컬러키 discard 의미 복구](../design/20260827-075-color-key-discard-semantics.md)

## 상태

**진행 중.** 작업 074의 `.str` 복구 후 사용자 화면에서 Title 로고 sprite의 검은 배경이 불투명하게 남는 것이 확인됐다.

## 작업

1. OpenGL backend fragment shader에 컬러키 활성 uniform을 추가하고, 활성일 때 키에 일치한 texel을 `discard`한다.
2. discard 판정은 diffuse 변조 전 texel alpha를 쓰고, 게스트 alpha test 분기와 독립적으로 둔다.
3. discard를 게스트 `D3DRENDERSTATE_COLORKEYENABLE` 상태로만 gate해 곱셈 mask pass를 회귀시키지 않는다.
4. DirectDraw `Blt` 경로에서 컬러키를 alpha test로 흉내내던 설정을 제거하고 컬러키 상태만 남긴다.
5. `LateDraw` 진단에 게스트 `COLORKEYENABLE` 값을 `colorkey=`로 추가한다.
6. x86/x64 warnings-as-errors build와 CTest로 검증한다.
7. 사용자 detached 재실행으로 로고 투명도와 배경·mask 계층 무회귀를 확인한다.
8. architecture, TODO, IMPLEMENTED와 작업 로그를 결과에 맞춰 갱신하고 커밋한다.

---

# Color-key Discard Semantics Work Order

Related design: [Color-key Discard Semantics](../design/20260827-075-color-key-discard-semantics.md)

**In progress.** After Task 074 recovered `.str` scene scripts, the user's screen shows the Title logo sprite keeping an opaque black background.

Add a color-key-enabled uniform to the OpenGL backend's fragment shader and `discard` key-matching texels while it is active, deciding on the texel alpha before diffuse modulation and keeping that branch independent of the guest alpha test. Gate the discard solely on the guest's `D3DRENDERSTATE_COLORKEYENABLE` so the multiplicative mask passes do not regress. Remove the DirectDraw `Blt` path's emulation of color keying through the alpha test and keep only the color-key state. Add the guest `COLORKEYENABLE` value to the `LateDraw` diagnostic as `colorkey=`. Verify with x86/x64 warnings-as-errors builds and CTest, confirm logo transparency and mask-layer stability in a detached user re-run, then update the architecture, status, and work-log documents and commit.
