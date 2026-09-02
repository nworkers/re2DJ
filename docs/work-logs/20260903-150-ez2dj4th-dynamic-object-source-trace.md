# Task 150 작업 로그: EZ2DJ 4th 동적 객체 공급 경계 추적

## 작업 내용

- Task 149의 고정 `0x001afcf0` stack watch를 제거하고 runtime-confirmed boundary 기반으로 변경했습니다.
- field-access anchor `0x0041a699` 주변 runtime bytes에서 baseline 실행 중 prologue `0x0041a649`와 prologue 직후 boundary `0x0041a64c`를 확인했습니다.
- 확인된 boundary RVA `0x001a64c`에 `DR0` one-shot execution breakpoint를 설치합니다.
- boundary hit에서 현재 `EBP-0x118`을 계산해 thread별 `DR2` 4-byte write watch를 동적으로 설치합니다.
- source hit에 frame slot, 값, target object 일치 여부와 runtime code window를 기록합니다.
- 기존 `DR3` field access trace와 `ez2dj1stse` IO 재사용 경로는 유지했습니다.

## 검증

- Windows x86 Debug build: 성공.
- 실제 실행:
  - `logs/windows_x86_launcher_probe/ez2dj4th/20260903-030421-317.jsonl`

실행 명령:

```text
re2dj_windows_x86_launcher_probe.exe --hdd <user-hdd> --chd <user-chd> --target ez2dj4th --target-executable EZ2DJ\EZ2DJ.EXE --hle-io-ports --device-mock-lptdi --device-mock-wts-console-session --null-context-object-source-trace --null-context-field-access-trace
```

- boundary hit: 1건, `0x0041a64c`.
- boundary frame: `EBP=0x001afe08`, dynamic slot `0x001afcf0`.
- source hit: 2건.
- target object match: 1건, `0x00acd708`.
- target assignment code: `mov [EBP-0x118], ECX`, instruction start `0x0041a668`, post-EIP `0x0041a66e`.
- field access: 1건, field `0x00acd824` 값 `0x00000000`.
- 후속 fault: `0x00434137`, read AV `0x00000014`.

## 결론

상위 객체 `0x00acd708`이 `[EBP-0x118]`에 공급되는 instruction은 확인되었습니다. 공급값은 `ECX=0x00acd708`이며, 이후 같은 frame에서 `object + 0x11c` field는 여전히 0입니다. 따라서 현재 실행 실패는 객체 pointer 공급 실패가 아니라 상위 객체 field 초기화 경로 미확정 문제로 좁혀졌습니다. 이번 작업에서는 field를 직접 주입하거나 Hardlock 응답을 바꾸지 않았습니다.

## 단위 테스트

별도 코드 변경 후 전체 unit test를 실행했으며 `checks: 1184, failures: 0`입니다.

---

# Task 150 Work Log: EZ2DJ 4th Dynamic Object-Supply Boundary Trace

## Changes

- Replaced Task 149's fixed `0x001afcf0` stack watch with a runtime-confirmed boundary flow.
- A baseline runtime scan around field-access anchor `0x0041a699` confirmed prologue `0x0041a649` and the post-prologue boundary `0x0041a64c`.
- Installed a one-shot `DR0` execution breakpoint at confirmed boundary RVA `0x001a64c`.
- On the boundary hit, calculated the current `EBP-0x118` and installed a per-thread four-byte `DR2` write watch.
- Recorded frame slots, values, target-object matches, and runtime code windows for source hits.
- Preserved the existing `DR3` field-access trace and `ez2dj1stse` IO-reuse path.

## Verification

- Windows x86 Debug build: passed.
- Real run:
  - `logs/windows_x86_launcher_probe/ez2dj4th/20260903-030421-317.jsonl`

Command shape:

```text
re2dj_windows_x86_launcher_probe.exe --hdd <user-hdd> --chd <user-chd> --target ez2dj4th --target-executable EZ2DJ\EZ2DJ.EXE --hle-io-ports --device-mock-lptdi --device-mock-wts-console-session --null-context-object-source-trace --null-context-field-access-trace
```

- Boundary hits: 1 at `0x0041a64c`.
- Boundary frame: `EBP=0x001afe08`, dynamic slot `0x001afcf0`.
- Source hits: 2.
- Target-object matches: 1, `0x00acd708`.
- Target assignment: `mov [EBP-0x118], ECX`, instruction start `0x0041a668`, post-EIP `0x0041a66e`.
- Field accesses: 1, field `0x00acd824` value `0x00000000`.
- Follow-up fault: `0x00434137`, read AV `0x00000014`.

## Conclusion

The instruction supplying upper object `0x00acd708` to `[EBP-0x118]` is confirmed. The supplied value is `ECX=0x00acd708`, while `object + 0x11c` remains zero in the same frame. The execution failure is therefore narrowed from object-pointer supply to the unresolved upper-object field initialization path. This task did not inject the field or change Hardlock responses.

## Unit Tests

The full unit-test suite passed after the code changes: `checks: 1184, failures: 0`.
