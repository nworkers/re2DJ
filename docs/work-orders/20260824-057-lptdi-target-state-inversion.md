# LPTDI target state 역산 작업 지시

관련 설계: [LPTDI target state 역산](../design/20260824-057-lptdi-target-state-inversion.md)

## 목표

8바이트 target state가 보호 `.data`를 변환하는 runtime 경로를 복원하고 정상 initializer를 만드는 값을 역산한다.

## 작업 범위

1. zero target state와 4096-step trace로 state 전달과 `.data` 접근을 추적한다.
2. 보호 raw, zero-state runtime, 비보호 plaintext의 대응 블록을 비교한다.
3. 필요한 경우 state 또는 `.data` 소비에 한정된 diagnostic을 설계·구현한다.
4. target-state 후보를 계산해 최소 두 번 canonical 실행한다.
5. access violation, initializer window, 첫 후속 API를 비교한다.
6. 코드 변경 시 Windows x86/x64 build와 범위에 맞는 CTest를 수행한다.
7. 누적 분석, TODO, 설계와 작업 로그를 갱신하고 커밋한다.

## 완료 조건

정상 initializer 복원과 AV 제거를 반복 확인하거나, 이를 막는 마지막 미확정 transform을 실행 주소·입출력·명령 수준으로 확정해야 한다.

## 수행 결과

최소 target state `0900000000000000`으로 정상 initializer와 AV 제거를 두 번 확인했다. 변경 코드는 Windows x86 전용 launcher diagnostic과 trace breakpoint 조정이므로 Windows x86 build와 CTest를 검증 범위로 확정했다.

---

# LPTDI Target-State Inversion Work Order

Related design: [LPTDI Target-State Inversion](../design/20260824-057-lptdi-target-state-inversion.md)

## Goal

Recover the runtime path by which the eight-byte target state transforms protected `.data`, then invert the value that produces the normal initializer.

## Scope

Trace zero target state for up to 4096 steps, compare protected raw/zero-state runtime/unprotected plaintext blocks, add narrowly scoped diagnostics only if needed, calculate and validate a candidate in two canonical runs, compare access violations and initializer state, run relevant x86/x64 verification after code changes, update cumulative documentation, and commit.

## Completion criteria

Either repeatedly restore the normal initializer and remove the AV, or identify the final unresolved transform at instruction-level addresses with repeatable inputs and outputs.

## Execution result

Minimal target state `0900000000000000` restored the normal initializer and removed the AV twice. The code changes affect only the Windows x86 launcher diagnostic and trace-breakpoint handling, so Windows x86 build and CTest are the applicable verification scope.
