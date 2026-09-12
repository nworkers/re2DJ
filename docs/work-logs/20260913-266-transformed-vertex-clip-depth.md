# 변환 정점 OpenGL 클립 깊이 보정 작업 로그

관련 설계: [변환 정점의 OpenGL 클립 깊이 보정 설계](../design/20260913-266-transformed-vertex-clip-depth.md)

관련 작업 지시서: [변환 정점 OpenGL 클립 깊이 보정 작업 지시](../work-orders/20260913-266-transformed-vertex-clip-depth.md)

## 결과

최신 complete diagnostic run `20260913-030626-082`를 기준으로 note·롱노트 미출력의 직접적인 HLE 후보를 SDL3/OpenGL transformed vertex clip 경계에서 수정했다. Note 및 `LONG_EFFECT` asset은 정상적으로 읽혔고 `DrawPrimitive`도 모두 API 성공으로 끝났지만, 일부 depth-disabled textured draw가 `0xCCCCCCCC` debug-fill에 해당하는 z를 OpenGL shader에 전달하고 있었다.

`ResolveLegacyClipDepth`를 공용 legacy graphics 계층에 추가하고 SDL3/OpenGL backend에서 사용하도록 연결했다. depth test가 꺼진 draw는 clip 깊이 `0.5`를 사용하며, depth test가 켜진 draw는 guest z를 그대로 유지한다. 따라서 OpenGL의 depth test가 아니라 clip-volume 검사 때문에 primitive가 사라지는 경로를 제거하면서, depth-ordered 장면의 guest 의미는 보존한다.

## 로그 근거

- VFS에서 `Note_white_0..5.abm`, `Note_BLUE_0..5.abm`, `Note_PEDAL_0..5.abm`, `LONG_EFFECT0..3.abm`의 open/read가 모두 성공했다.
- `DrawPrimitive` 297,628건이 모두 `reason=success`, `result=0x00000000`이었다.
- texture draw 11,755건이 `z=-107374176.000000`, `rhw=1.000000`, `zenable=0`을 사용했다.
- `-107374176.0f`는 정점 메모리의 `0xCCCCCCCC` debug-fill 값과 일치한다.
- 영향을 받은 texture ID는 `425..429`, `433..437`, `501`이며, draw 자체는 OpenGL 오류 없이 성공했다.

## 구현

- `include/re2dj/graphics/legacy_draw_command.h`: 중립 clip 깊이 상수와 `ResolveLegacyClipDepth` 계약을 추가했다.
- `src/graphics/legacy_draw_command.cpp`: depth state에 따른 깊이 선택 정책을 구현했다.
- `src/graphics/sdl3_opengl_backend.cpp`: 변환 정점의 OpenGL position z 생성에 정책을 적용했다.
- `tests/unit/legacy_draw_command_test.cpp`: depth-disabled 보정과 depth-enabled 입력 보존을 회귀 검증했다.
- `docs/analysis/ez2dj4th-graphics-path.md`, `ARCHITECTURE.md`: 확인됨/추정/미확정 상태와 backend 계약을 갱신했다.

## 검증

- `cmd /c scripts\build_win32.bat`: 통과.
- `re2dj_unit_tests.exe`: `checks: 1694, failures: 0`.
- `re2dj_ez2dj_keyboard_input_test.exe`: `checks: 7, failures: 0`.
- `re2dj_ez2dancer_keyboard_input_test.exe`: `checks: 8, failures: 0`.
- `re2dj_windows_product_loader_probe.exe`: profile defaults, second defaults, unsupported target, IAT slot resolution 모두 통과.
- `ctest --test-dir build/windows-x86 -C Debug -E re2dj_windows_vfs_runtime_probe --output-on-failure`: 4/4 통과.
- 전체 CTest는 `re2dj_windows_vfs_runtime_probe`가 172.63초 동안 lifecycle probe에서 응답하지 않아 중단되었고, 나머지 4개는 통과했다. 이 probe 정지는 이번 clip-depth 변경과 무관한 환경성 검증 항목으로 분리했다.
- Linux x64 빌드는 기존 CMake cache가 WSL 경로(`/mnt/e/...`)로 생성되어 Windows에서 재사용할 수 없었고, WSL service도 `E_ACCESSDENIED`로 시작되지 않아 실행하지 못했다.
- `git diff --check`: 통과.

## 남은 확인

수정된 실행 파일로 사용자가 실제 gameplay를 다시 수행하여 note와 롱노트가 화면에 복원되는지 확인해야 한다. 복원되지 않는 draw가 남으면 다음 complete log에서 texture ID, `zenable`, blend 상태와 실제 gameplay frame을 다시 상관시킨다.

---

# Transformed-Vertex OpenGL Clip-Depth Correction Work Log

Related design: [Transformed-Vertex OpenGL Clip-Depth Correction Design](../design/20260913-266-transformed-vertex-clip-depth.md)

Related work order: [Transformed-Vertex OpenGL Clip-Depth Correction Work Order](../work-orders/20260913-266-transformed-vertex-clip-depth.md)

## Result

Using complete diagnostic run `20260913-030626-082`, corrected the direct HLE candidate for missing notes and long notes at the SDL3/OpenGL transformed-vertex clip boundary. Note and `LONG_EFFECT` assets loaded successfully, and every `DrawPrimitive` completed successfully, but some depth-disabled textured draws passed a `0xCCCCCCCC` debug-fill-equivalent z into the OpenGL shader.

Added `ResolveLegacyClipDepth` to the shared legacy graphics layer and connected it to the SDL3/OpenGL backend. Depth-disabled draws use clip depth `0.5`, while depth-enabled draws preserve guest z. This removes the path where OpenGL clip-volume testing discards a primitive even though the depth test itself is disabled, while preserving guest semantics for depth-ordered scenes.

## Log evidence

- VFS open/read succeeded for `Note_white_0..5.abm`, `Note_BLUE_0..5.abm`, `Note_PEDAL_0..5.abm`, and `LONG_EFFECT0..3.abm`.
- All 297,628 `DrawPrimitive` records report `reason=success` and `result=0x00000000`.
- 11,755 textured draws use `z=-107374176.000000`, `rhw=1.000000`, and `zenable=0`.
- `-107374176.0f` matches the `0xCCCCCCCC` debug-fill value in transformed vertex memory.
- Affected texture IDs include `425..429`, `433..437`, and `501`; the draws complete without OpenGL errors.

## Implementation

- `include/re2dj/graphics/legacy_draw_command.h`: added the neutral clip-depth constant and `ResolveLegacyClipDepth` contract.
- `src/graphics/legacy_draw_command.cpp`: implemented depth-state-based depth selection.
- `src/graphics/sdl3_opengl_backend.cpp`: applied the policy when producing OpenGL transformed-vertex position z.
- `tests/unit/legacy_draw_command_test.cpp`: regression-tested depth-disabled correction and depth-enabled input preservation.
- Updated `docs/analysis/ez2dj4th-graphics-path.md` and `ARCHITECTURE.md` with the confirmed/inferred/unresolved status and backend contract.

## Verification

- `cmd /c scripts\build_win32.bat`: passed.
- `re2dj_unit_tests.exe`: `checks: 1694, failures: 0`.
- `re2dj_ez2dj_keyboard_input_test.exe`: `checks: 7, failures: 0`.
- `re2dj_ez2dancer_keyboard_input_test.exe`: `checks: 8, failures: 0`.
- `re2dj_windows_product_loader_probe.exe`: profile defaults, second defaults, unsupported target, and IAT slot resolution all passed.
- `ctest --test-dir build/windows-x86 -C Debug -E re2dj_windows_vfs_runtime_probe --output-on-failure`: 4/4 passed.
- Full CTest was stopped after `re2dj_windows_vfs_runtime_probe` remained unresponsive in its lifecycle probe for 172.63 seconds; the other four tests passed. This probe hang is recorded separately as an environmental verification item.
- The Linux x64 build could not run because its existing CMake cache was generated with WSL paths (`/mnt/e/...`) and could not be reused from Windows; the WSL service also failed to start with `E_ACCESSDENIED`.
- `git diff --check`: passed.

## Remaining verification

The user must replay actual gameplay with the corrected build and confirm that notes and long notes are visible. If any draw remains invisible, the next complete log should correlate texture ID, `zenable`, blend state, and the actual gameplay frame.
