# 작업 로그 197: Direct3D cull 상태 전달
# Work Log 197: Direct3D Cull-State Forwarding

관련 설계: [Direct3D cull 상태 전달 설계](../design/20260905-197-direct3d-cull-forwarding.md)  
관련 작업 지시: [작업 지시 197](../work-orders/20260905-197-direct3d-cull-forwarding.md)

## 결과 / Result

Music Select disc draw에서 확인된 `D3DRENDERSTATE_CULLMODE=3` (`D3DCULL_CCW`)을 공용 graphics state와 SDL3/OpenGL backend까지 전달하도록 구현했습니다. 사용자가 같은 실행 환경에서 화면을 비교할 수 있는 Debug build를 생성했습니다. 실제 화면 변화 여부는 사용자의 Music Select 진입 검증을 기다리고 있습니다.

*The observed `D3DRENDERSTATE_CULLMODE=3` (`D3DCULL_CCW`) from the Music Select disc draws is now forwarded through the shared graphics state into the SDL3/OpenGL backend. A Debug build is ready for the user to compare in the same execution environment. The actual visual effect remains pending the user's Music Select run.*

## 구현 내용 / Implementation

- `LegacyFixedFunctionState`에 `CullMode`를 추가했습니다.
- Direct3D facade에서 `D3DCULL_NONE`, `D3DCULL_CW`, `D3DCULL_CCW`를 명시적으로 변환하고, 알 수 없는 값은 draw 실패로 보고합니다.
- Direct3D device 기본 cull state를 `D3DCULL_CCW`로 초기화했습니다.
- OpenGL backend에서 `glCullFace`와 `glFrontFace`를 로드하고 triangle draw에 back-face culling을 적용했습니다.
- 현재 vertex shader의 guest top-left Y 변환을 고려하여 `D3DCULL_CW`는 `GL_CCW`, `D3DCULL_CCW`는 `GL_CW` front-face로 매핑했습니다.
- line draw와 Present copy에는 culling을 적용하지 않습니다.

*Implementation details:*

- *Added `CullMode` to `LegacyFixedFunctionState`.*
- *Explicitly convert `D3DCULL_NONE`, `D3DCULL_CW`, and `D3DCULL_CCW` in the Direct3D facade; unknown values fail the draw.*
- *Initialized the Direct3D device default cull state to `D3DCULL_CCW`.*
- *Loaded `glCullFace` and `glFrontFace` and applied back-face culling to triangle draws in the OpenGL backend.*
- *Mapped `D3DCULL_CW` to `GL_CCW` and `D3DCULL_CCW` to `GL_CW` front-face winding after accounting for the current vertex shader's guest top-left Y conversion.*
- *Disabled culling for line draws and the Present copy.*

## 검증 / Verification

- `cmd /c scripts\build_win32.bat`: 성공
- `build\windows-x86\bin\Debug\re2dj_unit_tests.exe`: `checks: 1265, failures: 0`
- `ctest --test-dir build/windows-x86 -C Debug -R re2dj_unit_tests --output-on-failure`: 1/1 성공
- `git diff --check`: 성공

*`cmd /c scripts\build_win32.bat` passed. The unit executable reported `checks: 1265, failures: 0`; CTest passed 1/1; and `git diff --check` passed.*

## 사용자 비교 / User Comparison

새 Debug build로 이전과 같은 한 줄 launcher 명령을 실행하고 Music Select까지 진입합니다. 중앙 artwork와 ring의 밝기·표시 상태가 바뀌는지 확인합니다. 변화가 있으면 culling이 과노출에 관여한 것으로 좁힐 수 있고, 변화가 없으면 color-key/linear-filter 경계 또는 Direct3D rasterization 차이를 계속 조사합니다.

*Run the same one-line launcher command with the new Debug build and enter Music Select. Check whether the center artwork and ring change in brightness or visibility. A change narrows the overexposure to culling; no change keeps the color-key/linear-filter boundary or Direct3D rasterization differences as the next candidates.*
