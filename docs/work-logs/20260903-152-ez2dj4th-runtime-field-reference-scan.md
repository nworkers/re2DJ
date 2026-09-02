# Task 152: EZ2DJ 4th 런타임 field 직접 참조 스캔 작업 로그

## 결과 요약

복호화된 runtime `.text` 전체를 첫 field access 시점에 읽어 `+0x11c` 직접 displacement 후보를 수집했습니다. 스캔은 성공했고 23개 후보 중 4개가 write 분류였습니다. 따라서 field writer가 코드에 전혀 없다는 결론은 폐기하지만, 4개 instruction 중 어느 것이 `0x00acd708 + 0x11c`를 실제로 쓰는지는 아직 확인하지 못했습니다.

## 변경 사항

- `--null-context-field-access-trace`의 첫 hit에서만 EZ2DJ 4th runtime `.text`를 스캔하도록 launcher probe를 변경했습니다.
- `.text` 범위는 image-base 기준 `RVA 0x001000`, virtual size `0x000db022`로 제한했습니다.
- ModRM disp32 `0x0000011c` 후보를 `read`, `write`, `other`로 분류했습니다.
- 개별 후보 이벤트와 `null_context_field_reference_scan` 요약 이벤트를 기록합니다.
- field 값, Hardlock 응답, VFS 경로는 변경하지 않았습니다.

## 검증 증거

- `cmd /c scripts\build_win32.bat`: 성공
- `build/windows-x86/bin/Debug/re2dj_unit_tests.exe`: `checks: 1184, failures: 0`
- 실제 실행 로그: `logs/windows_x86_launcher_probe/ez2dj4th/20260903-032038-359.jsonl`
- runtime scan summary: `readable=true`, `.text` `897058`바이트, `candidates=23`, `read_candidates=16`, `write_candidates=4`, `recorded=23`, `capped=false`
- 확인된 write 후보 RVA: `0x0000fdbd`, `0x0000fde1`, `0x0001825f`, `0x0001dbd3`
- 기존 실행 순서: source boundary 1회, dynamic source hit 2회, target match 1회, field access 1회, field `0x00000000`, AV `0x00434137 / 0xc0000005`

## 판정 및 다음 단계

이번 결과는 `+0x11c`를 직접 쓰는 코드가 runtime image 안에 존재함을 확인합니다. 다만 static candidate는 동일 offset을 공유하는 여러 객체를 포함할 수 있으므로, 이를 곧바로 target object initializer로 사용하지 않습니다. 다음 작업에서는 네 write 후보를 bounded execution trace로 감시하고, 각 hit의 receiver register와 실제 write target이 `0x00acd708 + 0x11c`인지 비교해야 합니다. target match가 확인되기 전까지 field 직접 주입과 Hardlock 응답 변경은 보류합니다.

---

# Task 152: EZ2DJ 4th Runtime Direct Field-Reference Scan Work Log

## Result summary

The complete decrypted runtime `.text` was scanned at the first field-access point for direct displacement `+0x11c` references. The scan succeeded and classified four of 23 candidates as writes. Therefore, the conclusion that no field writer exists in the code is rejected, but which of the four instructions actually writes `0x00acd708 + 0x11c` remains unconfirmed.

## Changes

- The launcher probe now scans the EZ2DJ 4th runtime `.text` only on the first hit of `--null-context-field-access-trace`.
- The scan is bounded to image-base-relative `RVA 0x001000`, virtual size `0x000db022`.
- ModRM disp32 `0x0000011c` candidates are classified as `read`, `write`, or `other`.
- Individual candidate events and a `null_context_field_reference_scan` summary event are recorded.
- Field values, Hardlock responses, and the VFS path are unchanged.

## Verification evidence

- `cmd /c scripts\build_win32.bat`: passed
- `build/windows-x86/bin/Debug/re2dj_unit_tests.exe`: `checks: 1184, failures: 0`
- Real execution log: `logs/windows_x86_launcher_probe/ez2dj4th/20260903-032038-359.jsonl`
- Runtime scan summary: `readable=true`, `.text` `897058` bytes, `candidates=23`, `read_candidates=16`, `write_candidates=4`, `recorded=23`, `capped=false`
- Confirmed write-candidate RVAs: `0x0000fdbd`, `0x0000fde1`, `0x0001825f`, `0x0001dbd3`
- Existing execution order: one source-boundary hit, two dynamic source hits, one target match, one field access, field `0x00000000`, and AV `0x00434137 / 0xc0000005`

## Classification and next step

This result confirms that runtime image code contains direct writes using `+0x11c`. Static candidates may belong to multiple object types sharing the same offset, so they are not treated as the target-object initializer yet. The next task should bounded-trace the four write candidates and compare each hit's receiver register and actual write target with `0x00acd708 + 0x11c`. Until a target match is confirmed, direct field injection and Hardlock-response changes remain deferred.
