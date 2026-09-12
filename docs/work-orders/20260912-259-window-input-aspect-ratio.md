# 작업 지시서: 실제 창 입력과 4:3 비율 보정

## 관련 설계

[창 입력 전달과 4:3 표시 비율 설계](../design/20260912-259-window-input-aspect-ratio.md)

## 작업 범위

1. 현재 Windows host/guest/SDL 입력 전달 경계를 확인한다.
2. host와 guest 양쪽에서 Alt scale shortcut 및 double-click fullscreen을 처리한다.
3. windowed 수동 resize 시 client 영역의 4:3 비율을 유지한다.
4. OpenGL fullscreen presentation에 4:3 letterbox/pillarbox viewport를 적용한다.
5. runtime probe와 기존 테스트를 갱신하고 build/test를 수행한다.
6. 설계, architecture, work log를 갱신하고 커밋한다.

## 제외 범위

- profile별 HLE flag 변경
- 원본 게임 실행 파일과 원본 자산 변경
- 논리 render target 또는 DirectDraw display mode 변경
- Linux/Web의 창 입력 정책 변경

## 완료 조건

- 실제 host focus 경로에서 Alt+1/2/3이 동작한다.
- 실제 client double-click으로 fullscreen 진입/해제가 가능하다.
- 수동 resize 후 client 영역이 4:3이다.
- fullscreen monitor 비율과 관계없이 콘텐츠가 4:3으로 표시된다.
- 기존 build와 테스트가 통과한다.

## English

### Related design

[Window input delivery and 4:3 aspect-ratio design](../design/20260912-259-window-input-aspect-ratio.md)

### Scope

1. Inspect the Windows host/guest/SDL input boundary.
2. Handle Alt scale shortcuts and double-click fullscreen in both host and guest paths.
3. Preserve a 4:3 client area during manual windowed resizing.
4. Apply a 4:3 letterbox/pillarbox viewport for OpenGL fullscreen presentation.
5. Update the runtime probe and run the existing build/tests.
6. Update design, architecture, and work log, then commit.

### Out of scope

- Profile-specific HLE flag changes
- Original executable or asset changes
- Logical render-target or DirectDraw display-mode changes
- Linux/Web window-input policy changes

### Completion criteria

- Alt+1/2/3 work through the actual host-focus input route.
- Actual client double-click enters and leaves fullscreen.
- The client area remains 4:3 after manual resizing.
- Fullscreen output remains 4:3 on any monitor aspect ratio.
- Existing build and tests pass.
