# 목적지 알파 블렌드 지원 작업 로그

## 결과

`ez2dj4th`에서 원본이 요청한 `D3DBLEND_DESTCOLOR(9)` + `D3DBLEND_INVDESTALPHA(8)` draw가 HLE facade에서 사전 거부되던 문제를 수정했습니다. raw ABI 7과 8을 공용 fixed-function state로 보존하고 SDL3/OpenGL backend의 blend 함수로 전달합니다.

이번 변경은 profile, launcher 설정, VFS, CHD 입력, note 판정 로직을 수정하지 않았습니다. 원본 CHD는 계속 read-only로 사용했습니다.

## 변경 사항

- `BlendFactor`에 `kDestinationAlpha`, `kInverseDestinationAlpha`를 추가했습니다.
- `DecodeLegacyBlendFactor`가 raw D3D ABI 7과 8을 각각 새 enum으로 변환하도록 했습니다.
- SDL3/OpenGL backend가 새 enum을 `GL_DST_ALPHA`, `GL_ONE_MINUS_DST_ALPHA`로 매핑하도록 했습니다.
- decoder 단위 테스트에 두 ABI 값을 추가했습니다.
- OpenGL blend probe에 9/8 조합의 실제 픽셀 검사를 추가했습니다.
- 설계, 작업 지시, graphics-path 분석, KB, `ARCHITECTURE.md`, 구현 완료 색인을 갱신했습니다.

## 검증

1. `cmake --build build/windows-x86 --config Debug --target re2dj_unit_tests re2dj_opengl_blend_probe` 성공.
2. `cmake --build build/windows-x86 --config Debug --target re2dj_windows_x86_launcher_probe` 성공.
3. `cmake --build build/windows-x86 --config Debug --target re2dj_windows_injected_runtime` 성공.
4. `build\\windows-x86\\bin\\Debug\\re2dj_unit_tests.exe`: 1,692 checks, failures 0.
5. `build\\windows-x86\\bin\\Debug\\re2dj_opengl_blend_probe.exe`: pixel checks 10, failures 0.
6. `ctest --test-dir build/windows-x86 -C Debug --output-on-failure -E re2dj_windows_vfs_runtime_probe`: 4/4 통과.
7. 갱신 runtime으로 실행한 진단 `20260913-020714-976`에서 `DrawPrimitive` 1,247건, `draw_errors=0`, `unsupported_blend=0`을 확인했습니다. VFS log의 `read-file-result` 445건도 모두 `ok=1`이었습니다. 진단은 이 상태를 확인한 뒤 수동 종료했습니다.
8. 기준 실행 `20260913-013726-216`의 unsupported blend 66건과 비교해 해당 HLE 거부 경로가 제거되었습니다.

개별 note가 실제 게임플레이 화면에서 보이는지에 대한 사용자 시각 검증은 별도 확인 항목으로 남깁니다. 이번 작업으로 확인된 것은 note texture read 실패가 아니라 draw를 backend에 전달하지 못하던 HLE 경계의 직접 원인이 제거되었다는 점입니다.

## English

# Destination-Alpha Blend Support Work Log

## Result

Fixed the HLE facade rejection of the original `ez2dj4th` draw using `D3DBLEND_DESTCOLOR(9)` plus `D3DBLEND_INVDESTALPHA(8)`. Raw ABI values 7 and 8 are now preserved in the shared fixed-function state and forwarded to the SDL3/OpenGL backend.

This change did not modify profile settings, launcher configuration, VFS, CHD input, or note-judgment logic. The original CHD remained read-only.

## Changes

- Added `kDestinationAlpha` and `kInverseDestinationAlpha` to `BlendFactor`.
- Made `DecodeLegacyBlendFactor` decode raw D3D ABI values 7 and 8 into the new enum values.
- Mapped the new enum values to `GL_DST_ALPHA` and `GL_ONE_MINUS_DST_ALPHA` in the SDL3/OpenGL backend.
- Added both ABI values to decoder unit coverage.
- Added an actual-pixel 9/8 regression check to the OpenGL blend probe.
- Updated the design, work order, graphics-path analysis, KB, `ARCHITECTURE.md`, and implementation index.

## Verification

1. `cmake --build build/windows-x86 --config Debug --target re2dj_unit_tests re2dj_opengl_blend_probe` succeeded.
2. `cmake --build build/windows-x86 --config Debug --target re2dj_windows_x86_launcher_probe` succeeded.
3. `cmake --build build/windows-x86 --config Debug --target re2dj_windows_injected_runtime` succeeded.
4. `build\\windows-x86\\bin\\Debug\\re2dj_unit_tests.exe`: 1,692 checks, zero failures.
5. `build\\windows-x86\\bin\\Debug\\re2dj_opengl_blend_probe.exe`: 10 pixel checks, zero failures.
6. `ctest --test-dir build/windows-x86 -C Debug --output-on-failure -E re2dj_windows_vfs_runtime_probe`: 4/4 passed.
7. Diagnostic run `20260913-020714-976` with the rebuilt runtime processed 1,247 `DrawPrimitive` records with `draw_errors=0` and `unsupported_blend=0`. Its VFS log contained 445 `read-file-result` records, all with `ok=1`. The diagnostic was stopped manually after this stable observation.
8. Compared with the baseline run `20260913-013726-216`, which had 66 unsupported-blend records, the affected HLE rejection path is gone.

User visual verification that individual notes appear during actual gameplay remains a separate follow-up. This task establishes that the direct HLE boundary failure was removed; it does not claim a visual gameplay capture was completed.
