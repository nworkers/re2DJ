# Task 151 작업 로그: EZ2DJ 4th entry 전 field writer trace

## 작업 내용

- `--null-context-field-writer-early-trace` 옵션을 추가했습니다.
- CREATE_PROCESS/CREATE_THREAD debug event 시점에 `0x00acd824` field의 `DR3` 4-byte write watch를 설치했습니다.
- initial breakpoint 대기 루프에서 early `DR3` hit를 처리하고 field 값과 context를 기록하도록 했습니다.
- field-access trace와 early trace를 함께 지정할 수 있도록 CLI normalization을 추가했습니다. 이 조합에서는 entry 이후 `DR3`가 field-access watch로 전환됩니다.
- 기존 source boundary trace와 IO/VFS 경로는 변경하지 않았습니다.

## 검증

- Windows x86 Debug build: 성공.
- unit tests: `checks: 1184, failures: 0`.
- 실제 실행 로그: `logs/windows_x86_launcher_probe/ez2dj4th/20260903-031354-995.jsonl`.

실행 옵션은 기존 CHD/VFS/IO mock 경로에 다음을 추가했습니다.

```text
--null-context-field-writer-early-trace --null-context-object-source-trace --null-context-field-access-trace
```

## 결과

- early field writer hit: 0건.
- object source boundary hit: 1건, `0x0041a64c`.
- dynamic source hit: 2건, target match 1건.
- target object: `0x00acd708`.
- field access hit: 1건, field 값 `0x00000000`.
- 후속 fault: `0x00434137`, read AV `0x00000014`.

현재 실행 증거에서는 보호 stub 또는 initial breakpoint 이전에 field를 쓰는 경로가 관찰되지 않았습니다. 따라서 field는 early 구간에서 초기화되지 않았거나, 다른 주소/간접 경로를 통해 결정될 가능성이 남습니다. field 값과 Hardlock 응답은 변경하지 않았습니다.

---

# Task 151 Work Log: EZ2DJ 4th Pre-Entry Field-Writer Trace

## Changes

- Added `--null-context-field-writer-early-trace`.
- Installed a `DR3` four-byte write watch for field `0x00acd824` at CREATE_PROCESS/CREATE_THREAD debug events.
- Handled early `DR3` hits while waiting for the initial breakpoint and recorded field value and context.
- Added CLI normalization so early tracing can be combined with field-access tracing. In that combination, `DR3` switches to the field-access watch after entry.
- Preserved the existing source-boundary trace and IO/VFS paths.

## Verification

- Windows x86 Debug build: passed.
- Unit tests: `checks: 1184, failures: 0`.
- Real-run log: `logs/windows_x86_launcher_probe/ez2dj4th/20260903-031354-995.jsonl`.

The run preserved the existing CHD/VFS/IO-mock path and added:

```text
--null-context-field-writer-early-trace --null-context-object-source-trace --null-context-field-access-trace
```

## Result

- Early field-writer hits: 0.
- Object-source boundary hits: 1 at `0x0041a64c`.
- Dynamic source hits: 2, with 1 target match.
- Target object: `0x00acd708`.
- Field-access hits: 1, field value `0x00000000`.
- Follow-up fault: `0x00434137`, read AV `0x00000014`.

No field write was observed in the protection-stub or pre-initial-breakpoint interval. The field may therefore remain uninitialized in the early interval or be determined through another address/indirect path. The field value and Hardlock responses were not changed.
