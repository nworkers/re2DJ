# 20260905-194 텍스처 주소 모드 전달 작업 로그

관련 설계: [텍스처 주소 모드 전달 설계](../design/20260905-194-texture-address-forwarding.md)  
관련 작업 지시: [텍스처 주소 모드 전달 작업 지시서](../work-orders/20260905-194-texture-address-forwarding.md)

## 1. 작업 상태

**구현 및 정적 검증 완료, 사용자 화면 비교 대기.** 사용자의 `20260905-111600-576` 로그에서 게스트가 `D3DTADDRESS_WRAP`을 요청한 것이 확인되어, 해당 상태를 OpenGL sampler에 전달하도록 구현했다. 주소 모드 변경이 밝기 차이를 해결하는지는 아직 사용자 화면 비교로 검증하지 않았다.

**Implementation and static verification complete; user-visible comparison pending.** The user's `20260905-111600-576` log confirmed a guest `D3DTADDRESS_WRAP` request, so the state is now forwarded to the OpenGL sampler. Whether this removes the brightness mismatch still requires a user-visible comparison.

## 2. 구현

- `LegacyFixedFunctionState`에 `TextureAddressMode`와 U/V 필드를 추가했다.
- Direct3D facade에서 `WRAP`, `MIRROR`, `CLAMP`를 공용 상태로 변환한다.
- OpenGL backend에서 textured draw마다 `GL_TEXTURE_WRAP_S/T`를 현재 상태에 맞게 설정한다.
- `BORDER`, `MIRRORONCE`는 정확한 변환이 없어 unsupported 오류로 처리한다.
- alpha stage, color-key discard, 필터, blend 식과 원본 자산은 변경하지 않았다.

- Added `TextureAddressMode` and U/V fields to `LegacyFixedFunctionState`.
- The Direct3D facade converts `WRAP`, `MIRROR`, and `CLAMP` to the common state.
- The OpenGL backend sets `GL_TEXTURE_WRAP_S/T` per textured draw from the current state.
- `BORDER` and `MIRRORONCE` return an unsupported error because no exact conversion is available.
- Alpha stage, color-key discard, filtering, blend equations, and original assets were not changed.

## 3. 검증

- `cmd /c scripts\\build_win32.bat`: 통과.
- `build\\windows-x86\\bin\\Debug\\re2dj_unit_tests.exe`: `checks: 1265, failures: 0`.
- `ctest --test-dir build/windows-x86 -C Debug -R re2dj_unit_tests --output-on-failure`: 1/1 통과.
- `git diff --check`: 통과.

- `cmd /c scripts\\build_win32.bat`: passed.
- `build\\windows-x86\\bin\\Debug\\re2dj_unit_tests.exe`: `checks: 1265, failures: 0`.
- `ctest --test-dir build/windows-x86 -C Debug -R re2dj_unit_tests --output-on-failure`: 1/1 passed.
- `git diff --check`: passed.

## 4. 남은 확인

새 Debug build로 동일한 Music Select에 진입하여 중앙 artwork와 선택 링의 화면을 이전 실행과 비교한다. 주소 모드 전달 후에도 밝기 차이가 남으면 주소 모드는 원인에서 제외하고, 컬러키 텍스처의 선형 필터 경계와 RGB565/OpenGL framebuffer 정밀도를 다음 후보로 조사한다.

Enter the same Music Select screen with the new Debug build and compare the center artwork and selection ring with the previous run. If the brightness difference remains after forwarding addressing, remove addressing from the primary cause and investigate linear filtering at color-key boundaries and RGB565/OpenGL framebuffer precision next.
