# 작업 지시서: ez2dj3rd 윈도우 모드 프라이머리 서피스 Blt 프레임 프리젠테이션 지원

## 한국어

설계: [ez2dj3rd 윈도우 모드 프라이머리 서피스 Blt 프레임 프리젠테이션 설계](../design/20260906-217-ez2dj3rd-windowed-primary-blt-present.md)

### 목표

`ez2dj3rd`가 소리만 나오고 화면이 검은 상태로 지속되던 문제를 해결하기 위해, `BuildSurfaceRectangle`의 프라이머리 서피스 대상 화면 좌표계 사각형 수용과 `SurfaceBlt`의 프라이머리 서피스 대상 프리젠테이션(`Present`) 처리를 구현하여 윈도우 모드에서 렌더링된 Direct3D3 화면이 정상 출력되도록 합니다.

### 작업 항목

1. `src/platform/windows/direct3d3_com_facade.cpp`의 `BuildSurfaceRectangle`을 수정하여 `(surface.capabilities & DDSCAPS_PRIMARYSURFACE) != 0`일 때 게스트가 전달한 화면 좌표계 사각형(`right > left`, `bottom > top`)을 `DDERR_INVALIDRECT`로 거부하지 않고 정규화된 크기(`width = right - left`, `height = bottom - top`)로 수용하도록 구현합니다.
2. `src/platform/windows/direct3d3_com_facade.cpp`의 `SurfaceBlt`에서 대상 서피스가 `DDSCAPS_PRIMARYSURFACE`인 경우 윈도우 모드 프레임 프리젠테이션으로 처리하도록 구현합니다:
   - `surface->root->render_backend->Present(&error)`를 호출하여 백엔드 버퍼를 화면에 스왑 표시.
   - `Re2djExitIfWindowClosed(surface->root->window)` 호출로 윈도우 종료 처리.
   - `++surface->root->frame_number;` 증가 및 `RecordPresentedFrame(surface->root);` 호출로 FPS 측정 연동.
   - `DD_OK`(`0x00000000`) 반환.
3. Windows x86 Release 구성을 빌드하고 `re2dj_unit_tests` 및 `re2dj_windows_product_loader_probe` 단위 테스트를 실행하여 정상 통과를 확인합니다.
4. `.\build\windows-x86\bin\Release\re2dj.exe ez2dj3rd --io-config .\config\ez2dj-io.example.ini`를 실행하여 윈도우 창에 게임 영상(타이틀 화면, 데모 화면 등)이 사운드와 함께 정상 렌더링되는지 확인합니다.
5. 작업 로그 `docs/work-logs/20260906-217-ez2dj3rd-windowed-primary-blt-present.md`를 작성하고 커밋합니다.

### 제외 범위

- `config/ez2dj-io.example.ini` 사용자 수정 파일의 변경 또는 커밋
- 원본 CHD/HDD 자산의 저장소 커밋

### 완료 조건

- 단위 테스트 `re2dj_unit_tests`와 `re2dj_windows_product_loader_probe`가 성공합니다.
- `ez2dj3rd` 실행 시 화면에 그래픽이 표시되고 `ddraw.log`의 `Blt` 결과가 `DD_OK`(`0x00000000`)로 기록됩니다.
- 작업 로그가 작성되고 변경 사항이 Git 커밋으로 기록됩니다.

---

## English

Design: [ez2dj3rd Windowed Primary Surface Blt Frame Presentation Design](../design/20260906-217-ez2dj3rd-windowed-primary-blt-present.md)

### Objective

Resolve the issue where `ez2dj3rd` plays audio but displays a black screen by extending `BuildSurfaceRectangle` to accept screen-space rectangles on primary surfaces and implementing render presentation (`Present`) in `SurfaceBlt` for primary surface targets, allowing windowed Direct3D3 graphics to display properly.

### Work items

1. Update `BuildSurfaceRectangle` in `src/platform/windows/direct3d3_com_facade.cpp` so that when `(surface.capabilities & DDSCAPS_PRIMARYSURFACE) != 0`, screen-space rectangles (`right > left`, `bottom > top`) are accepted rather than rejected with `DDERR_INVALIDRECT`, computing normalized size (`width = right - left`, `height = bottom - top`).
2. Update `SurfaceBlt` in `src/platform/windows/direct3d3_com_facade.cpp` to treat blits targeting `DDSCAPS_PRIMARYSURFACE` as windowed frame presentations:
   - Call `surface->root->render_backend->Present(&error)` to swap the rendered backend buffer to the display.
   - Call `Re2djExitIfWindowClosed(surface->root->window)` to handle window close events.
   - Increment `++surface->root->frame_number;` and call `RecordPresentedFrame(surface->root);`.
   - Return `DD_OK` (`0x00000000`).
3. Build Windows x86 Release configuration and run `re2dj_unit_tests` and `re2dj_windows_product_loader_probe`.
4. Run `.\build\windows-x86\bin\Release\re2dj.exe ez2dj3rd --io-config .\config\ez2dj-io.example.ini` and verify that game visuals are rendered on screen alongside working sound.
5. Write work log `docs/work-logs/20260906-217-ez2dj3rd-windowed-primary-blt-present.md` and commit changes.

### Out of scope

- Modifying or committing the user-modified `config/ez2dj-io.example.ini`.
- Committing original CHD/HDD assets to the repository.

### Completion criteria

- All unit tests pass (`re2dj_unit_tests`, `re2dj_windows_product_loader_probe`).
- Graphics render on screen when running `ez2dj3rd`, and `ddraw.log` records `DD_OK` (`0x00000000`) for `Blt`.
- Work log is written and changes are committed to Git.
