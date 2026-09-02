# ez2dj4th null field 최초 writer 추적 작업 로그

## 결과 요약

Task 145에서 확인한 `0x00acd708 + 0x11c` field에 x86 `DR3` 4-byte write watch를 설치했습니다. 실제 CHD 두 번의 실행에서 watch는 정상 준비됐지만 hit는 0회였고, field는 ready 시점부터 AV까지 0으로 유지됐습니다. 고정 absolute reference scan도 `matches=0`이었습니다.

## 변경 내용

- `--null-context-field-writer-trace` diagnostic option을 추가했습니다.
- field 주소를 `image_base + 0x006cd824`로 계산하고 DR3에 4-byte write watch를 설정합니다.
- primary 및 생성 thread에 watch를 적용합니다.
- post-store EIP, registers, DR6, field 값, runtime code window를 hit event에 기록합니다.
- 기존 `--slot-writer-trace`와 DR0–DR2/DR3로 공존하도록 했습니다.
- 원본 code, field 값, guest return value, branch, IAT는 수정하지 않았습니다.

## 실행 증거

- `20260903-014526-938.jsonl`: `prepared=true`, ready field `0x00000000`, field writer hit 0회, 동일 AV 재현.
- `20260903-014716-040.jsonl`: `prepared=true`, ready field `0x00000000`, field writer hit 0회, child-exit field current `0x00000000`; 고정 field reference scan `matches=0`.
- `20260903-014819-394.jsonl`: 기존 slot-writer trace와 동시 실행. slot writer `0x00aefe62` hit는 정상 기록됐고 새 field writer hit는 0회였습니다.

## 판정

이번 작업의 판정은 **관찰 범위 내 writer 미확정**입니다. field가 watch 설치 이전에 이미 0이었거나 간접 객체 경로로 접근될 수 있습니다. 따라서 writer 부재나 Hardlock과의 무관계를 확정하지 않습니다.

다음 작업은 같은 field에 read/access watch를 설정해 최초 read 지점, read 직전의 `ECX`/객체 값, caller chain을 기록해야 합니다.

## 검증

- `cmd /c scripts\build_win32.bat`: 성공
- `re2dj_unit_tests.exe`: `checks: 1184, failures: 0`
- 실제 `4thTrax.chd` 및 staging HDD 실행: 성공적으로 진단 로그 생성 및 AV 재현
- 원본 CHD·HDD·실행 파일과 runtime 로그는 저장소에 추가하지 않았습니다.

---

# ez2dj4th Null-Field First-Writer Trace Work Log

## Result summary

An x86 `DR3` four-byte write watch was installed for the `0x00acd708 + 0x11c` field identified by Task 145. In two real-CHD runs the watch was prepared successfully but produced zero hits, and the field remained zero from the ready event through the AV. The fixed absolute reference scan also reported `matches=0`.

## Changes

- Added the `--null-context-field-writer-trace` diagnostic option.
- Compute the field as `image_base + 0x006cd824` and install a four-byte write watch in DR3.
- Apply the watch to the primary and created threads.
- Record post-store EIP, registers, DR6, field values, and a runtime code window at each hit.
- Keep the watch compatible with the existing DR0–DR2 `--slot-writer-trace`.
- Do not modify original code, the field value, guest return values, branches, or IAT entries.

## Execution evidence

- `20260903-014526-938.jsonl`: `prepared=true`, ready field `0x00000000`, zero field-writer hits, and the same AV reproduced.
- `20260903-014716-040.jsonl`: `prepared=true`, ready field `0x00000000`, zero field-writer hits, child-exit field current `0x00000000`, and fixed field reference scan `matches=0`.
- `20260903-014819-394.jsonl`: ran the existing slot-writer trace concurrently. The slot-writer hit at `0x00aefe62` was recorded normally, while the new field-writer hit count stayed zero.

## Classification

This task is classified as **writer unresolved within the observed scope**. The field may already have been zero before the watch was armed or may be accessed through an indirect object path. The absence of a hit therefore does not prove writer absence or independence from Hardlock.

The next task should set a read/access watch on the same field and record the first read, `ECX`/object values immediately before the read, and the caller chain.

## Verification

- `cmd /c scripts\build_win32.bat`: passed.
- `re2dj_unit_tests.exe`: `checks: 1184, failures: 0`.
- Running with the real `4thTrax.chd` and staging HDD produced diagnostic logs and reproduced the AV.
- The original CHD, HDD, executable, and runtime logs were not added to the repository.
