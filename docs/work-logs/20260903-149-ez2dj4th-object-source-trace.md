# Task 149 작업 로그: EZ2DJ 4th 객체 공급 경로 추적

## 작업 내용

- `--null-context-object-source-trace` 옵션을 추가했습니다.
- x86 `DR2`에 관찰된 `0x001afcf0` stack-local의 4바이트 write-only watch를 설치했습니다.
- main thread와 생성 thread에 watch를 설정하고, hit context·frame slot·target object 일치 여부·runtime code window를 JSON으로 기록했습니다.
- 기존 field access `DR3`와 함께 사용할 수 있도록 했고, slot-writer `DR0–DR2`와의 충돌은 CLI에서 거부했습니다.
- 기존 `ez2dj1stse` IO 재사용, 4th CHD VFS, Hardlock 응답 경로는 변경하지 않았습니다.

## 검증

- Windows x86 Debug build: 성공.
- unit tests: `checks: 1184, failures: 0`.
- 실제 실행:
  - source + field access: `logs/windows_x86_launcher_probe/ez2dj4th/20260903-025152-528.jsonl`
  - field access baseline: `logs/windows_x86_launcher_probe/ez2dj4th/20260903-025218-345.jsonl`

## 결과

- source trace 준비: 성공.
- 고정 slot source hit: 61건.
- target object 일치: 0건.
- baseline field access: 1건.
- baseline object: `0x00acd708`.
- baseline frame slot: `0x001afcf0`.
- baseline field value: `0x00000000`.
- baseline 후속 fault: `0x00434137`, read AV `0x00000014`.

고정 주소 watch 자체는 작동했지만, 해당 주소가 실행마다 target frame slot로 유지되지 않았고 다른 stack 쓰기를 다수 포착했습니다. 이번 작업에서는 field 값을 주입하거나 Hardlock 응답을 변경하지 않았습니다. 객체 공급 지점은 미확정으로 남겼으며, 다음 작업에서 runtime 함수 진입 후 동적 `EBP-0x118` watch를 구현합니다.

---

# Task 149 Work Log: EZ2DJ 4th Object Supply-Path Trace

## Changes

- Added `--null-context-object-source-trace`.
- Installed a four-byte write-only watch in x86 `DR2` for the observed `0x001afcf0` stack-local.
- Configured the watch for the main and created threads and recorded hit context, frame-slot information, target-object matches, and a runtime code window as JSON.
- Preserved the existing field-access `DR3` watch and rejected conflicts with slot-writer `DR0–DR2` from the CLI.
- Did not change the existing `ez2dj1stse` IO reuse, 4th CHD VFS, or Hardlock-response paths.

## Verification

- Windows x86 Debug build: passed.
- Unit tests: `checks: 1184, failures: 0`.
- Real runs:
  - source plus field access: `logs/windows_x86_launcher_probe/ez2dj4th/20260903-025152-528.jsonl`
  - field-access baseline: `logs/windows_x86_launcher_probe/ez2dj4th/20260903-025218-345.jsonl`

## Result

- Source trace preparation: passed.
- Fixed-slot source hits: 61.
- Target-object matches: 0.
- Baseline field-access hits: 1.
- Baseline object: `0x00acd708`.
- Baseline frame slot: `0x001afcf0`.
- Baseline field value: `0x00000000`.
- Follow-up baseline fault: `0x00434137`, read AV `0x00000014`.

The fixed-address watch worked, but the address did not remain the target frame slot and captured many unrelated stack writes. This task did not inject the field or change Hardlock responses. The object-supply point remains unresolved; the next task will implement a dynamic `EBP-0x118` watch after the runtime function entry.
