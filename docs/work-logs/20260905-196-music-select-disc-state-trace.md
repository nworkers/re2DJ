# 작업 로그 196: Music Select 원판 상태 추적

# Work Log 196: Music Select Disc-State Trace

## 결과 / Result

사용자 최신 실행 `20260905-164819-234`를 재검토하여, 원판 핵심 draw(`texture=279`, `texture=387`)의 raw cull mode를 확인했습니다. 두 draw 모두 `D3DCULL_CCW(3)`이며, OpenGL backend는 아직 culling을 적용하지 않습니다. 이는 구현 차이지만 원판 quad가 원본에서도 보이므로 이번 과노출의 직접 원인인지는 미확정입니다.

일반 late-draw 진단은 frame 1475에 도달하기 전에 상한에 도달하므로, 확인된 두 texture identity에만 적용되는 별도 bounded diagnostic을 추가했습니다. 새 기록은 정점별 위치/UV/diffuse, raw cull/blend/depth, stage-0 texture-coordinate index/transform flags 및 texture-0 matrix를 포함합니다.

*The latest user run `20260905-164819-234` was rechecked. Both core disc draws (`texture=279` and `texture=387`) use raw `D3DCULL_CCW(3)`, while the OpenGL backend does not yet apply culling. This is an implementation difference, but because the disc quads are visible in the original, its role in the overexposure remains unresolved.*

*Because the ordinary late-draw diagnostic limit was reached before the relevant frame, a separate bounded diagnostic was added for the two confirmed texture identities. The new run recorded 2048 entries with per-vertex position/UV/diffuse, raw cull/blend/depth state, stage-0 texture-coordinate index/transform flags, and the texture-0 matrix. The values show default UV processing and no Z involvement.*

## 검증 / Verification

- `cmd /c scripts\build_win32.bat` 통과
- `build\windows-x86\bin\Debug\re2dj_unit_tests.exe`: `checks: 1265, failures: 0`
- `ctest --test-dir build\windows-x86 -C Debug -R re2dj_unit_tests --output-on-failure`: 1/1 통과
- `git diff --check` 통과

*The Win32 build, unit tests, CTest, and whitespace checks passed.*

## 후속 / Follow-up

새 로그에서 texture transform과 정점 UV/diffuse는 정상적인 기본값으로 확인되었습니다. 다음 후보는 culling을 backend에 정확히 전달하는 구현 실험과 color-key + linear filtering 경계 동작입니다. culling은 먼저 동일 winding quad가 유지되는지 확인하면서 적용해야 합니다.

*The new log confirms default texture transform processing and expected vertex UV/diffuse values. The next candidates are an implementation experiment that forwards culling accurately and the color-key plus linear-filter boundary behavior. Culling should be applied while verifying that the same-winding quads remain visible.*
