# ez2dj4th null field access 추적 작업 로그

## 결과 요약

`0x00acd708 + 0x11c = 0x00acd824` field에 x86 `DR3` 4-byte read/write access watch를 설치했습니다. 실제 CHD 실행에서 최초 access는 `0x0041a699`의 `mov ECX, [ECX+0x11c]`와 일치했고, field는 0으로 읽혔습니다. 같은 thread에서 `ECX=0` 상태로 곧바로 `0x00434137` null receiver AV가 발생했습니다.

이번 결과로 read site와 null 전달 순서는 확인했지만, field가 0으로 초기화되는 상위 경계는 아직 확인하지 않았습니다. 따라서 guest field나 Hardlock 응답을 임의로 보정하지 않았습니다.

## 변경 내용

- `--null-context-field-access-trace` bounded diagnostic option을 추가했습니다.
- `image_base + 0x006cd824`에 DR3 4-byte read/write watch를 설정했습니다.
- primary 및 생성 thread에 동일한 watch를 적용했습니다.
- 기존 `--slot-writer-trace`의 DR0–DR2와 DR3 access watch의 공존을 유지했습니다.
- access hit에서 post-access EIP, registers, DR6, field 값, runtime code window, stack return address를 기록합니다.
- Task 146의 writer-only DR3 option과의 동시 사용은 거부합니다.
- 원본 code, field 값, guest return value, branch, IAT, Hardlock response를 수정하지 않았습니다.

## 실행 증거

- `20260903-015902-887.jsonl`: access watch `prepared=true`; first hit thread `4772`, field `0x00acd824`, post-access EIP `0x0041a69f`, `ECX=0`, field before/after `0`; 이후 같은 thread에서 `0x00434137` AV.
- `20260903-020004-160.jsonl`: 기존 slot-writer `DR0–DR2`와 access `DR3`를 함께 사용했으며, 두 watch의 hit가 같은 thread에서 정상 기록됨.
- 두 실행 모두 child-exit boundary가 기록되어 bounded diagnostic path가 정상 종료됨.

## 판정

이번 작업은 **read site와 null 전달 순서 확인**으로 판정합니다. field read 직후 null receiver AV로 이어지는 실행 순서는 확인됐습니다. 다만 field가 왜 0인지, `0x00acd708` 상위 객체를 어느 HLE 경계에서 초기화해야 하는지, raw I/O 상태가 간접적으로 영향을 주는지는 미확정입니다.

다음 분석에서는 해당 상위 객체의 생성·초기화 경로 또는 `ez2dj1stse`의 대응 객체 공급 경계를 비교해야 합니다. 현재 단계에서는 field 직접 주입, branch 우회, 확인되지 않은 4th Hardlock/raw-I/O 응답 승격을 수행하지 않습니다.

## 검증

- `cmd /c scripts\build_win32.bat`: 성공
- `re2dj_unit_tests.exe`: `checks: 1184, failures: 0`
- 실제 `4thTrax.chd` 및 staging HDD 실행: access hit와 AV 순서 재현
- 원본 CHD·HDD·실행 파일 및 runtime 로그는 저장소에 추가하지 않았습니다.

관련 누적 분석은 [ez2dj4th Hardlock runtime 분석](../analysis/ez2dj4th-hardlock-runtime.md)을 참조합니다.

---

# ez2dj4th Null-Field Access Trace Work Log

## Result summary

An x86 `DR3` four-byte read/write access watch was installed for field `0x00acd708 + 0x11c = 0x00acd824`. In the real-CHD run, the first access matched `mov ECX, [ECX+0x11c]` at `0x0041a699`, and the field read as zero. The same thread then reached the `0x00434137` null-receiver AV with `ECX=0`.

The result confirms the read site and null-propagation order, but not the upper boundary that leaves the field zero. No guest field or Hardlock response was therefore patched or guessed.

## Changes

- Added the bounded diagnostic option `--null-context-field-access-trace`.
- Installed a four-byte read/write watch in DR3 at `image_base + 0x006cd824`.
- Applied the same watch to the primary and created threads.
- Preserved coexistence between the existing `--slot-writer-trace` DR0–DR2 watches and the DR3 access watch.
- Record post-access EIP, registers, DR6, field values, the runtime code window, and the stack return address at each access hit.
- Reject simultaneous use of Task 146's writer-only DR3 option.
- Do not modify original code, the field value, guest return values, branches, IAT entries, or Hardlock responses.

## Execution evidence

- `20260903-015902-887.jsonl`: access watch `prepared=true`; first hit on thread `4772`, field `0x00acd824`, post-access EIP `0x0041a69f`, `ECX=0`, field before/after `0`; the same thread then raised the AV at `0x00434137`.
- `20260903-020004-160.jsonl`: ran the existing slot-writer `DR0`–`DR2` watches together with access `DR3`; both watch hits were recorded normally on the same thread.
- Both runs recorded the child-exit boundary, confirming normal termination of the bounded diagnostic path.

## Classification

This task is classified as **read site and null-propagation order confirmed**. The execution order from the field read to the null-receiver AV is confirmed. The reason the field is zero, the HLE boundary that should initialize object `0x00acd708`, and any indirect relation to raw-I/O state remain unresolved.

The next analysis should compare the upper object's construction/initialization path or the corresponding object-supply boundary used by `ez2dj1stse`. At this stage, no direct field injection, branch bypass, or promotion of unconfirmed 4th Hardlock/raw-I/O responses is performed.

## Verification

- `cmd /c scripts\build_win32.bat`: passed.
- `re2dj_unit_tests.exe`: `checks: 1184, failures: 0`.
- Real `4thTrax.chd` and staging-HDD execution: access-hit and AV ordering reproduced.
- The original CHD, HDD, executable, and runtime logs were not added to the repository.

See [ez2dj4th Hardlock runtime analysis](../analysis/ez2dj4th-hardlock-runtime.md) for the cumulative findings.
