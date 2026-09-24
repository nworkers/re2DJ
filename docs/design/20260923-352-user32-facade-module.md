# 작업 352: `user32` facade module 설계 / Task 352: `user32` facade module design

선행: [작업 351 작업 로그](../work-logs/20260923-351-linux-guest-seh-dispatch.md), [작업 340 계획](../work-orders/20260921-340-guest-pe-compatibility-modules.md)
분석: [4th Linux in-process 첫 import](../analysis/ez2dj4th-linux-inprocess-first-import.md), [import surface](../analysis/ez2dj-import-surface.md)

## 배경 / Background

작업 351 이후 실제 4th CHD 연속 실행은 API 15개를 기록하고 `#0015 GetProcAddress(0, "GetActiveWindow")`에서 멈춥니다. module 인자 0은 `#0002 GetModuleHandleA("user32")`가 받은 값입니다. facade에 `user32`가 없어서 그 호출이 0을 돌려줬고, 게스트는 이 0을 저장해 두었다가 resolver에 넘겼습니다.

*After Task 351, the real 4th CHD continuation records 15 APIs and stops at `#0015 GetProcAddress(0, "GetActiveWindow")`. The zero module argument is the value `#0002 GetModuleHandleA("user32")` received: the facade has no `user32`, so that call returned zero, and the guest kept it and passed it to the resolver.*

4th EXE는 `USER32.dll`을 정적 import하므로(**확인됨**, [import surface §11](../analysis/ez2dj-import-surface.md)) 실제 Windows에서는 process 시작 시점에 `user32`가 이미 load되어 있습니다. 그러면 `GetModuleHandleA("user32")`는 NULL이 아닌 값을 돌려줍니다.

*The 4th EXE statically imports `USER32.dll` (**confirmed**, [import surface §11](../analysis/ez2dj-import-surface.md)), so on real Windows `user32` is already loaded when the process starts and `GetModuleHandleA("user32")` returns non-NULL.*

## 결정 / Decision

1. 플랫폼 중립 `user32` module descriptor(`MakeUser32ModuleDescriptor`)를 `kernel32`와 같은 방식으로 `src/hle/modules/`에 추가합니다. 이름은 `user32.dll`, 별칭은 `user32`입니다.
2. export는 **실행으로 요청이 확인된 `GetActiveWindow` 하나만** 둡니다. 작업 340의 "확인되지 않은 export를 성공 stub으로 등록하지 않는다" 제약을 따릅니다.
3. Linux i386 진단 문맥(`NativeKernel32Diagnostic::Setup`)이 `kernel32` 뒤에 `user32` facade도 등록합니다. 그 뒤 `GetModuleHandleA("user32")`는 registry의 `user32` base를 돌려주고, `GetProcAddress(user32, "GetActiveWindow")`는 facade thunk를 돌려줍니다.
4. `FindGuestModule`은 지금 어떤 module을 찾든 `kernel32_base_` 관측값을 덮어씁니다. 이를 `kernel32`를 찾았을 때만 기록하도록 고칩니다. 그렇지 않으면 `user32` 조회가 resolver identity 보고를 오염시킵니다.

*Decisions: (1) add a platform-neutral `user32` module descriptor (`MakeUser32ModuleDescriptor`) under `src/hle/modules/` in the same form as `kernel32`, named `user32.dll` with alias `user32`; (2) give it **only `GetActiveWindow`**, the one export whose request a run has confirmed, following Task 340's rule against registering unconfirmed exports as success stubs; (3) have the Linux i386 diagnostic context (`NativeKernel32Diagnostic::Setup`) register the `user32` facade after `kernel32`, so `GetModuleHandleA("user32")` returns the registry's `user32` base and `GetProcAddress(user32, "GetActiveWindow")` returns the facade thunk; (4) `FindGuestModule` currently overwrites the observed `kernel32_base_` whichever module it finds, so record it only when `kernel32` is found, or a `user32` lookup would corrupt the resolver-identity report.*

```mermaid
sequenceDiagram
    participant G as guest (protected 4th)
    participant K as kernel32 facade
    participant R as GuestModuleRegistry
    participant U as user32 facade
    G->>K: #0002 GetModuleHandleA("user32")
    K->>R: FindModule("user32")
    R-->>K: user32 base (new; was 0)
    K-->>G: user32 base
    G->>K: #0015 GetProcAddress(user32, "GetActiveWindow")
    K->>R: FindExport(user32, "GetActiveWindow")
    R-->>G: facade thunk
    G->>U: GetActiveWindow()
    U-->>G: NULL (no guest window exists)
```

## `GetActiveWindow` 반환값 / `GetActiveWindow` result

`GetActiveWindow`는 호출 thread의 message queue에 붙은 active window를 돌려주고, 없으면 NULL을 돌려줍니다([Microsoft 문서](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getactivewindow)). 현재 facade에는 창을 만드는 export가 없으므로 게스트 창은 존재할 수 없습니다. 따라서 NULL은 stub이 아니라 **현재 상태에서 정확한 결과**입니다. 인자는 없고 stdcall입니다.

*`GetActiveWindow` returns the active window attached to the calling thread's message queue, or NULL if there is none ([Microsoft documentation](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getactivewindow)). The facade has no window-creating export, so no guest window can exist, and NULL is therefore **the accurate result for the current state**, not a stub. It takes no arguments and is stdcall.*

창 생성 export(`CreateWindowExA` 등)를 추가하는 작업은 이 handler를 window service 조회로 바꿔야 합니다. handler 주석과 이 문서에 그 조건을 남깁니다.

*The task that adds a window-creating export such as `CreateWindowExA` must turn this handler into a window-service query; the handler comment and this document record that condition.*

## 정적 import는 바꾸지 않음 / Static imports are unchanged

4th의 packed 정적 import 중 `USER32.dll`은 `MessageBoxA`, `UpdateWindow`입니다. 이 둘은 호출이 확인되지 않았으므로 facade에 넣지 않습니다. 정적 IAT rebinding은 registry에 있는 export만 바꾸므로 두 slot은 기존 import gate에 남습니다. 호출되면 지금처럼 `kContinuationUnhandledImport`로 멈춥니다.

*Of 4th's packed static imports, `USER32.dll` contributes `MessageBoxA` and `UpdateWindow`. No call to either is confirmed, so they stay out of the facade. Static IAT rebinding only rewrites exports present in the registry, so both slots keep their existing import gates, and a call still stops with `kContinuationUnhandledImport`.*

## 배치 / Placement

두 facade 모두 `kDefaultNativeGuestModuleBase`(`0x6F000000`)에서 후보 탐색을 시작합니다. `kernel32`가 그 주소를 차지하므로 `user32`는 기존 충돌 회피 규칙에 따라 64 KiB 아래 후보로 내려갑니다. base 값은 고정 계약이 아닙니다. 게스트는 `GetModuleHandleA` 결과로만 이 값을 얻습니다.

*Both facades start the candidate search at `kDefaultNativeGuestModuleBase` (`0x6F000000`). `kernel32` takes that address, so `user32` falls to a lower 64 KiB candidate under the existing collision-avoidance rule. The base is not a fixed contract; the guest only obtains it from `GetModuleHandleA`.*

## 범위 밖 / Out of scope

* window·message queue service, `CreateWindowExA` 등 창 API. `GetActiveWindow` 뒤에 게스트가 요청하는 다음 경계는 연속 실행 결과로 기록만 합니다.
* 진단 class 이름(`NativeKernel32Diagnostic`) 변경. 여러 module을 담게 되지만, 이름 변경은 이 작업의 동작 변화와 분리합니다.
* Windows injected runtime. 이 경로는 facade module을 쓰지 않습니다.

*Out of scope: window and message-queue services and window APIs such as `CreateWindowExA` (the guest's next request after `GetActiveWindow` is only recorded from the continuation); renaming the diagnostic class `NativeKernel32Diagnostic`, which now holds more than one module but is kept separate from this behavior change; and the Windows injected runtime, which does not use facade modules.*

## 검증 전략 / Verification strategy

* 단위 테스트 `user32_module_test.cpp`: descriptor 형식(이름, 별칭, export 하나, stdcall, 인자 0)과 handler 반환값 NULL.
* Linux i386 `re2dj_linux_native_guest_module_probe`에 `user32` facade 등록, `kernel32`와 다른 base, `GetActiveWindow` export가 mapping 안에 있는지 검사를 추가합니다.
* Linux i386·x64, Windows x86 빌드와 단위 테스트.
* 실제 4th CHD `--linux-in-process-continue`: `#0002`가 `user32` base를 받고, `#0015`가 `GetActiveWindow` thunk를 받아 호출이 NULL을 반환한 뒤의 다음 경계를 기록합니다. `kernel32` resolver identity 보고가 그대로 일치하는지도 확인합니다.
* 기존 네 resolver 진단의 경계가 바뀌지 않았는지 확인합니다.

*Verification: a `user32_module_test.cpp` unit test for the descriptor shape (name, alias, one export, stdcall, zero arguments) and the NULL handler result; extend the Linux i386 `re2dj_linux_native_guest_module_probe` to register the `user32` facade and check that its base differs from `kernel32` and that `GetActiveWindow` lies inside its mapping; Linux i386/x64 and Windows x86 builds and unit tests; on the real 4th CHD `--linux-in-process-continue`, record that `#0002` receives the `user32` base, `#0015` receives the `GetActiveWindow` thunk, and the next boundary after that call returns NULL, and confirm the `kernel32` resolver-identity report still matches; and confirm that the four earlier resolver diagnostics keep their boundaries.*
