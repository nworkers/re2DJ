# 작업 352 작업 지시서 — `user32` facade module / Task 352 work order — `user32` facade module

설계: [20260923-352-user32-facade-module.md](../design/20260923-352-user32-facade-module.md)
선행: [작업 351 작업 로그](../work-logs/20260923-351-linux-guest-seh-dispatch.md)

[작업 352 설계](../design/20260923-352-user32-facade-module.md)에 따라 `user32` facade module을 추가합니다. 실제 4th CHD의 `GetModuleHandleA("user32")`가 module handle을 받고 `GetProcAddress(user32, "GetActiveWindow")`가 해석되도록 하고, 그 다음 경계를 기록합니다.

*Following the [Task 352 design](../design/20260923-352-user32-facade-module.md), add a `user32` facade module so that on the real 4th CHD `GetModuleHandleA("user32")` receives a module handle and `GetProcAddress(user32, "GetActiveWindow")` resolves, then record the next boundary.*

## 단계 / Steps

1. `include/re2dj/hle/modules/user32_module.h`, `src/hle/modules/user32_module.cpp`에 `MakeUser32ModuleDescriptor`와 `GetActiveWindow` handler를 추가하고 CMake core 목록에 등록합니다.
2. `tests/unit/user32_module_test.cpp`를 추가하고 테스트 main에 연결합니다.
3. `NativeKernel32Diagnostic::Setup`이 `user32` facade를 등록하게 하고, `FindGuestModule`이 `kernel32`를 찾았을 때만 `kernel32_base_`를 기록하게 합니다.
4. `re2dj_linux_native_guest_module_probe`에 `user32` 등록 검사를 추가합니다.
5. Linux i386·x64, Windows x86을 빌드하고 단위 테스트를 실행합니다.
6. 실제 4th CHD로 연속 실행과 기존 네 진단을 실행합니다.
7. 분석 문서, `ARCHITECTURE.md`, `docs/TODO.md`, 작업 로그를 갱신하고 커밋합니다.

*Steps: (1) add `MakeUser32ModuleDescriptor` and the `GetActiveWindow` handler in `include/re2dj/hle/modules/user32_module.h` and `src/hle/modules/user32_module.cpp`, registered in the CMake core list; (2) add `tests/unit/user32_module_test.cpp` and hook it into the test main; (3) have `NativeKernel32Diagnostic::Setup` register the `user32` facade, and have `FindGuestModule` record `kernel32_base_` only when it finds `kernel32`; (4) add `user32` registration checks to `re2dj_linux_native_guest_module_probe`; (5) build Linux i386/x64 and Windows x86 and run the unit tests; (6) run the continuation and the four earlier diagnostics on the real 4th CHD; (7) update the analysis, `ARCHITECTURE.md`, `docs/TODO.md`, and the work log, then commit.*

## 완료 조건 / Completion criteria

* 단위 테스트와 Linux i386 probe가 통과합니다.
* 실제 4th CHD에서 `#0002`가 0이 아닌 `user32` base를 받고, `GetActiveWindow` 요청이 더 이상 unresolved lookup으로 멈추지 않습니다.
* 새 경계와 호출 기록이 분석 문서에 확인됨/추정/미확정으로 나뉘어 기록됩니다.

*Completion: unit tests and the Linux i386 probe pass; on the real 4th CHD `#0002` receives a non-zero `user32` base and the `GetActiveWindow` request no longer stops as an unresolved lookup; and the new boundary and call log are recorded in the analysis, split into confirmed, inferred, and unresolved.*
