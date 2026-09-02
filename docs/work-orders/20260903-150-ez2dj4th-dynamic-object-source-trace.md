# Task 150: EZ2DJ 4th 동적 객체 공급 경계 추적

## 작업 목표

고정 stack 주소를 제거하고 runtime에서 확인된 함수 boundary 이후 실제 `EBP-0x118`을 계산해 객체 공급 write를 추적합니다.

## 구현 범위

1. field-read runtime anchor에서 확인된 prologue/boundary를 기록하고 교차검증합니다.
2. 확인된 boundary 직후 `DR0` 실행 breakpoint를 설치합니다.
3. `DR0` hit에서 `EBP-0x118`을 계산해 `DR2` write watch로 전환합니다.
4. 동적 source hit와 target object 일치 여부를 기록합니다.
5. 기존 field access trace 및 새 thread arm 경로를 유지합니다.
6. build, unit test, 실제 CHD 실행 결과를 문서화하고 커밋합니다.

## 비범위

- 원본 EXE 패치.
- field 직접 주입.
- Hardlock 응답 변경.
- 원본 자산 저장.

## 완료 조건

- source option이 고정 absolute stack 주소 없이 준비됩니다.
- runtime boundary hit가 실제 frame 정보를 기록합니다.
- target object 일치 여부가 source hit에 남습니다.
- build와 unit test가 통과합니다.
- 실제 CHD 결과가 work log와 analysis에 반영됩니다.

---

# Task 150: EZ2DJ 4th Dynamic Object-Supply Boundary Trace

## Objective

Remove the fixed stack address and trace the object-supply write by calculating the actual `EBP-0x118` after the runtime-confirmed function boundary.

## Scope

1. Record and cross-check the prologue/boundary found from the runtime field-read anchor.
2. Install a `DR0` execution breakpoint at the confirmed boundary.
3. On the `DR0` hit, calculate `EBP-0x118` and switch to a `DR2` write watch.
4. Record dynamic source hits and target-object matches.
5. Preserve the existing field-access trace and created-thread arming path.
6. Document and commit build, unit-test, and real-CHD results.

## Out of Scope

- Patching the original executable.
- Direct field injection.
- Changing Hardlock responses.
- Storing original assets.

## Completion Criteria

- The source option prepares without a fixed absolute stack address.
- A runtime-boundary hit records the actual frame information.
- Source hits record whether the target object matched.
- The build and unit tests pass.
- Real-CHD results are reflected in the work log and analysis.
