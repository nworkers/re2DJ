# 작업 358 작업 지시서 — `kernel32!ExitProcess`와 게스트 종료 경계 / Task 358 work order — `kernel32!ExitProcess` and the guest exit boundary

설계: [20260924-358-kernel32-exit-process.md](../design/20260924-358-kernel32-exit-process.md)

## 단계 / Steps

1. `ImportReturn`과 `NativeImportGateResult`에 `exit_process`/`exit_code`를 추가한다. `ImportDispatcher`는 이를 `kStop`으로, `NativeGuestModuleSet`은 그대로 전달한다.
2. `kernel32` facade에 `ExitProcess`를 추가하고 단위 테스트를 갱신한다.
3. `ExitNativeGuestProcess`와 `GuestProcessExited`/`GuestExitCode`를 x86·x64 bootstrap과 bridge에 구현한다. `NativePeSession`과 `NativeInProcessRunner`가 종료를 정상 완료로 보고하게 한다.
4. continuation과 CLI가 `kProcessExit`를 `ExitProcess(<code>)`로 출력하게 한다.
5. 두 폭의 in-process probe에 합성 종료 검사를 추가한다. 빌드, CTest, helper 스크립트를 실행하고, 실제 4th CHD로 두 폭의 새 경계를 확인한다.
6. 분석, `ARCHITECTURE.md`, `docs/TODO.md`, 작업 로그를 갱신하고 커밋한다.

*Steps: (1) add `exit_process`/`exit_code` to `ImportReturn` and `NativeImportGateResult`, mapping to `kStop` in `ImportDispatcher` and passing through in `NativeGuestModuleSet`; (2) add `ExitProcess` to the `kernel32` facade and update the unit tests; (3) implement `ExitNativeGuestProcess` and `GuestProcessExited`/`GuestExitCode` in the x86 and x64 bootstraps and bridges, with `NativePeSession` and `NativeInProcessRunner` reporting the exit as a normal completion; (4) have the continuation and CLI print `kProcessExit` as `ExitProcess(<code>)`; (5) add a synthetic exit check to both widths' in-process probes, build, run CTest and the helper script, and check the new boundary on the real 4th CHD on both widths; (6) update the analysis, `ARCHITECTURE.md`, `docs/TODO.md`, and the work log, then commit.*

## 완료 조건 / Completion criteria

* 단위 테스트와 두 폭의 합성 종료 검사가 통과한다.
* 실제 4th CHD에서 `#0016`이 facade 주소를 받고, 다음 경계가 두 폭에서 같게 기록된다.

*Completion: the unit tests and both widths' synthetic exit checks pass, and on the real 4th CHD `#0016` receives a facade address with the next boundary recorded identically on both widths.*
