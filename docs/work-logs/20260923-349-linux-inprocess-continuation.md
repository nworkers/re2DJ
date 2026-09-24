# 작업 349 작업 로그 — Linux i386 in-process 연속 실행 진단 / Task 349 work log — Linux i386 in-process continuation diagnostic

설계: [20260923-349-linux-inprocess-continuation.md](../design/20260923-349-linux-inprocess-continuation.md)
작업 지시: [20260923-349-linux-inprocess-continuation.md](../work-orders/20260923-349-linux-inprocess-continuation.md)
선행: [작업 348 앞선 resolver 진단의 facade 합류](20260923-348-early-resolver-diagnostics-convergence.md)
분석: [4th Linux in-process 첫 import](../analysis/ez2dj4th-linux-inprocess-first-import.md)

## 한국어

### 구현

| 계층 | 변경 |
| --- | --- |
| Linux i386 공용 문맥 | `NativeKernel32Diagnostic`에 RX `INT3` stop stub, 복귀 slot 재지정, facade export 조회, gate 이름, 인자 문자열 읽기 추가. `GetModuleHandleA` 실패도 기록하고 기록 형식을 `GetProcAddress(<name>)` 등으로 통일 |
| Linux i386 runner | 세 진단이 따로 복사하던 fault context 변환을 `CopyNativeFaultObservation`으로 통합 |
| 연속 실행 진단 | `native_continuation_observation.cpp`의 `RunOriginalInProcessContinuation` |
| 결과 타입 | `OriginalApiCall`, `api_calls`, `api_call_count`, `continuation_stop_detail`, 연속 실행 경계 네 개 |
| CLI | `--linux-in-process-continue`, 호출 기록과 경계 출력 |
| `kernel32` module | `GetVersion`이 `kKernel32GuestVersion`(`0x23F00206`) 반환. 단위 테스트와 guest module probe 갱신 |

### 설계에서 바꾼 것 — module 조회 실패는 멈추지 않음

처음 구현은 `GetModuleHandleA` 실패에서도 멈췄다. 그러자 실제 4th CHD가 두 번째 호출 `GetModuleHandleA("user32")`에서 멈춰, 이전 진단들이 이미 지나간 지점보다 앞에서 끝났다. NULL은 정상적인 Win32 반환값이기도 하다. 그래서 module 조회 실패는 기록만 하고, `GetProcAddress`가 0을 반환할 때 멈추도록 바꿨다. 이 조건은 NULL module handle로 호출된 경우도 잡는다. 상세는 설계 문서에 기록했다.

### 검증 — 실제 4th CHD 연속 실행

`roms/ez2dj4th/4thTrax.chd`를 읽기 전용으로 사용했다. 연속 실행은 API 13개를 기록한 뒤 게스트 자신의 `INT3`(`0x00af1135`)에서 SIGTRAP으로 끝났다(`kContinuationFault`). 13개 호출의 표, 레지스터, 확인됨·추정·미확정 구분은 [분석 문서](../analysis/ez2dj4th-linux-inprocess-first-import.md)에 기록했다. 요약하면 다음과 같다.

* "resolver 두 번 → `GetVersion` → `CreateFileA("\\.\NTICE")`" 묶음이 서로 다른 세 코드 영역에서 되풀이된다.
* 13번째 호출(`GetVersion`) 뒤 게스트가 ESI=`'FG'`, EDI=`'JM'` 상태로 자기 `INT3`를 실행한다. SoftICE 검사로 **추정**하며, 다음 경계는 SEH 전달로 보인다.
* `GetModuleHandleA("user32")`는 0을 받았고 게스트는 계속 진행했다.

### 검증 — GetVersion 값 비교

`kKernel32GuestVersion`을 0으로 바꾼 임시 로컬 빌드로 연속 실행을 한 번 더 했다. 13개 호출의 API·순서·복귀 주소와 마지막 SIGTRAP 위치·레지스터가 모두 같았다. 차이는 `GetVersion` 반환값과 ASLR stack 주소뿐이었다. 임시 변경은 되돌렸고 커밋하지 않았다.

되돌리는 과정에서 `git checkout`이 헤더를 HEAD로 되돌려 새로 추가한 `kKernel32GuestVersion` 정의도 함께 지워졌다. 같은 내용을 다시 적용했다. 이후 i386·x64를 다시 빌드하고 모든 검증을 다시 실행했으며, 아래 결과는 그 재실행 결과다.

### 검증 — 기존 진단 회귀

| 진단 | 결과 |
| --- | --- |
| `--linux-in-process-first-import` | return `0x00ae028a`, SIGTRAP `0x00ae028b`. 불변 |
| `--linux-in-process-first-resolver` | return `0x00af0b99`, SIGTRAP `0x00af0b9a`. 불변 |
| `--linux-in-process-getversion-call` | return `0x00aefd82`, SIGTRAP `0x00aefd83`, trace 43 frame. 불변 |
| `--linux-in-process-createfile-call` | `\\.\NTICE`, return `0x00aeffbc`. identity 모두 일치. 불변 |

`GetModuleHandleA` 실패를 기록하게 되면서 앞의 세 진단은 이제 `unresolved lookup: GetModuleHandleA(user32)`를 추가로 출력한다. 경계는 바뀌지 않았다.

### 검증 — 빌드와 테스트

* Linux i386(`linux-x86-debug`) 빌드: 오류·경고 0건. 단위 테스트 `checks: 2111, failures: 0`(`GetVersion` 인코딩 검사 4건 추가).
* Linux i386 `re2dj_linux_native_guest_module_probe`: 처음에는 `GetVersion`=0을 가정한 검사 때문에 종료 코드 1이었다. `kKernel32GuestVersion`과 비교하도록 고친 뒤 종료 코드 0. `re2dj_linux_native_in_process_probe`: 종료 코드 0.
* Linux x64(`linux-x64-debug`) 빌드: 오류 0건. 단위 테스트 `checks: 2111, failures: 0`. 연속 실행 진단은 "requires an i386 host"로 거절한다.
* Windows x86 Debug: 컴파일 오류 0건. 단위 테스트 실행 파일이 새로 빌드되어 `checks: 2111, failures: 0`. 다만 `re2dj_windows_injected_runtime.dll` link가 `LNK1168`(파일을 쓰기용으로 열 수 없음)로 두 번 실패했다. `tasklist /m`으로는 그 DLL을 연 프로세스를 찾지 못해, 잠금 주체는 확인하지 못했다. 이 작업의 변경은 Linux 전용 코드와 `kernel32` facade module이며, Windows injected runtime은 facade를 쓰지 않는다.

### 남은 범위

* guest `INT3`를 `EXCEPTION_BREAKPOINT`로 게스트 SEH 체인에 전달하는 것. 그 시점의 SEH 등록 여부부터 확인해야 한다.
* 정적 import된 DLL(`user32` 등)의 `GetModuleHandleA` 정책.
* API 호출 없는 무한 루프는 이 진단이 제한하지 않는다.
* 원래 캐비닛 OS의 `GetVersion` 값은 미확정이다.

## English

Design: [20260923-349-linux-inprocess-continuation.md](../design/20260923-349-linux-inprocess-continuation.md)
Work order: [20260923-349-linux-inprocess-continuation.md](../work-orders/20260923-349-linux-inprocess-continuation.md)
Prerequisite: [Task 348, early resolver diagnostics convergence](20260923-348-early-resolver-diagnostics-convergence.md)
Analysis: [4th Linux in-process first import](../analysis/ez2dj4th-linux-inprocess-first-import.md)

### Implementation

| Layer | Change |
| --- | --- |
| Linux i386 shared context | `NativeKernel32Diagnostic` gains an RX `INT3` stop stub, return-slot redirection, facade-export lookup, gate naming, and argument-string reads; `GetModuleHandleA` failures are recorded too, with text unified as `GetProcAddress(<name>)` and so on |
| Linux i386 runner | The fault-context conversion the diagnostics each copied is now `CopyNativeFaultObservation` |
| Continuation diagnostic | `RunOriginalInProcessContinuation` in `native_continuation_observation.cpp` |
| Result types | `OriginalApiCall`, `api_calls`, `api_call_count`, `continuation_stop_detail`, and four continuation boundaries |
| CLI | `--linux-in-process-continue` with call-log and boundary output |
| `kernel32` module | `GetVersion` returns `kKernel32GuestVersion` (`0x23F00206`); unit test and guest module probe updated |

### Changed from the design — a failed module lookup does not stop

The first implementation also stopped on a failed `GetModuleHandleA`, and the real 4th CHD stopped at the second call, `GetModuleHandleA("user32")`, ahead of points earlier diagnostics had already passed. NULL is also a legitimate Win32 result. A failed module lookup is now only recorded, and the run stops when `GetProcAddress` returns zero, which also covers a NULL module handle. Details are in the design.

### Verification — real 4th CHD continuation

`roms/ez2dj4th/4thTrax.chd` was used read-only. The continuation recorded 13 APIs and ended with SIGTRAP on the guest's own `INT3` at `0x00af1135` (`kContinuationFault`). The call table, registers, and the confirmed/inferred/unresolved split are in the [analysis](../analysis/ez2dj4th-linux-inprocess-first-import.md). In short: a "two resolver calls → `GetVersion` → `CreateFileA("\\.\NTICE")`" group repeats across three code regions; after call 13 (`GetVersion`) the guest executes its own `INT3` with ESI `'FG'` and EDI `'JM'`, **inferred** to be a SoftICE check whose next boundary is SEH delivery; and `GetModuleHandleA("user32")` received zero while the guest kept going.

### Verification — GetVersion value comparison

A temporary local build with `kKernel32GuestVersion` set to zero ran the continuation again. The 13 calls' APIs, order, return addresses, and the final SIGTRAP location and registers were identical; only the `GetVersion` result and ASLR stack addresses differed. The temporary change was reverted and not committed.

While reverting, `git checkout` reset the header to HEAD and also removed the newly added `kKernel32GuestVersion` definition. The same content was re-applied, i386 and x64 were rebuilt, and every check was rerun; the results below are from that rerun.

### Verification — regression of the existing diagnostics

First-import (return `0x00ae028a`, SIGTRAP `0x00ae028b`), first-resolver (return `0x00af0b99`, SIGTRAP `0x00af0b9a`), GetVersion-call (return `0x00aefd82`, SIGTRAP `0x00aefd83`, 43 trace frames), and CreateFileA (`\\.\NTICE`, return `0x00aeffbc`, all identities matching) are unchanged. Because `GetModuleHandleA` failures are now recorded, the three resolver diagnostics additionally print `unresolved lookup: GetModuleHandleA(user32)`; their boundaries did not change.

### Verification — builds and tests

* Linux i386 (`linux-x86-debug`) build with no errors or warnings; unit tests `checks: 2111, failures: 0` (four new `GetVersion` encoding checks).
* Linux i386 `re2dj_linux_native_guest_module_probe` first exited 1 because it assumed `GetVersion` returns zero; after comparing against `kKernel32GuestVersion` it exits zero. `re2dj_linux_native_in_process_probe` exits zero.
* Linux x64 (`linux-x64-debug`) build with no errors; unit tests `checks: 2111, failures: 0`. The continuation diagnostic refuses with "requires an i386 host".
* Windows x86 Debug compiled with no errors, and the rebuilt unit-test binary reports `checks: 2111, failures: 0`. Linking `re2dj_windows_injected_runtime.dll` failed twice with `LNK1168` (cannot open for writing); `tasklist /m` found no process holding the DLL, so the lock holder was not identified. This task's changes are Linux-only code and the `kernel32` facade module, which the Windows injected runtime does not use.

### Remaining scope

Delivering the guest `INT3` to the guest SEH chain as `EXCEPTION_BREAKPOINT`, starting with whether an SEH handler is registered at that point; a `GetModuleHandleA` policy for statically imported DLLs such as `user32`; infinite loops with no API calls, which this diagnostic does not bound; and the original cabinet OS's `GetVersion` value, which remains unresolved.
