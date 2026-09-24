# 작업 348 작업 로그 — 앞선 resolver 진단의 facade 합류 / Task 348 work log — Early resolver diagnostics convergence

설계: [20260923-348-early-resolver-diagnostics-convergence.md](../design/20260923-348-early-resolver-diagnostics-convergence.md)
작업 지시: [20260923-348-early-resolver-diagnostics-convergence.md](../work-orders/20260923-348-early-resolver-diagnostics-convergence.md)
선행: [작업 344 게스트 모듈 resolver 합류](20260922-344-guest-module-resolver-convergence.md)
분석: [4th Linux in-process 첫 import](../analysis/ez2dj4th-linux-inprocess-first-import.md)

## 한국어

### 구현

| 계층 | 변경 |
| --- | --- |
| Linux i386 공용 문맥 | `x86/native_kernel32_diagnostic.{h,cpp}`의 `NativeKernel32Diagnostic`. facade 등록·재결합, `ImportCallServices`, identity 관측, 첫 미처리 정적 import와 첫 미해석 `GetProcAddress` 요청 기록 |
| Linux i386 module set | `NativeGuestModuleSet::FindGate(GuestAddress)` 추가 |
| Linux i386 trace | `StopNativeInstructionTrace()` 추가 |
| 결과 타입 | identity 필드를 `OriginalCreateFileObservation`에서 `OriginalResolverIdentity`로 이동, `registry_get_version`과 `unhandled_import` 추가 |
| first resolver 진단 | pseudo handle과 EAX=1 응답 제거. `GetProcAddress` facade gate의 `GetVersion` 요청 복귀 지점에서 제한 |
| GetVersion call 진단 | pseudo handle과 `0xF1000001` dynamic thunk 제거. `GetVersion` facade gate 호출 복귀 지점에서 제한 |
| CreateFileA 진단 | 공용 문맥으로 이동. 동작 불변 |
| CLI | 세 진단 모두 identity, 미처리 정적 import, 미해석 `GetProcAddress` 요청 출력. `kGetVersionCalled`에서도 instruction trace 출력 |

`src/platform/linux/`에는 `0x7F000001`과 `0xF1000001`이 남지 않는다. `NativeDynamicThunk`는 `native_in_process_probe`가 계속 쓰므로 남겨 두었다.

### 구현 중 발견 — trace가 진단 INT3를 소비함

facade로 옮긴 GetVersion-call 진단을 처음 실행했을 때 경계 판정이 실패했다. 판정 실패 메시지에 상태를 담아 다시 실행한 결과는 `called=1`, 복귀 주소 `0x00aefd82`, SIGSEGV, EIP `0x00aefd84`였다. 즉 `GetVersion`은 호출되었지만 복귀 지점의 `INT3`가 SIGTRAP 경계가 되지 못하고 실행이 +2까지 진행했다.

원인은 import bridge의 `ResumeNativeInstructionTrace`가 같은 복귀 지점에 trace breakpoint를 다시 걸면서 진단이 쓴 `0xCC`를 원래 바이트로 저장한 것이다. 복귀 뒤 trace가 `0xCC`를 되쓰고 single-step을 시작하며, 두 번째 `0xCC`는 trace frame으로 소비된다. 이전 구현은 `GetVersion` 호출 전에 멈췄으므로 드러나지 않았다. `StopNativeInstructionTrace()`를 추가해 진단이 `INT3`를 두기 전에 trace를 끝내도록 했다. 상세는 [설계 문서](../design/20260923-348-early-resolver-diagnostics-convergence.md)에 기록했다.

### 검증 — 실제 4th CHD

`roms/ez2dj4th/4thTrax.chd`를 읽기 전용으로 사용했다. 변경 전 기준선도 같은 빌드 트리에서 먼저 실행했다.

| 진단 | 변경 전 | 변경 후 |
| --- | --- | --- |
| `--linux-in-process-first-import` | return `0x00ae028a`, SIGTRAP `0x00ae028b` | 동일 |
| `--linux-in-process-first-resolver` | return `0x00af0b99`, SIGTRAP `0x00af0b9a` | 동일. `GetVersion` registry `0x6f002026` = 게스트가 받은 값 |
| `--linux-in-process-getversion-call` | 미도달. SIGSEGV EIP `0x00af0c22`, fault stack에 `0x7f000001` | **`GetVersion` 호출.** return `0x00aefd82`, SIGTRAP `0x00aefd83` |
| `--linux-in-process-createfile-call` | `\\.\NTICE`, return `0x00aeffbc` | 동일. identity `0x6f000000`/`0x6f002039`/`0x6f002026` 모두 일치 |

GetVersion-call 진단의 trace는 `0x00af0b99`에서 시작해 43개 frame 뒤 `GetVersion` facade thunk `0x6f002026`에 진입했다. 두 번째 resolver 요청은 이제 `CreateFileA` thunk `0x6f002039`를 돌려주고, 이전 null read 지점 `0x00af0c22`의 `MOV CL, byte ptr [EAX]`는 그 주소에서 `0x68`을 읽고 fault 없이 진행한다. frame별 근거와 확인됨·추정·미확정 구분은 [분석 문서](../analysis/ez2dj4th-linux-inprocess-first-import.md)에 기록했다.

세 진단 모두 미처리 정적 import나 미해석 `GetProcAddress` 요청은 관측되지 않았다.

### 검증 — 빌드와 테스트

* Windows x86 Debug 빌드: 오류 0건. 단위 테스트 `checks: 2107, failures: 0`. 마지막 CLI 출력 추가 뒤 재빌드는 컴파일은 통과했으나, 실행 중이던 `re2dj.exe`가 파일을 잡고 있어 link가 `LNK1168`로 실패했다. 그 변경은 `main.cpp`의 `#if defined(__linux__)` 블록 안에만 있어 Windows 산출물에 영향이 없다.
* Linux i386(`linux-x86-debug`) 빌드: 오류·경고 0건. 단위 테스트 `checks: 2107, failures: 0`.
* Linux i386 `re2dj_linux_native_guest_module_probe`, `re2dj_linux_native_in_process_probe`: 종료 코드 0.
* Linux x64(`linux-x64-debug`) 빌드: 오류 0건. 단위 테스트 `checks: 2107, failures: 0`. GetVersion-call 진단은 기존대로 "requires an i386 host"로 거절한다. 서드파티 SDL3 wayland 경고는 이 작업과 무관하다.

### 남은 범위

* `GetVersion` handler는 0을 반환한다. 원본이 기대하는 OS 버전 값과 그에 따른 분기는 미확정이다.
* `0x00af0c22`의 API 첫 바이트 읽기가 무엇과 비교되는지는 확인하지 않았다.
* `\\.\NTICE` 이후의 guest handle 의미, facade 밖 정적 import의 HLE는 작업 340의 후속 범위다.

## English

Design: [20260923-348-early-resolver-diagnostics-convergence.md](../design/20260923-348-early-resolver-diagnostics-convergence.md)
Work order: [20260923-348-early-resolver-diagnostics-convergence.md](../work-orders/20260923-348-early-resolver-diagnostics-convergence.md)
Prerequisite: [Task 344, guest module resolver convergence](20260922-344-guest-module-resolver-convergence.md)
Analysis: [4th Linux in-process first import](../analysis/ez2dj4th-linux-inprocess-first-import.md)

### Implementation

| Layer | Change |
| --- | --- |
| Linux i386 shared context | `NativeKernel32Diagnostic` in `x86/native_kernel32_diagnostic.{h,cpp}`: facade registration and rebinding, `ImportCallServices`, identity observation, and recording the first unhandled static import and first unresolved `GetProcAddress` request |
| Linux i386 module set | `NativeGuestModuleSet::FindGate(GuestAddress)` |
| Linux i386 trace | `StopNativeInstructionTrace()` |
| Result types | Identity fields moved from `OriginalCreateFileObservation` into `OriginalResolverIdentity`, adding `registry_get_version` and `unhandled_import` |
| First-resolver diagnostic | Pseudo handle and EAX=1 reply removed; bounded at the return of the `GetVersion` request through the `GetProcAddress` facade gate |
| GetVersion-call diagnostic | Pseudo handle and `0xF1000001` dynamic thunk removed; bounded at the return of the `GetVersion` facade-gate call |
| CreateFileA diagnostic | Moved onto the shared context, behavior unchanged |
| CLI | Identity, unhandled static import, and unresolved `GetProcAddress` request printed for all three; instruction trace also printed at `kGetVersionCalled` |

No `0x7F000001` or `0xF1000001` remains under `src/platform/linux/`. `NativeDynamicThunk` stays because `native_in_process_probe` still uses it.

### Found during implementation — the trace consumed the diagnostic INT3

The first run of the facade-based GetVersion-call diagnostic failed its boundary check. With the state added to the failure message, the rerun showed `called=1`, return address `0x00aefd82`, SIGSEGV at EIP `0x00aefd84`: `GetVersion` was called, but the return-site `INT3` did not become a SIGTRAP boundary and execution ran on to +2.

The import bridge's `ResumeNativeInstructionTrace` re-arms a trace breakpoint at the same return site and saves the diagnostic's `0xCC` as the original byte. After the return, the trace writes that `0xCC` back and starts single-stepping, and the second `0xCC` is consumed as a trace frame. The previous implementation stopped before calling `GetVersion`, so this never surfaced. `StopNativeInstructionTrace()` now lets the diagnostic end the trace before placing its `INT3`. Details are in the [design](../design/20260923-348-early-resolver-diagnostics-convergence.md).

### Verification — the real 4th CHD

`roms/ez2dj4th/4thTrax.chd` was used read-only, with a pre-change baseline run first from the same build tree. First-import (return `0x00ae028a`, SIGTRAP `0x00ae028b`), first-resolver (return `0x00af0b99`, SIGTRAP `0x00af0b9a`), and CreateFileA (`\\.\NTICE`, return `0x00aeffbc`) are unchanged, and first-resolver now shows the guest-received `GetVersion` address equal to the registry's `0x6f002026`. CreateFileA identity agrees at `0x6f000000`, `0x6f002039`, and `0x6f002026`.

The GetVersion-call diagnostic changed from a pre-call SIGSEGV at `0x00af0c22` (with `0x7f000001` in the fault stack) to an **actual `GetVersion` call**, bounded at return `0x00aefd82` with SIGTRAP at `0x00aefd83`. Its trace starts at `0x00af0b99` and enters the `GetVersion` facade thunk `0x6f002026` after 43 frames. The second resolver request now returns the `CreateFileA` thunk `0x6f002039`, and `MOV CL, byte ptr [EAX]` at the former null-read site `0x00af0c22` reads `0x68` from it and continues. Frame-level evidence, split into confirmed, inferred, and unresolved, is in the [analysis](../analysis/ez2dj4th-linux-inprocess-first-import.md).

No unhandled static import or unresolved `GetProcAddress` request was observed in any of the three diagnostics.

### Verification — builds and tests

* Windows x86 Debug build with no errors; unit tests `checks: 2107, failures: 0`. After the final CLI output addition, the rebuild compiled but linking failed with `LNK1168` because a running `re2dj.exe` held the file. That change sits entirely inside `main.cpp`'s `#if defined(__linux__)` block and does not affect Windows output.
* Linux i386 (`linux-x86-debug`) build with no errors or warnings; unit tests `checks: 2107, failures: 0`.
* Linux i386 `re2dj_linux_native_guest_module_probe` and `re2dj_linux_native_in_process_probe` exit zero.
* Linux x64 (`linux-x64-debug`) build with no errors; unit tests `checks: 2107, failures: 0`. The GetVersion-call diagnostic still refuses with "requires an i386 host". The vendored SDL3 wayland warnings are unrelated.

### Remaining scope

The `GetVersion` handler returns zero, so the OS-version value the original expects and its branch remain unresolved. What the first-byte read at `0x00af0c22` is compared against was not established. Guest-handle semantics after `\\.\NTICE` and HLE for static imports outside the facade remain Task 340 follow-up work.
