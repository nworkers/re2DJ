# 컬러키 discard 의미 복구 작업 로그

관련 설계: [컬러키 discard 의미 복구](../design/20260827-075-color-key-discard-semantics.md)  
관련 작업 지시: [컬러키 discard 의미 복구](../work-orders/20260827-075-color-key-discard-semantics.md)

## 증상

작업 074의 `.str` 복구 뒤 Title 화면이 표시됐다. 사용자 화면에서 배경 `bg.bmp`, `SE-logo.bmp`, 저작권 문구는 정상이지만 EZ2DJ 로고 sprite 뒤에 원본 BMP의 검은 배경이 불투명한 사각형으로 남았다.

## 원인 분석

- 원본 자산 `System\Title\logo.bmp`는 24bpp 256×256이고 배경이 순수 검정 `(0, 0, 0)`이다. 샘플링한 상위 색은 `(0,0,0)`, `(255,255,255)`, `(27,27,27)` 순이다.
- 실행 로그 `20260827-020438-743.ddraw.log`의 source color key는 `keylow=0x0000:keyhigh=0x0000`이다. 키 값 자체는 맞다.
- backend는 컬러키를 texture upload 때 alpha `0`으로 굽고, 그 texel을 실제로 버리는 것은 shader의 `u_alpha_test_enabled` 분기뿐이었다. 이 값은 게스트 `D3DRENDERSTATE_ALPHATESTENABLE`에서 온다.
- 로그의 `alphatest=0`이 보여주듯 원본은 Direct3D draw 경로에서 alpha test를 켜지 않는다. blend factor는 `srcblend=2(D3DBLEND_ONE)`, `dstblend=1(D3DBLEND_ZERO)`인 단순 복사라 alpha가 결과에 관여하지 않는다. 그래서 keyed texel이 검정 그대로 기록됐다.
- DirectDraw `Blt` 경로는 `color_key_enabled`와 `alpha_test_enabled`를 같은 값으로 넣고 있어 증상이 없었다. 문제는 두 상태를 게스트가 준 대로 독립 전달하는 Direct3D `DrawPrimitive` 경로였다.

Direct3D에서 컬러키는 alpha blending이 아니다. `D3DRENDERSTATE_COLORKEYENABLE`이 켜진 동안 키에 일치하는 texel은 blend factor와 무관하게 버려진다.

## 구현

- fragment shader에 `u_color_key_enabled` uniform을 추가하고, 활성일 때 diffuse 변조 전 texel alpha가 임계값 미만이면 `discard`한다. 게스트 alpha test 분기는 원래 의미대로 별도로 남긴다.
- discard를 `state.color_key_enabled`(게스트 `COLORKEYENABLE`)와 texture의 source key 존재 여부로만 gate했다. `srcblend=ZERO`, `dstblend=SRCCOLOR`인 곱셈 mask pass는 keyed texel을 버리면 mask가 무의미해지므로 게스트 상태 밖의 조건을 넣지 않았다.
- texture upload의 effective key 계산을 같은 판정으로 통일했다.
- `Blt` 경로가 컬러키를 alpha test로 흉내내던 설정을 제거했다. 같은 의미를 두 경로에서 한 방식으로 표현한다.
- `LateDraw` 진단에 게스트 `colorkey=`와 `alphatest=`를 추가했다. 기존 `key=` 필드는 surface에 키가 붙어 있는지만 뜻하고 그 draw에서 키가 적용되는지는 알려주지 않았다.

## 검증

- Windows x86 warnings-as-errors 전체 빌드 통과.
- Windows x64 warnings-as-errors 전체 빌드 통과.
- Windows x86 CTest 2/2 통과, Windows x64 CTest 1/1 통과.
- 이 변경은 backend shader 경계에 있어 공용 core 로직이 바뀌지 않았다. 기존 RGB565 컬러키 일치 단위 테스트를 그대로 사용한다.

## 남은 확인

로고 sprite의 검은 사각형이 사라지는지, 배경과 mask 계층 그리고 작업 073에서 정상화된 마스킹이 회귀하지 않는지는 사용자 detached 재실행으로 확인한다. 새 로그의 `colorkey=` 필드로 sprite pass와 mask pass의 게스트 상태를 구분해 기록한다.

---

# Color-key Discard Semantics Work Log

## Symptom

After Task 074 recovered `.str` scene scripts the Title screen renders. The background `bg.bmp`, `SE-logo.bmp`, and the copyright line are correct, but the EZ2DJ logo sprite keeps the original bitmap's black background as an opaque rectangle.

## Cause

`System\Title\logo.bmp` is a 24bpp 256×256 image whose background is pure black `(0, 0, 0)` — the sampled top colors are `(0,0,0)`, `(255,255,255)`, and `(27,27,27)` — and run log `20260827-020438-743.ddraw.log` reports the source key as `keylow=0x0000:keyhigh=0x0000`, so the key value is correct. The backend baked color keying into texture alpha at upload time, and the only branch that actually dropped such a texel was the shader's `u_alpha_test_enabled`, which comes from the guest's `D3DRENDERSTATE_ALPHATESTENABLE`. As the log's `alphatest=0` shows, the original never enables the alpha test on the Direct3D draw path, and its blend factors are `srcblend=2 (D3DBLEND_ONE)` and `dstblend=1 (D3DBLEND_ZERO)` — a plain copy in which alpha plays no part — so keyed texels were written out as black. The DirectDraw `Blt` path escaped this only because it set `color_key_enabled` and `alpha_test_enabled` to the same value; the defect belonged to the Direct3D `DrawPrimitive` path, which forwards both states exactly as the guest set them. In Direct3D, color keying is not alpha blending: while `D3DRENDERSTATE_COLORKEYENABLE` is set, key-matching texels are discarded regardless of the blend factors.

## Implementation

The fragment shader gains a `u_color_key_enabled` uniform and discards a texel whose alpha, before diffuse modulation, falls below the threshold while keying is active; the guest alpha-test branch keeps its own meaning separately. The discard is gated solely on `state.color_key_enabled` (the guest `COLORKEYENABLE`) together with the texture carrying a source key, with no condition outside the guest state, because the multiplicative mask passes using `srcblend=ZERO` and `dstblend=SRCCOLOR` would become meaningless if their keyed texels were dropped. Texture upload's effective-key computation now uses the same decision. The `Blt` path no longer borrows the alpha test to express color keying, so one meaning has one representation on both paths. Finally, `LateDraw` diagnostics report the guest `colorkey=` and `alphatest=` values, since the existing `key=` field says only whether a surface carries a key, not whether keying applies to that draw.

## Verification

Windows x86 and x64 warnings-as-errors builds pass, x86 CTest passes 2/2, and x64 CTest passes 1/1. The change sits at the backend shader boundary and alters no shared-core logic, so the existing RGB565 color-key match unit tests apply unchanged.

## Remaining

Whether the logo sprite's black rectangle is gone, and whether backgrounds, mask layers, and the masking corrected in Task 073 hold, must come from a detached user re-run. The new log's `colorkey=` field records the guest state separately for sprite and mask passes.
