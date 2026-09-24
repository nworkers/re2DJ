# 작업 352 작업 로그 — `user32` facade module / Task 352 work log — `user32` facade module

설계: [20260923-352-user32-facade-module.md](../design/20260923-352-user32-facade-module.md)
작업 지시: [20260923-352-user32-facade-module.md](../work-orders/20260923-352-user32-facade-module.md)
선행: [작업 351 게스트 SEH 디스패치](20260923-351-linux-guest-seh-dispatch.md)
분석: [4th Linux in-process 첫 import](../analysis/ez2dj4th-linux-inprocess-first-import.md)

## 한국어

### 시작 상태

작업 351까지 들어간 코드를 `linux-x86-debug`로 빌드하고 실제 4th CHD에서 `--linux-in-process-continue`를 실행했다. API 15개를 기록한 뒤 `#0015 GetProcAddress(0, "GetActiveWindow")`에서 unresolved lookup으로 멈췄다. module 인자 0은 `#0002 GetModuleHandleA("user32")`가 받은 값이다.

### 구현

| 계층 | 변경 |
| --- | --- |
| 공용 HLE module | `include/re2dj/hle/modules/user32_module.h`, `src/hle/modules/user32_module.cpp`. `user32.dll`(별칭 `user32`), export는 `GetActiveWindow`(stdcall, 인자 0) 하나. handler는 NULL 반환 |
| Linux i386 진단 문맥 | `NativeKernel32Diagnostic::Setup`이 `kernel32` 다음에 `user32` facade를 등록 |
| 관측값 수정 | `FindGuestModule`은 전에 어떤 module을 찾든 `kernel32_base_`를 덮어썼다. 이제 `kernel32` base를 찾았을 때만 기록 |
| 단위 테스트 | `tests/unit/user32_module_test.cpp`: descriptor 형식, handler NULL 반환, null result 거부 |
| Linux i386 probe | `re2dj_linux_native_guest_module_probe`가 `user32`를 등록해 `kernel32`와 겹치지 않는 별도 module인지, `GetActiveWindow`가 `user32` mapping 안에만 있는지 확인. bridge를 거쳐 thunk를 실제로 호출해 반환값 0과 stack 균형도 확인 |
| 빌드 | CMake core 목록과 단위 테스트 목록에 두 파일 추가 |

### 검증 — 빌드와 테스트

* Linux i386(`linux-x86-debug`, `-DRE2DJ_WARNINGS_AS_ERRORS=ON`): 오류·경고 0건. 단위 테스트 `checks: 2127, failures: 0`.
* Linux i386 `re2dj_linux_native_guest_module_probe`, `re2dj_linux_native_in_process_probe`: 둘 다 종료 코드 0.
* Linux x64(`linux-x64-debug`, `-DRE2DJ_WARNINGS_AS_ERRORS=ON`): 오류·경고 0건. 단위 테스트 `checks: 2127, failures: 0`. 연속 실행 진단은 "requires an i386 host"로 거절한다.
* Windows x86 Debug: 모든 target 빌드 성공(작업 349의 `LNK1168`은 이번에는 재현되지 않음). 단위 테스트 `checks: 2127, failures: 0`.

### 검증 — 실제 4th CHD 연속 실행

`roms/ez2dj4th/4thTrax.chd`를 읽기 전용으로 사용했다. API 16개를 기록한 뒤 `#0016 GetProcAddress(0x6f000000, "ExitProcess")`(caller 복귀 `0x00ae7030`)에서 `kContinuationUnresolvedLookup`으로 멈췄다.

* `#0002 GetModuleHandleA("user32")` → `0x6eff0000`. `kernel32`가 `0x6f000000`을 차지해 한 후보 아래로 배치됐다.
* `#0015 GetProcAddress(0x6eff0000, "GetActiveWindow")` → `0x6eff2000`.
* 게스트는 `GetActiveWindow` thunk를 호출하지 않은 채 `#0016`으로 넘어갔다.
* `#0001`~`#0014`와 SEH 전달(handler `0x00af159b`, 재개 `0x00af11af`)은 작업 351과 같다.
* `kernel32` resolver identity 세 값이 모두 일치한다. 이전 실행에서 출력되던 `unresolved lookup: GetModuleHandleA(user32)`는 사라지고 `GetProcAddress(ExitProcess)`로 바뀌었다.

해석(확인됨/추정/미확정)은 [분석 문서](../analysis/ez2dj4th-linux-inprocess-first-import.md)에 기록했다.

### 검증 — 기존 진단 회귀

| 진단 | 결과 |
| --- | --- |
| `--linux-in-process-first-import` | return `0x00ae028a`, SIGTRAP `0x00ae028b`. 불변 |
| `--linux-in-process-first-resolver` | `GetVersion`, return `0x00af0b99`, SIGTRAP `0x00af0b9a`. 불변 |
| `--linux-in-process-getversion-call` | return `0x00aefd82`, SIGTRAP `0x00aefd83`, trace 43 frame. 불변 |
| `--linux-in-process-createfile-call` | `\\.\NTICE`, return `0x00aeffbc`. identity 모두 일치. 불변 |

### 남은 범위

* 다음 경계: `kernel32` facade의 `ExitProcess`.
* window·message queue service. 창 생성 export를 추가할 때 `GetActiveWindow` handler도 바꿔야 한다.
* `user32` 정적 import `MessageBoxA`, `UpdateWindow`는 호출이 확인되지 않아 facade에 넣지 않았다.
* 진단 class 이름 `NativeKernel32Diagnostic`은 이제 두 module을 담지만 이름은 그대로 두었다.

## English

### Starting state

The code through Task 351 was built with `linux-x86-debug`, and `--linux-in-process-continue` on the real 4th CHD recorded 15 APIs before stopping with an unresolved lookup at `#0015 GetProcAddress(0, "GetActiveWindow")`. The zero module argument is what `#0002 GetModuleHandleA("user32")` had received.

### Implementation

| Layer | Change |
| --- | --- |
| Shared HLE module | `include/re2dj/hle/modules/user32_module.h` and `src/hle/modules/user32_module.cpp`: `user32.dll` (alias `user32`) with a single export, `GetActiveWindow` (stdcall, no arguments), whose handler returns NULL |
| Linux i386 diagnostic context | `NativeKernel32Diagnostic::Setup` registers the `user32` facade after `kernel32` |
| Observation fix | `FindGuestModule` used to overwrite `kernel32_base_` whichever module it found; it now records it only for the `kernel32` base |
| Unit test | `tests/unit/user32_module_test.cpp`: descriptor shape, NULL handler result, rejection of a null result |
| Linux i386 probe | `re2dj_linux_native_guest_module_probe` registers `user32` and checks that it is a separate module not overlapping `kernel32` and that `GetActiveWindow` lies only in the `user32` mapping; it also calls the thunk through the bridge and checks the zero result and stack balance |
| Build | Both files added to the CMake core list and the unit-test list |

### Verification — builds and tests

* Linux i386 (`linux-x86-debug`, `-DRE2DJ_WARNINGS_AS_ERRORS=ON`): no errors or warnings; unit tests `checks: 2127, failures: 0`.
* Linux i386 `re2dj_linux_native_guest_module_probe` and `re2dj_linux_native_in_process_probe` both exit zero.
* Linux x64 (`linux-x64-debug`, `-DRE2DJ_WARNINGS_AS_ERRORS=ON`): no errors or warnings; unit tests `checks: 2127, failures: 0`. The continuation diagnostic refuses with "requires an i386 host".
* Windows x86 Debug: every target built (Task 349's `LNK1168` did not recur); unit tests `checks: 2127, failures: 0`.

### Verification — real 4th CHD continuation

`roms/ez2dj4th/4thTrax.chd` was used read-only. The run recorded 16 APIs and stopped with `kContinuationUnresolvedLookup` at `#0016 GetProcAddress(0x6f000000, "ExitProcess")` (caller return `0x00ae7030`). `#0002 GetModuleHandleA("user32")` received `0x6eff0000`, one candidate below `kernel32` at `0x6f000000`; `#0015 GetProcAddress(0x6eff0000, "GetActiveWindow")` received `0x6eff2000`; and the guest moved on to `#0016` without calling the `GetActiveWindow` thunk. Calls `#0001`–`#0014` and SEH delivery (handler `0x00af159b`, resume `0x00af11af`) match Task 351. All three `kernel32` resolver-identity values match, and the previous `unresolved lookup: GetModuleHandleA(user32)` line has become `GetProcAddress(ExitProcess)`. The confirmed/inferred/unresolved interpretation is in the [analysis](../analysis/ez2dj4th-linux-inprocess-first-import.md).

### Verification — regression of the existing diagnostics

First-import (return `0x00ae028a`, SIGTRAP `0x00ae028b`), first-resolver (`GetVersion`, return `0x00af0b99`, SIGTRAP `0x00af0b9a`), GetVersion-call (return `0x00aefd82`, SIGTRAP `0x00aefd83`, 43 trace frames), and CreateFileA (`\\.\NTICE`, return `0x00aeffbc`, all identities matching) are unchanged.

### Remaining scope

The next boundary is `ExitProcess` in the `kernel32` facade. Window and message-queue services remain, and adding a window-creating export must also change the `GetActiveWindow` handler. `user32`'s static imports `MessageBoxA` and `UpdateWindow` stay out of the facade because no call to them is confirmed. The diagnostic class keeps the name `NativeKernel32Diagnostic` although it now holds two modules.
