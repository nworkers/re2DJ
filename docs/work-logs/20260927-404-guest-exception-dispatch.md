# 작업 404 작업 로그 — 게스트 예외 디스패치와 Linux x86 호스트 GS 복원 / Task 404 work log — Guest exception dispatch and Linux x86 host GS restoration

설계: [20260927-404-guest-exception-dispatch.md](../design/20260927-404-guest-exception-dispatch.md) · 지시서: [20260927-404-guest-exception-dispatch.md](../work-orders/20260927-404-guest-exception-dispatch.md)

## 2026-09-27

- 배경 및 문제:
  - `ez2dj1st` 실행 시 게스트 예외를 게스트 SEH 체인으로 넘기는 전달 루틴과 `kernel32!RtlUnwind`를 추가했다.
  - Linux x64에서는 첫 예외를 정상적으로 통과하고 계속 실행되었으나, Linux x86(i386)에서는 실행 직후 프로세스가 segmentation fault(core dump)로 비정상 종료되는 문제가 발생했다.
  - dmesg 및 역어셈블리 분석 결과, `ez2dj1st`의 보호 코드가 수시로 `%gs` 레지스터를 0으로 변경하는 동작을 확인했다.
  - Linux x86 커널은 시그널 전달 시 유저의 GS 레지스터를 그대로 보존하므로, 시그널 핸들러 진입 시 `%gs`가 0인 상태였다.
  - 32비트 x86 GCC는 스택 카나리 확인을 위해 함수 프롤로그에서 `mov %gs:0x14, %eax`를 실행하므로, `GuestSignalHandler` 진입 직후 즉시 세그폴트가 발생하며 coredump로 이어졌다.

  *Context and Problem:*
  - *Added a general delivery path passing guest exceptions to the guest SEH chain and `kernel32!RtlUnwind` for `ez2dj1st` execution.*
  - *While Linux x64 survived the first exception and continued execution, Linux x86 (i386) immediately terminated with a segmentation fault (core dump).*
  - *Analysis of dmesg and disassembly showed that `ez2dj1st`'s protection stub frequently overwrites `%gs` with 0.*
  - *The Linux x86 kernel preserves the user's GS register upon signal delivery, leaving `%gs == 0` upon entry to the signal handler.*
  - *Because 32-bit x86 GCC emits `mov %gs:0x14, %eax` in function prologues for stack canary checking, entering `GuestSignalHandler` immediately triggered a nested segfault and coredump.*

- 해결 내용:
  1. 호스트 GS 셀렉터 보존: `g_native_host_gs_selector`를 전역 데이터로 정의하고 정적 초기화 시점에 현재 유효한 GS 셀렉터를 저장.
  2. 시그널 핸들러 트램펄린: `GuestSignalTrampoline` naked 어셈블리 래퍼를 도입하여, C++ 함수 진입 전 PC 상대 주소로 `g_native_host_gs_selector`를 읽어 `%gs`를 복원한 뒤 `GuestSignalHandler`로 점프하도록 구현.
  3. Import Bridge 트램펄린: `NativeImportGateBridge`를 naked 트램펄린으로 변경하여 게스트 GS를 스택에 보존하고 호스트 GS를 로드한 뒤 C++ `NativeImportGateBridgeImpl`을 호출하며, 복귀 전 게스트 GS를 복원하도록 수정.
  4. 게스트 호출자 복원: `CallGuestEntry`, `CallGuestTls`, `CallGuestStdcallWords`에서 게스트 함수 반환 직후 호스트 GS를 복원하여 호스트 C++ 코드로 안전하게 복귀하도록 보장.

  *Resolution:*
  - *Host GS preservation: Defined `g_native_host_gs_selector` in global data and statically initialized it with the active GS selector.*
  - *Signal handler trampoline: Implemented `GuestSignalTrampoline` naked assembly wrapper to load `g_native_host_gs_selector` via PC-relative addressing into `%gs` before tail-calling `GuestSignalHandler`.*
  - *Import bridge trampoline: Changed `NativeImportGateBridge` to a naked trampoline that saves guest GS onto the stack, restores host GS, calls `NativeImportGateBridgeImpl`, and restores guest GS before returning.*
  - *Guest caller restoration: Ensured `CallGuestEntry`, `CallGuestTls`, and `CallGuestStdcallWords` restore host GS immediately upon guest function return before executing any host C++ code.*

- 결과 및 검증:
  - Windows x86 CTest 6/6 전체 통과 (단위 테스트 4460 checks).
  - Linux x64 CTest 4/4 전체 통과 (단위 테스트 4460 checks).
  - Linux x86 CTest 4/4 전체 통과 (단위 테스트 4460 checks, `re2dj_linux_native_guest_module_probe` 및 `re2dj_linux_native_in_process_probe` 포함).
  - Linux x86 `ez2dj1st` 실제 실행:
    - `./build/linux-x86-debug/bin/re2dj --run ez2dj1st --call-limit 50` 실행 결과 coredump 크래시 없이 정상 완료 (exit code 0).
    - `seh dispatched : count=1 last_handler=0x01da3404 resumed_eip=0x01da30d3`, 50개 API 호출 정상 통과 확인.
  - Linux x86 `ez2dj4th` 실제 실행:
    - `./build/linux-x86-debug/bin/re2dj --run ez2dj4th --call-limit 50` 정상 완료 확인.

  *Results and Verification:*
  - *Windows x86: All 6 CTest tests pass (4460 unit checks).*
  - *Linux x64: All 4 CTest tests pass (4460 unit checks).*
  - *Linux x86: All 4 CTest tests pass (4460 unit checks, including `re2dj_linux_native_guest_module_probe` and `re2dj_linux_native_in_process_probe`).*
  - *Linux x86 `ez2dj1st` real execution:*
    - *`./build/linux-x86-debug/bin/re2dj --run ez2dj1st --call-limit 50` completed cleanly with exit code 0 and no coredump.*
    - *Confirmed `seh dispatched : count=1 last_handler=0x01da3404 resumed_eip=0x01da30d3` and 50 API calls processed.*
  - *Linux x86 `ez2dj4th` real execution:*
    - *`./build/linux-x86-debug/bin/re2dj --run ez2dj4th --call-limit 50` completed cleanly.*
