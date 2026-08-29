# 작업 091: Win32 실행 창 제목과 기본 2배 확대

## 한국어

### 목표

[설계 091](../design/20260829-091-window-title-default-scale.md)에 따라 Win32 창 제목에 version, build date, renderer, FPS를 표시하고 기본 windowed 클라이언트 크기를 1280×960으로 변경한다.

### 작업 항목

1. injected runtime target에 `RE2DJ_VERSION` 빌드 정의를 전달한다.
2. `window_mode`에 일관된 제목 생성·갱신 함수를 추가한다.
3. DirectDraw의 성공한 `Flip`을 약 1초 단위로 측정하여 FPS 제목을 갱신한다.
4. windowed 크기 계산에 가로·세로 2배 배율과 overflow 검사를 적용한다.
5. runtime probe의 제목, FPS 형식, 1280×960 client 크기 검증을 갱신한다.
6. `ARCHITECTURE.md`와 대응 작업 로그를 갱신한다.
7. Windows x86 Debug/Release build와 CTest를 수행한다.
8. 원본 popup WndProc가 소비할 수 있는 caption text/non-client 메시지를 host adapter에서 기본 처리한다.
9. windowed style을 표준 resize/maximize 가능한 `WS_OVERLAPPEDWINDOW`로 보정한다.
10. hostile WndProc를 사용하는 runtime probe로 제목 저장, caption hit-test와 표준 style을 검증한다.
11. host 제목과 icon 설정 뒤 들어오는 일반 `WM_SETTEXT`/`WM_SETICON` 변경을 차단한다.
12. 빈 제목·null icon overwrite 회귀 검증을 runtime probe에 추가한다.
13. 실제 제품에서 host 직접 그리기와 DWM non-client 정책을 대조해 caption 소거 원인을 확정한다.
14. windowed에서 `DWMNCRP_DISABLED`, fullscreen에서 `DWMNCRP_ENABLED`를 적용한다.
15. 저장 caption 상태와 WndProc를 bounded graphics trace에 추가한다.

### 완료 조건

- 기본 windowed 실행이 1280×960 client 영역으로 열린다.
- 제목이 설계 형식을 따르고 실행 중 FPS가 갱신된다.
- 원본 640×480 display mode와 fullscreen monitor-bounds 동작은 유지된다.
- 관련 자동 검증이 통과한다.
- 실제 원본 WndProc 동작과 무관하게 제목 문자열과 표준 caption 상호작용이 유지된다.

## English

### Goal

Following [Design 091](../design/20260829-091-window-title-default-scale.md), show version, build date, renderer, and FPS in the Win32 title and change the default windowed client size to 1280x960.

### Work items

1. Pass the `RE2DJ_VERSION` build definition to the injected runtime target.
2. Add consistent title construction and update functions to `window_mode`.
3. Measure successful DirectDraw `Flip` calls over roughly one-second intervals and update the FPS title.
4. Apply a 2x horizontal and vertical scale with overflow checks to windowed size calculation.
5. Update runtime-probe checks for the title, FPS formatting, and 1280x960 client size.
6. Update `ARCHITECTURE.md` and the corresponding work log.
7. Run Windows x86 Debug/Release builds and CTest.
8. Apply default host processing to caption text/non-client messages that the original popup WndProc may consume.
9. Correct windowed style to the standard resizable/maximizable `WS_OVERLAPPEDWINDOW`.
10. Verify title storage, caption hit testing, and standard style with a hostile-WndProc runtime probe.
11. Block ordinary `WM_SETTEXT`/`WM_SETICON` replacement after host title and icon assignment.
12. Add runtime-probe regression checks for empty-title and null-icon overwrite attempts.
13. Compare explicit host drawing and DWM non-client policy in the real product to confirm the caption-removal cause.
14. Apply `DWMNCRP_DISABLED` in windowed mode and `DWMNCRP_ENABLED` in fullscreen.
15. Add bounded stored-caption and WndProc state to the graphics trace.

### Completion criteria

- Default windowed execution opens with a 1280x960 client area.
- The title follows the designed format and updates FPS while running.
- The original 640x480 display mode and fullscreen monitor-bounds behavior remain intact.
- Relevant automated verification passes.
- Title text and standard caption interaction remain available independently of the original WndProc behavior.
