# ez2dj4th AV null-context 원인 분리 작업 로그

## 결과 요약

`ez2dj4th`의 `0x00434137 / 0xc0000005` 중단을 runtime instruction과 stack frame 기준으로 분리했습니다. 직접 원인은 Hardlock `DeviceIoControl`의 null 반환이 아니라, 상위 객체의 `+0x11c` 필드가 0인 상태가 caller를 거쳐 null receiver로 전달된 것입니다. 해당 field의 writer와 raw I/O 상태의 간접 연관성은 후속 분석 항목으로 남겼습니다.

## 변경 내용

- raw I/O `IN` trap 로그에 처리 전후 EAX, 다음 EIP, return address와 return-site code window를 추가했습니다.
- AV 로그에 fault thread, 정확한 callee return address, caller local 및 caller return address를 추가했습니다.
- AV stack call target의 runtime jump chain, callsite window, caller의 caller return-site window를 추가했습니다.
- caller의 caller frame에서 `[EBP-0x118]`, 해당 객체의 `+0x11c`, outer return address를 기록하도록 추가 계측했습니다.
- 원본 guest code의 return value, null pointer, branch, IAT, 제품 기본 실행 정책은 수정하지 않았습니다.

## 실행 증거

- `20260903-010101-549.jsonl`: `0x9c40244c` 반환 후 48-step trace가 event cap에 도달했으며 AV 전에는 bounded trace가 종료됐습니다.
- `20260903-010211-910.jsonl`: `0x9c40244c` 1-step trace에서 39회 `0x44c`, 마지막 `EAX=1`과 256-byte output 이후 AV가 재현됐습니다.
- `20260903-010325-894.jsonl`: `0x9c402450` 1-step trace에서 4회 `0x450`, 응답 `0100fafa0010`, `EAX=1` 이후에도 동일 AV가 재현됐습니다.
- `20260903-010808-683.jsonl`: raw I/O가 `0x0103 -> 0x80`, `0x0104 -> 0x80`, `0x0105 -> 0x00`을 반환하고 각각 원본 객체 `+0xb3c`, `+0xb40`, `+0xb44`에 저장하는 runtime 경로를 확인했습니다.
- `20260903-012922-258.jsonl`: AV thread `18688`, `EIP=0x00434137`, `EAX=0`, `ECX=0`, read address `0x14`; caller `[EBP-8]=0`; outer pointer `0x00acd708`; `0x00acd708+0x11c=0`을 확인했습니다.

## 판정

Hardlock과 raw-I/O 경계는 관찰 가능한 성공 상태까지 진행했으며, AV는 그 이후의 상위 객체 초기화·전달 경로에서 발생한 null receiver fault입니다. 이는 “Hardlock 대체 응답이 직접 null을 반환했다”는 가설을 배제하지만, raw I/O가 상위 객체 초기화에 간접 영향을 주는 가능성까지 배제하지는 않습니다.

다음 작업에서는 runtime hardware breakpoint 또는 제한된 writer 추적을 사용해 `0x00acd708+0x11c`의 초기화 명령과 최초 writer를 확인해야 합니다.

## 검증

- `cmd /c scripts\build_win32.bat`: 성공
- `re2dj_windows_x86_launcher_probe.exe` 실제 `4thTrax.chd` 및 staging HDD 실행: 성공적으로 진단 로그 생성, child AV 재현
- 원본 CHD와 staging HDD는 읽기 전용으로 사용했으며 저장소에 추가하지 않았습니다.

---

# ez2dj4th AV Null-Context Causality Work Log

## Result summary

The `ez2dj4th` stop at `0x00434137 / 0xc0000005` was separated using runtime instructions and stack frames. The immediate cause is not a null return from Hardlock `DeviceIoControl`; an upper object's `+0x11c` field is zero, and that state is passed through the caller as a null receiver. The writer of that field and any indirect relation to raw-I/O state remain follow-up analysis items.

## Changes

- Added pre/post EAX, next EIP, return address, and return-site code-window fields to raw-I/O `IN` trap logs.
- Added the fault thread, exact callee return address, caller locals, and caller return address to AV logs.
- Added runtime jump-chain, callsite-window, and caller-caller return-site diagnostics for AV stack calls.
- Added caller-caller frame capture for `[EBP-0x118]`, the candidate object's `+0x11c`, and the outer return address.
- Did not modify the guest return value, null pointer, branch, IAT, or product default execution policy.

## Execution evidence

- `20260903-010101-549.jsonl`: the 48-step trace after the `0x9c40244c` return reached the event cap and ended before an AV in the bounded trace.
- `20260903-010211-910.jsonl`: the one-step `0x9c40244c` trace reproduced the AV after 39 `0x44c` calls, final `EAX=1`, and a 256-byte output.
- `20260903-010325-894.jsonl`: the one-step `0x9c402450` trace reproduced the same AV after four `0x450` calls, response `0100fafa0010`, and `EAX=1`.
- `20260903-010808-683.jsonl`: confirmed runtime raw-I/O returns `0x0103 -> 0x80`, `0x0104 -> 0x80`, and `0x0105 -> 0x00`, stored into original-object offsets `+0xb3c`, `+0xb40`, and `+0xb44`.
- `20260903-012922-258.jsonl`: confirmed AV thread `18688`, `EIP=0x00434137`, `EAX=0`, `ECX=0`, read address `0x14`; caller `[EBP-8]=0`; outer pointer `0x00acd708`; and `0x00acd708+0x11c=0`.

## Classification

Hardlock and raw-I/O boundaries reach their observable success states, while the AV occurs in a later upstream object-initialization/propagation path as a null receiver fault. This rules out the hypothesis that the Hardlock replacement directly returned a null value, but does not rule out an indirect effect of raw I/O on upper-object initialization.

The next task should use a runtime hardware breakpoint or bounded writer trace to identify the initialization instruction and first writer for `0x00acd708+0x11c`.

## Verification

- `cmd /c scripts\build_win32.bat`: passed.
- Running `re2dj_windows_x86_launcher_probe.exe` with the real `4thTrax.chd` and staging HDD produced a diagnostic log and reproduced the child AV.
- The original CHD and staging HDD were read-only inputs and were not added to the repository.
