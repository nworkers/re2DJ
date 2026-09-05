# 20260905-194 텍스처 주소 모드 전달 작업 지시서

## 목표

원본 Direct3D가 요청한 stage-0 `ADDRESSU/V`를 OpenGL sampler에 전달하여, `WRAP` 요청을 현재의 고정 `CLAMP_TO_EDGE`와 다르게 적용한다.

Forward the original Direct3D stage-0 `ADDRESSU/V` request to the OpenGL sampler so a `WRAP` request is not treated as the currently fixed `CLAMP_TO_EDGE` mode.

## 작업 항목

1. 공용 `LegacyFixedFunctionState`에 U/V 주소 모드 enum과 필드를 추가한다.
2. Direct3D facade에서 `D3DTADDRESS_WRAP`, `MIRROR`, `CLAMP`를 변환한다.
3. OpenGL backend에서 draw별 `GL_TEXTURE_WRAP_S/T`를 적용한다.
4. 정확히 지원할 수 없는 주소 모드는 명시적인 unsupported 오류로 처리한다.
5. 기존 color-key, filter, blend, shader 동작을 변경하지 않는다.
6. Windows x86 Debug 빌드와 단위 테스트를 실행한다.

## 완료 조건

- Music Select trace의 `addressu/addressv=1`이 OpenGL `GL_REPEAT`로 전달된다.
- 지원하는 주소 모드의 변환이 컴파일되고 테스트된다.
- 빌드·단위 테스트와 후속 사용자 화면 확인 결과가 작업 로그에 남는다.

---

# 20260905-194 Texture-Address Forwarding Work Order

## Goal

Forward the original Direct3D stage-0 `ADDRESSU/V` request to the OpenGL sampler so a `WRAP` request is not treated as the currently fixed `CLAMP_TO_EDGE` mode.

## Work items

1. Add U/V address-mode enum values and fields to `LegacyFixedFunctionState`.
2. Convert `D3DTADDRESS_WRAP`, `MIRROR`, and `CLAMP` in the Direct3D facade.
3. Apply per-draw `GL_TEXTURE_WRAP_S/T` in the OpenGL backend.
4. Return an explicit unsupported error for address modes that cannot be represented exactly.
5. Preserve color-key, filtering, blending, and shader behavior.
6. Run the Windows x86 Debug build and unit tests.

## Completion criteria

- Music Select trace values `addressu/addressv=1` are forwarded as OpenGL `GL_REPEAT`.
- Supported address-mode mappings compile and are tested.
- Build, unit-test, and follow-up user-visible verification results are recorded in the work log.
