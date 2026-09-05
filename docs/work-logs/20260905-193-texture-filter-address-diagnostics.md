# 20260905-193 텍스처 필터·주소 모드 진단 작업 로그

관련 설계: [텍스처 필터·주소 모드 진단 설계](../design/20260905-193-texture-filter-address-diagnostics.md)  
관련 작업 지시: [텍스처 필터·주소 모드 진단 작업 지시서](../work-orders/20260905-193-texture-filter-address-diagnostics.md)

## 1. 작업 상태

**진단 구현 및 사용자 재실행 확인 완료.** alpha stage 가설을 후속 후보인 컬러키·필터·주소 모드와 분리하기 위해 draw record에 stage-0 필터와 주소 모드를 추가했고, 사용자의 후속 Music Select 실행에서 값을 확인했다. OpenGL sampler와 shader 의미는 아직 변경하지 않았다.

**Diagnostics implementation and user rerun confirmed.** Stage-0 filter and address modes were added to the draw record to separate the rejected alpha-stage hypothesis from the remaining color-key, filtering, and addressing candidates, and the values were confirmed in the user's follow-up Music Select run. OpenGL sampler and shader semantics were not changed.

## 2. 구현

- `LateDraw`에 `minfilter`, `magfilter`, `addressu`, `addressv`를 추가했다.
- Direct3D 기본 texture address mode인 `D3DTADDRESS_WRAP`를 stage 0의 U/V 초기값으로 설정했다.
- 기존 alpha stage, blend, color-key, texture content summary와 bounded trace budget은 유지했다.

- Added `minfilter`, `magfilter`, `addressu`, and `addressv` to `LateDraw`.
- Initialized stage-0 U/V address modes to `D3DTADDRESS_WRAP`, the Direct3D default.
- Preserved the existing alpha-stage, blend, color-key, texture-content summary, and bounded trace budget.

## 3. 기존 사용자 실행과 코드 대조

사용자 실행 `20260905-104129-239`의 기존 `DrawPrimitive` 기록에서 중앙 artwork `texture=387`과 선택 링 `texture=279`는 모두 `minfilter=2`, `magfilter=2`였다. 중앙 artwork는 `colorkey=1`, `srcblend=2`, `dstblend=2`, `alphaop=4`, `alphaarg1=2`, `alphaarg2=0`이었다.

후속 실행 `20260905-111600-576`의 frame `1040`에서 중앙 artwork `texture=387`과 선택 링 `texture=279` 모두 `minfilter=2`, `magfilter=2`, `addressu=1`, `addressv=1`을 기록했다. 중앙 artwork는 `colorkey=1`, `srcblend=2`, `dstblend=2`였다.

현재 backend는 텍스처 생성 시 `GL_TEXTURE_WRAP_S/T`를 `GL_CLAMP_TO_EDGE`로 설정하고, 진단 전 `LegacyFixedFunctionState`에는 주소 모드가 없어서 게스트의 `ADDRESSU/V`를 전달하지 않았다. 따라서 게스트의 `D3DTADDRESS_WRAP`와 backend의 clamp 차이가 확인되었다. 이 차이가 화면의 밝기 차이를 일으키는지는 아직 확정하지 않는다.

In the user's `20260905-104129-239` run, the existing `DrawPrimitive` records for center artwork `texture=387` and selection ring `texture=279` both used `minfilter=2` and `magfilter=2`. The center artwork used `colorkey=1`, `srcblend=2`, `dstblend=2`, `alphaop=4`, `alphaarg1=2`, and `alphaarg2=0`.

Follow-up run `20260905-111600-576` recorded `minfilter=2`, `magfilter=2`, `addressu=1`, and `addressv=1` for both center artwork `texture=387` and selection ring `texture=279` in frame `1040`. The center artwork used `colorkey=1`, `srcblend=2`, and `dstblend=2`.

The current backend sets `GL_TEXTURE_WRAP_S/T` to `GL_CLAMP_TO_EDGE` when creating a texture, and before this diagnostic `LegacyFixedFunctionState` had no address-mode fields, so guest `ADDRESSU/V` were not forwarded. The guest's `D3DTADDRESS_WRAP` request therefore differs from the backend's clamp mode. This does not yet establish that the difference causes the screen brightness mismatch.

## 4. 검증

- `cmd /c scripts\\build_win32.bat`: 통과.
- `build\\windows-x86\\bin\\Debug\\re2dj_unit_tests.exe`: `checks: 1265, failures: 0`.
- `ctest --test-dir build/windows-x86 -C Debug -R re2dj_unit_tests --output-on-failure`: 1/1 통과.
- `git diff --check`: 통과.

- `cmd /c scripts\\build_win32.bat`: passed.
- `build\\windows-x86\\bin\\Debug\\re2dj_unit_tests.exe`: `checks: 1265, failures: 0`.
- `ctest --test-dir build/windows-x86 -C Debug -R re2dj_unit_tests --output-on-failure`: 1/1 passed.
- `git diff --check`: passed.

## 5. 결론

`addressu/addressv=1`과 `minfilter/magfilter=2`가 실제 Music Select draw에서 확인되었다. 따라서 backend의 고정 `GL_CLAMP_TO_EDGE`는 게스트가 요청한 기본 `WRAP`와 다른 실제 구현 차이이며, 주소 모드 전달을 후속 수정 대상으로 승격한다. 컬러키 draw의 선형 필터 경계 동작과 전체 밝기 영향은 주소 모드 수정 후 별도로 판정한다.

The actual Music Select draws confirmed `addressu/addressv=1` and `minfilter/magfilter=2`. The backend's fixed `GL_CLAMP_TO_EDGE` is therefore a real implementation difference from the guest's default `WRAP` request, and address forwarding is promoted to the next implementation task. The linear-filter boundary behavior of color-keyed draws and its overall brightness impact will be judged separately after address forwarding.
