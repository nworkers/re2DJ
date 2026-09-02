# Task 149: EZ2DJ 4th 객체 공급 경로 추적

## 작업 목표

`0x00acd708` 객체가 `[EBP-0x118]` stack-local에 공급되는 쓰기 지점을 확인할 수 있는 Windows x86 진단 옵션을 추가합니다.

## 선행 근거

- Task 147: `0x00acd824` field read와 null receiver AV 순서를 확인했습니다.
- Task 148: 상위 객체가 image-resident이며 관찰된 `LocalAlloc`/`VirtualAlloc` 반환값과 일치하지 않음을 확인했습니다.
- 기존 `ez2dj1stse` IO 재사용과 4th CHD read-only VFS 경로는 변경하지 않습니다.

## 구현 범위

1. 설계 문서에 `--null-context-object-source-trace`의 목적·제약·검증 전략을 기록합니다.
2. `DR2` 기반 4-byte write watch 설치 및 thread 생성 시 재설치를 구현합니다.
3. source hit context와 object/frame-slot 일치 여부를 JSON 진단 로그에 기록합니다.
4. 기존 slot-writer trace와 debug-register 충돌을 방지하는 CLI 검증을 추가합니다.
5. build, unit test, 실제 CHD 진단 실행으로 결과를 확인합니다.
6. 확인된 사실·추정·미확정 결과를 analysis와 작업 로그에 반영하고 커밋합니다.

## 비범위

- 원본 실행 파일 패치.
- `0x00acd824` field 직접 주입.
- Hardlock 응답값 변경.
- 원본 HDD/CHD 저장 또는 커밋.

## 완료 조건

- 새 옵션이 `ez2dj4th`에만 허용됩니다.
- source trace가 기존 field access trace와 함께 준비됩니다.
- hit가 발생하면 configured slot, 실제 frame slot, 값, EIP가 로그에 남습니다.
- Windows x86 Debug build와 전체 unit test가 통과합니다.
- 실제 CHD 실행 결과가 작업 로그에 기록됩니다.

---

# Task 149: EZ2DJ 4th Object Supply-Path Trace

## Objective

Add a Windows x86 diagnostic option that can identify the write supplying object `0x00acd708` to the `[EBP-0x118]` stack-local.

## Basis

- Task 147 confirmed the `0x00acd824` field-read and null-receiver AV order.
- Task 148 established that the upper object is image-resident and did not match observed `LocalAlloc` or `VirtualAlloc` return values.
- Preserve the existing `ez2dj1stse` IO reuse and the 4th CHD read-only VFS path.

## Scope

1. Record the purpose, constraints, and verification strategy of `--null-context-object-source-trace` in the design document.
2. Implement a `DR2` four-byte write watch and re-install it for created threads.
3. Record source-hit context and object/frame-slot match information in JSON diagnostics.
4. Add CLI validation that prevents a debug-register conflict with the existing slot-writer trace.
5. Verify with the build, unit tests, and a real CHD diagnostic run.
6. Update analysis and the work log with confirmed, inferred, and unresolved results, then commit.

## Out of Scope

- Patching the original executable.
- Direct injection of field `0x00acd824`.
- Changing Hardlock response values.
- Storing or committing the original HDD/CHD.

## Completion Criteria

- The new option is accepted only for `ez2dj4th`.
- The source trace prepares alongside the existing field-access trace.
- A hit records the configured slot, actual frame slot, values, and EIP.
- The Windows x86 Debug build and full unit-test suite pass.
- The real CHD result is recorded in the work log.
