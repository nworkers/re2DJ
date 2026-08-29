# Win32 창 닫기와 원본 프로세스 종료 작업 지시

관련 설계: [Win32 창 닫기와 원본 프로세스 종료](../design/20260829-090-window-close-process-exit.md)

## 상태

**완료.** 실제 trace로 확인한 `ExitProcess(0)` termination 교착을 current-process `TerminateProcess(..., 0)`으로 우회했고, Debug/Release 자동 검증과 실제 제품 사용자 검증을 완료했다.

## 작업

1. 1차 공용 SDL close 상태와 `Flip` thread 종료 전달을 제거한다.
2. `window_mode`가 원본 WndProc를 보존하고 `WM_CLOSE`와 `WM_SYSCOMMAND/SC_CLOSE` 정리 뒤 `ExitProcess(0)`을 호출하게 한다.
3. runtime probe child의 원본 WndProc가 `SC_CLOSE`를 소비하고 HWND를 직접 파괴하게 하여, parent가 bounded process exit code 0을 검사한다.
4. adapter 설치 뒤 WndProc를 교체하는 probe로 message chain 우회를 재현하고, 파괴·숨김 HWND fallback을 검증한다.
5. process당 하나의 HWND watcher를 window-mode 적용 뒤 시작하고 반복 적용에서는 target만 갱신한다.
6. child main thread가 message/Flip 없이 기다리는 probe로 watcher의 독립 종료를 검증한다.
7. watcher 등록·시작·초당 상태 표본·close/exit를 기존 graphics trace에 bounded 기록한다.
8. 확인된 host close 경계의 `ExitProcess`를 current-process `TerminateProcess`로 교체하고 exit code 0을 유지한다.
9. 작업 089의 중앙 그림 사용자 검증 성공을 analysis, TODO, IMPLEMENTED와 작업 로그에 반영한다.
10. Windows x86 Debug/Release build와 CTest를 수행한다.
11. architecture와 작업 로그를 갱신하고 커밋한다.

---

# Win32 Window Close and Original Process Exit Work Order

Related design: [Win32 Window Close and Original Process Exit](../design/20260829-090-window-close-process-exit.md)

## Status

**Complete.** Current-process `TerminateProcess(..., 0)` bypasses the `ExitProcess(0)` termination deadlock confirmed by actual trace; Debug/Release automated verification and actual-product user validation are complete.

## Work

1. Remove the first shared-SDL close state and `Flip`-thread forwarding.
2. Preserve the original WndProc in `window_mode` and call `ExitProcess(0)` after cleanup for both `WM_CLOSE` and `WM_SYSCOMMAND/SC_CLOSE`.
3. Make the runtime-probe child's original WndProc consume `SC_CLOSE` and destroy its HWND directly, then verify bounded exit code zero from the parent.
4. Reproduce message-chain bypass by replacing the WndProc after adapter installation, then verify the destroyed/hidden-HWND fallback.
5. Start one HWND watcher per process after applying window mode and update only its target on repeated application.
6. Verify independent watcher termination with a child whose main thread waits without messages or `Flip`.
7. Record bounded watcher registration, startup, one-per-second state samples, close, and exit decisions in the existing graphics trace.
8. Replace `ExitProcess` at the confirmed host-close boundary with current-process `TerminateProcess`, preserving exit code zero.
9. Record successful user validation of Task 089 center artwork in analysis, TODO, IMPLEMENTED, and its work log.
10. Run Windows x86 Debug/Release builds and CTest.
11. Update architecture and the work log, then commit.
