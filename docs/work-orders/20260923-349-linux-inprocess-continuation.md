# 작업 지시 349: Linux i386 in-process 연속 실행 진단 / Work order 349: Linux i386 in-process continuation diagnostic

## 목표 / Objective

[작업 349 설계](../design/20260923-349-linux-inprocess-continuation.md)에 따라 원본을 `kernel32` facade 위에서 처음 처리할 수 없는 지점까지 실행하고 API 호출 순서를 기록하는 `--linux-in-process-continue`를 추가해, 실제 4th CHD에서 다음 HLE 경계를 찾습니다.

*Following the [Task 349 design](../design/20260923-349-linux-inprocess-continuation.md), add `--linux-in-process-continue`, which runs the original on the `kernel32` facade until the first point it cannot handle and records the API call sequence, then use it to find the next HLE boundary on the real 4th CHD.*

## 작업 / Work

1. `NativeKernel32Diagnostic`에 RX `INT3` stop stub, 복귀 slot 재지정, facade export 조회, 미해석 조회 counter를 추가하고 `GetModuleHandleA` 실패도 기록합니다.
2. `OriginalApiCall` 기록과 네 가지 연속 실행 경계를 결과 타입에 추가합니다.
3. `src/platform/linux/native_continuation_observation.cpp`에 `RunOriginalInProcessContinuation`을 구현하고 CMake에 등록합니다.
4. CLI에 `--linux-in-process-continue` 옵션과 호출 기록·경계 출력을 추가합니다.
5. `GetVersion` 반환값을 `0x23F00206`으로 바꾸고 단위 테스트를 갱신합니다.
6. 빌드·테스트·probe, 실제 4th CHD의 기존 네 진단과 연속 실행, `GetVersion`=0 임시 빌드 비교를 수행합니다.
7. analysis, ARCHITECTURE, TODO, 작업 로그를 갱신합니다.

*Add the RX `INT3` stop stub, return-slot redirection, facade-export lookup, and an unresolved-lookup counter to `NativeKernel32Diagnostic`, also recording `GetModuleHandleA` failures; add `OriginalApiCall` records and the four continuation boundaries to the result types; implement `RunOriginalInProcessContinuation` and register it in CMake; add the CLI option with call-log and boundary output; change `GetVersion` to `0x23F00206` and update its unit test; run builds, tests, probes, the four existing diagnostics and the continuation on the real 4th CHD, and the `GetVersion`=0 comparison; and update analysis, ARCHITECTURE, TODO, and the work log.*

## 완료 조건 / Completion criteria

- 연속 실행 진단이 실제 4th CHD에서 네 경계 또는 `kProcessExit` 중 하나로 끝나고, 호출 순서와 경계를 출력합니다.
- 진단은 guest 코드 바이트를 쓰지 않고 복귀 slot 재지정으로만 멈춥니다.
- 기존 네 진단이 이전 경계를 유지하거나, 달라졌다면 그 차이를 원인과 함께 기록합니다.
- `GetVersion` 값의 영향 유무를 비교 실행으로 기록합니다.
- 원본 CHD는 읽기 전용으로 사용하고, 검증 결과와 남은 범위를 작업 로그에 남긴 뒤 하나의 Git 커밋으로 마칩니다.

*The continuation diagnostic ends on the real 4th CHD at one of the four boundaries or `kProcessExit` and prints the call sequence and boundary; it stops only by redirecting return slots, never writing guest code bytes; the four existing diagnostics keep their boundaries or any difference is recorded with its cause; the effect of the `GetVersion` value is recorded from the comparison run; and the original CHD stays read-only, with validation and remaining scope in the work log and one task commit.*
