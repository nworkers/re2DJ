# 작업 092: Win32 DWM 캡션 보존

## 한국어

### 목표

[설계 092](../design/20260829-092-dwm-caption-preservation.md)에 따라 DWM non-client rendering을 끄지 않고 Win32 상태 제목과 icon을 표시한다.

### 작업 항목

1. windowed DWM 정책을 명시적 `DWMNCRP_ENABLED`로 바꾸고 실제 제품에서 확인한다.
2. runtime probe가 두 창 모드의 DWM 활성 상태를 검사하도록 갱신한다.
3. 필요하면 SDL external wrapping 뒤 host caption adapter 설치 단계와 WndProc chain을 분리한다.
4. 필요하면 caption 메시지에 `DwmDefWindowProc` 우선 처리를 적용한다.
5. 현재 HWND DPI 기반 frame 계산을 적용하고 1280×960 client를 보존한다.
6. 실제 제품과 Debug/Release build 및 CTest로 검증한다.
7. `ARCHITECTURE.md`, 구현 현황과 대응 작업 로그를 갱신한다.
8. 원본 HWND top-level 변환 실험을 폐기하고 표준 host shell HWND 안에 원본 HWND를 child로 배치한다.
9. host shell이 제목·icon·resize·fullscreen·close와 lifetime을 소유하도록 분리한다.

### 완료 조건

- windowed에서 `DWMWA_NCRENDERING_ENABLED`가 참이다.
- Windows 11 기본 frame layout에서 제목과 icon이 표시된다.
- client 크기, FPS 제목, resize와 close 동작이 유지된다.
- 관련 자동 검증이 통과한다.

## English

### Goal

Following [Design 092](../design/20260829-092-dwm-caption-preservation.md), display the Win32 status title and icon without disabling DWM non-client rendering.

### Work items

1. Change the windowed DWM policy to explicit `DWMNCRP_ENABLED` and validate it in the actual product.
2. Update the runtime probe to check that DWM remains enabled in both window modes.
3. If necessary, split host-caption adapter installation until after SDL external wrapping and preserve the WndProc chain.
4. If necessary, offer caption messages to `DwmDefWindowProc` first.
5. Apply current-HWND-DPI frame calculation while preserving the 1280x960 client.
6. Verify the actual product and run Debug/Release builds and CTest.
7. Update `ARCHITECTURE.md`, implementation status, and the corresponding work log.
8. Reject conversion of the original HWND into a top-level window and host it as a child inside a standard host-shell HWND.
9. Separate title/icon, resize, fullscreen, close, and lifetime ownership into the host shell.

### Completion criteria

- `DWMWA_NCRENDERING_ENABLED` is true in windowed mode.
- The title and icon appear with the native Windows 11 frame layout.
- Client size, FPS title, resizing, and close behavior remain intact.
- Relevant automated verification passes.
