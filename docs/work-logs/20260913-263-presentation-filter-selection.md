# 정수 배율 화면 필터 선택 작업 로그

## 결과

최종 SDL3/OpenGL presentation sampler가 aspect-fit viewport의 배율에 따라 자동 선택되도록 구현했습니다. 논리 해상도와 출력 viewport가 같은 양의 정수 배수이면 `GL_NEAREST`, 그 외에는 `GL_LINEAR`를 사용합니다. 개별 Direct3D 텍스처의 필터 상태와 RGB565 논리 렌더 타깃은 변경하지 않았습니다.

## 변경 사항

- `PresentationFilter`와 순수 배율 판정 helper를 `re2dj_legacy_graphics`에 추가했습니다.
- 640×480, 1280×960, 1920×1440, 2560×1920에 nearest를 선택하는 단위 테스트를 추가했습니다.
- 1280×720, 1920×1080, 960×720과 잘못된 크기 입력에 linear를 선택하는 단위 테스트를 추가했습니다.
- SDL3/OpenGL `Present`가 최종 FBO texture의 min/mag filter를 helper 결과로 설정하도록 연결했습니다.

## 검증

- `powershell -ExecutionPolicy Bypass -File scripts/build_release.ps1`: Release 빌드 성공. 기존 스크립트의 CTest 경로가 `build/windows-x86-debug`를 가리켜 마지막 테스트 단계는 경로 오류로 종료되었습니다.
- `ctest --test-dir build/windows-x86 -C Release --output-on-failure -E re2dj_windows_vfs_runtime_probe`: 4/4 통과.
- 새 `re2dj_unit_tests`에서 presentation filter 테스트를 포함해 통과했습니다.
- `git diff --check`: 통과.
- 실제 원본 자산 실행 화면에서의 선명도 차이는 사용자 재검증이 필요합니다.

## English

# Integer-Scale Presentation Filter Selection Work Log

## Result

The final SDL3/OpenGL presentation sampler now selects automatically from the aspect-fit viewport scale. It uses `GL_NEAREST` when the viewport is the same positive integer multiple of the logical resolution and `GL_LINEAR` otherwise. Individual Direct3D texture filters and the RGB565 logical render target remain unchanged.

## Changes

- Added the pure `PresentationFilter` scale-selection helper to `re2dj_legacy_graphics`.
- Added unit coverage for nearest at 640×480, 1280×960, 1920×1440, and 2560×1920.
- Added unit coverage for linear at 1280×720, 1920×1080, 960×720, and invalid-size input.
- Connected SDL3/OpenGL `Present` so the final FBO texture minification and magnification filters follow the helper result.

## Verification

- `powershell -ExecutionPolicy Bypass -File scripts/build_release.ps1`: Release build succeeded. The existing script's CTest path points to `build/windows-x86-debug`, so its final test step ended with a path error.
- `ctest --test-dir build/windows-x86 -C Release --output-on-failure -E re2dj_windows_vfs_runtime_probe`: 4/4 passed.
- The new presentation-filter tests passed within `re2dj_unit_tests`.
- `git diff --check`: passed.
- Sharpness differences in a real run with original assets still require user revalidation.
