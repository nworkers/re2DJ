# Win32 창 모드와 메시지 pump 작업 지시

관련 설계: [Win32 창 모드와 메시지 pump](../design/20260828-084-window-mode-message-pump.md)

## 한국어

### 상태

**완료.** 기본 제목 표시줄 창, 외부 fullscreen 설정 주입과 SDL event pump를 구현하고 검증했다.

### 작업

1. 제품 CLI와 `OriginalProcessOptions`에 기본 false인 `--fullscreen`을 추가한다.
2. launcher가 fullscreen 값을 injected runtime export로 전달한다.
3. DirectDraw cooperative-level 경계에서 `re2DJ` title과 windowed/fullscreen style·크기·위치를 적용한다.
4. SDL3/OpenGL Present가 event queue를 매 frame pump하도록 한다.
5. product/runtime probe와 사용자 실행 절차를 확장한다.
6. Windows x86 build·CTest와 실제 windowed/fullscreen 실행을 검증한다.
7. architecture, analysis, TODO, implemented와 작업 로그를 결과에 맞춰 갱신한다.

### 완료 조건

- 기본 실행이 `re2DJ` 제목의 640×480 client-area 창이다.
- `--fullscreen`이 원본 파일 변경 없이 monitor-sized borderless mode를 선택한다.
- 창 클릭·이동 뒤에도 message queue가 계속 처리된다.
- 기존 graphics/audio/input 경계와 host display mode가 회귀하지 않는다.

## English

### Status

**Complete.** Implemented and verified a titled default window, externally injected fullscreen selection, and SDL event pumping.

### Tasks and completion

Add default-false `--fullscreen` transport through product options and the launcher into an injected-runtime export. Apply the `re2DJ` title and windowed/fullscreen style, size, and placement at the DirectDraw cooperative-level boundary. Pump SDL events from Present. Extend product/runtime probes and user procedures, pass the Windows x86 build and CTest, verify real windowed/fullscreen runs, and update cumulative documentation and the work log.

Completion requires a default `re2DJ` window with an exact 640x480 client area, externally selected monitor-sized borderless fullscreen without original-file mutation, a responsive message queue after interaction, and no regression to graphics, audio, input, or host display mode.
