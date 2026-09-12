# 작업 지시서: Alt+3 창 전환 중 종료 수정

## 관련 설계

[Alt+3 창 전환 중 종료 방지 설계](../design/20260912-260-alt3-window-exit.md)

## 작업 범위

1. 제공된 `ez2dj4th` trace와 현재 Win32 host/lifetime 경로를 대조한다.
2. host 창 모드 재적용에서 `WS_VISIBLE` 경쟁 조건을 제거한다.
3. mode transition 동안의 lifetime 종료 오판과 일시적 숨김 상태를 방어한다.
4. 관련 architecture/analysis 문서를 갱신한다.
5. Windows x86 build/test와 runtime trace 검증을 수행한다.
6. 작업 로그를 남기고 변경을 커밋한다.

## 제외 범위

- 프로파일 값 변경
- 원본 실행 파일 또는 CHD 자산 변경
- CHD 쓰기 또는 VFS read-only 정책 변경
- 원본 게임 로직과 DirectDraw 표시 해상도 변경

## 완료 조건

- `Alt+3` 창 모드 전환 중 lifetime watcher가 프로세스를 종료하지 않는다.
- 실제 창이 파괴되면 기존 close/termination 정책이 유지된다.
- 기존 scale/fullscreen/aspect-ratio 동작과 테스트가 유지된다.
- launcher handoff 성공과 창 전환 종료 원인이 문서에 구분되어 기록된다.

## English

### Related design

[Preventing exit during the Alt+3 window transition](../design/20260912-260-alt3-window-exit.md)

### Scope

1. Compare the supplied `ez2dj4th` trace with the current Win32 host/lifetime path.
2. Remove the `WS_VISIBLE` race during host window-mode reconfiguration.
3. Prevent lifetime termination false positives during a mode transition and transient hiding.
4. Update the relevant architecture and analysis documents.
5. Run Windows x86 build/tests and runtime-trace verification.
6. Leave a work log and commit the changes.

### Out of scope

- Profile value changes
- Original executable or CHD asset changes
- CHD writes or VFS read-only policy changes
- Original game logic and DirectDraw display-resolution changes

### Completion criteria

- The lifetime watcher does not terminate the process during `Alt+3` mode changes.
- Existing close/termination behavior remains for a destroyed window.
- Existing scale/fullscreen/aspect-ratio behavior and tests remain intact.
- The launcher handoff success is documented separately from the window-transition exit cause.
