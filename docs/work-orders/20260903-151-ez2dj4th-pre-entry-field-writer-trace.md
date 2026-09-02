# Task 151: EZ2DJ 4th entry 전 field writer trace

## 작업 목표

process-create 시점부터 `0x00acd824` field write를 감시해 보호 stub와 초기 진입 구간의 writer를 놓치지 않습니다.

## 구현 범위

1. `--null-context-field-writer-early-trace` CLI 옵션을 추가합니다.
2. initial breakpoint 대기 루프의 main/created thread에 `DR3` write watch를 설치합니다.
3. pre-entry `DR3` hit를 처리하고 field 값·EIP·thread를 기록합니다.
4. initial breakpoint 이후 기존 field-writer trace를 유지합니다.
5. build, unit test, 실제 CHD 실행 결과를 기록하고 커밋합니다.

## 비범위

- field 값 변경.
- Hardlock 응답 변경.
- 원본 EXE/자산 저장.

## 완료 조건

- early trace가 `ez2dj4th`에서 준비됩니다.
- pre-entry hit가 있으면 로그에 남습니다.
- 기존 source/access trace와 충돌하지 않습니다.
- build 및 unit test가 통과합니다.

---

# Task 151: EZ2DJ 4th Pre-Entry Field-Writer Trace

## Objective

Watch field writes from process creation so writers in the protection stub and early-entry phase are not missed.

## Scope

1. Add `--null-context-field-writer-early-trace`.
2. Install a `DR3` write watch for main and created threads while waiting for the initial breakpoint.
3. Handle pre-entry `DR3` hits and record field value, EIP, and thread.
4. Preserve the existing post-entry field-writer trace.
5. Record build, unit-test, and real-CHD results and commit.

## Out of Scope

- Changing the field value.
- Changing Hardlock responses.
- Storing the original executable or assets.

## Completion Criteria

- Early trace prepares for `ez2dj4th`.
- Any pre-entry hit is recorded.
- The trace does not conflict with existing source/access traces.
- The build and unit tests pass.
