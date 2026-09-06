# 작업 로그: ez2dj3rd 윈도우 모드 프라이머리 서피스 Blt 프레임 프리젠테이션 지원

## 한국어

### 작업 요약

`ez2dj3rd` 실행 시 사운드는 정상 출력되지만 화면이 검은 상태로 지속되던 문제를 조사하여 원인을 규명하고, 윈도우 모드 프라이머리 서피스 대상 `Blt` 화면 좌표계 수용 및 백엔드 프레임 프리젠테이션(`Present`) 호출을 구현하여 호스트 창에 정상적으로 60 FPS 화면이 출력되도록 해결했습니다.

### 원인 분석

1. **`BuildSurfaceRectangle`의 화면 좌표계 거부 (`DDERR_INVALIDRECT`)**:
   - EZ2DJ 3rd Trax는 윈도우 모드로 실행되며, 매 프레임 `GetClientRect`와 `ClientToScreen`을 통해 데스크톱 화면 좌표계 기준 윈도우 사각형(`dstrect = {640, 227, 1920, 1187}`)을 `PrimarySurface->Blt(...)`의 대상 영역으로 전달합니다.
   - 기존 `BuildSurfaceRectangle`은 `surface.width`(640)와 `surface.height`(480) 경계를 벗어나는 사각형을 무조건 거부하므로, `dstrect.right`(1920) > 640 조건에 걸려 매 프레임의 모든 `Blt` 호출이 `DDERR_INVALIDRECT`(`0x88760096`)로 실패했습니다.

2. **윈도우 모드 `Present` 호출 경로 부재**:
   - `ez2dj3rd`는 매 프레임 `IDirect3DDevice3::DrawPrimitive`를 통해 수천 개의 3D 프리미티브를 HLE OpenGL FBO에 성공적으로 렌더링하고 있었습니다.
   - 그러나 호스트 SDL 윈도우로 버퍼 스왑을 수행하는 `render_backend->Present(...)` 호출은 풀스크린 전용인 `SurfaceFlip` 내부에만 존재했습니다. 윈도우 모드는 DirectDraw 사양상 `Flip`을 호출하지 않고 `PrimarySurface->Blt`를 프레임 프리젠테이션으로 사용하므로, `Present`가 한 번도 호출되지 않아 화면이 갱신되지 못했습니다.

### 적용된 변경 사항

1. **`BuildSurfaceRectangle` 프라이머리 서피스 화면 좌표계 수용**:
   - `src/platform/windows/direct3d3_com_facade.cpp`: 서피스가 `DDSCAPS_PRIMARYSURFACE`인 경우, `input != nullptr`일 때 `input->right > input->left` 및 `input->bottom > input->top`을 만족하면 거부하지 않고 정규화된 크기(`width = right - left`, `height = bottom - top`)의 사각형으로 수용하도록 수정했습니다.

2. **`SurfaceBlt` 및 `SurfaceBltFast` 프라이머리 서피스 프리젠테이션 구현**:
   - `src/platform/windows/direct3d3_com_facade.cpp`: 대상 서피스가 `DDSCAPS_PRIMARYSURFACE`인 경우 윈도우 프레임 프리젠테이션으로 처리하도록 확장했습니다.
   - `surface->root->render_backend->Present(&error)`를 호출하여 OpenGL 백엔드 버퍼를 호스트 SDL 창에 표시.
   - `Re2djExitIfWindowClosed(surface->root->window)`를 호출하여 창 닫기 이벤트 반영.
   - `++surface->root->frame_number;` 증가 및 `RecordPresentedFrame(surface->root);`를 호출하여 초당 프레임 수(FPS) 측정 및 창 제목 갱신 연동.
   - `DD_OK`(`0x00000000`)를 반환하도록 구현.

### 검증 결과

1. **단위 테스트**:
   - `re2dj_unit_tests.exe`: 1,379 checks, 0 failures (100% 통과).
   - `re2dj_windows_product_loader_probe.exe`: 100% 통과.
2. **실행 검증**:
   - `.\build\windows-x86\bin\Release\re2dj.exe ez2dj3rd --io-config .\config\ez2dj-io.example.ini` 실행.
   - `logs/windows_x86_launcher_probe/ez2dj3rd/20260906-224726-071.ddraw.log`에서 모든 `Blt` 호출이 `result=0x00000000` (`DD_OK`)를 반환하고, `frame=1`, `frame=2`, ..., `frame=256`으로 매 프레임 정상 증가함을 확인했습니다.
   - `window-caption:event=title-update` 로그가 1초마다 지속적으로 발생하여 `RecordPresentedFrame`에 의해 안정적인 60 FPS로 호스트 창에 화면이 정상 프리젠테이션됨을 확인했습니다.

---

## English

### Summary

Investigated and resolved the issue where `ez2dj3rd` produced audio but displayed a black window by accepting screen-space coordinates on primary surface blits and implementing backend buffer presentation (`Present`) inside `SurfaceBlt`, properly displaying game visuals at a stable 60 FPS.

### Root Cause Analysis

1. **Screen-Space Coordinate Rejection in `BuildSurfaceRectangle` (`DDERR_INVALIDRECT`)**:
   - EZ2DJ 3rd Trax runs in windowed mode and every frame passes its window bounds in desktop screen coordinates (`dstrect = {640, 227, 1920, 1187}`) obtained via `GetClientRect` and `ClientToScreen` to `PrimarySurface->Blt(...)`.
   - `BuildSurfaceRectangle` strictly rejected any rectangle exceeding `surface.width` (640) or `surface.height` (480), causing all frame presentation `Blt` calls to fail with `DDERR_INVALIDRECT` (`0x88760096`).

2. **Missing `Present` Call Path for Windowed Mode**:
   - `ez2dj3rd` successfully rendered all 3D primitives to the OpenGL FBO via `IDirect3DDevice3::DrawPrimitive`.
   - However, the `render_backend->Present(...)` call that performs buffer swapping to the host window was only present inside `SurfaceFlip`. Because windowed mode in DirectDraw presents via `PrimarySurface->Blt` rather than `Flip`, `Present` was never invoked, leaving the display window blank.

### Applied Changes

1. **Screen-Space Coordinate Support for Primary Surfaces in `BuildSurfaceRectangle`**:
   - `src/platform/windows/direct3d3_com_facade.cpp`: Updated to accept valid non-empty rectangles (`right > left`, `bottom > top`) when the target has `DDSCAPS_PRIMARYSURFACE`, computing normalized dimensions (`width = right - left`, `height = bottom - top`).

2. **Windowed Frame Presentation in `SurfaceBlt` and `SurfaceBltFast`**:
   - `src/platform/windows/direct3d3_com_facade.cpp`: Added presentation handling when the destination surface has `DDSCAPS_PRIMARYSURFACE`.
   - Invoked `surface->root->render_backend->Present(&error)` to swap the rendered OpenGL FBO to the host SDL window.
   - Handled window close checking via `Re2djExitIfWindowClosed(surface->root->window)`.
   - Incremented `++surface->root->frame_number;` and invoked `RecordPresentedFrame(surface->root);` to drive FPS measurement and window title updates.
   - Returned `DD_OK` (`0x00000000`).

### Verification Results

1. **Unit Tests**:
   - `re2dj_unit_tests.exe`: 1,379 checks, 0 failures (100% passed).
   - `re2dj_windows_product_loader_probe.exe`: 100% passed.
2. **Runtime Verification**:
   - Executed `.\build\windows-x86\bin\Release\re2dj.exe ez2dj3rd --io-config .\config\ez2dj-io.example.ini`.
   - In `logs/windows_x86_launcher_probe/ez2dj3rd/20260906-224726-071.ddraw.log`, confirmed that all `Blt` calls returned `result=0x00000000` (`DD_OK`) and `frame` numbers advanced sequentially (`frame=1` through `frame=256`).
   - Verified repeated `window-caption:event=title-update` events, confirming active 60 FPS presentation to the host window.
