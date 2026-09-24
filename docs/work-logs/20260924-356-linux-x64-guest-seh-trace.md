# 작업 356 작업 로그 — Linux x64 게스트 SEH 디스패치와 instruction trace / Task 356 work log — Linux x64 guest SEH dispatch and instruction trace

설계: [20260924-356-linux-x64-guest-seh-trace.md](../design/20260924-356-linux-x64-guest-seh-trace.md)
작업 지시서: [20260924-356-linux-x64-guest-seh-trace.md](../work-orders/20260924-356-linux-x64-guest-seh-trace.md)

## 변경 / Changes

- **공용 register 형태.** `native_guest_fault.h`에 `NativeTrapRegisters`를 추가했다. 두 폭의 signal handler가 `ucontext`와 이 형태 사이를 변환한다.
  ***Shared register form.** Added `NativeTrapRegisters` to `native_guest_fault.h`; each width's signal handler converts between `ucontext` and this form.*
- **SEH 판단 공용화.** `x86/guest_seh_types.h`를 루트 `native_guest_seh.h`로 옮겼다. x64에서 경고가 나는 cdecl 함수 타입은 x86 bootstrap에 남겼다. `PrepareNativeGuestBreakpointDispatch`(INT3 판정, TEB의 SEH frame 검증, record·CONTEXT 구성)와 `ApplyNativeGuestSehContext`를 추가했다.
  ***Shared SEH decisions.** Moved `x86/guest_seh_types.h` to the root `native_guest_seh.h`, keeping the cdecl function type (which warns on x64) in the x86 bootstrap, and added `PrepareNativeGuestBreakpointDispatch` (INT3 recognition, TEB SEH-frame validation, record and CONTEXT construction) and `ApplyNativeGuestSehContext`.*
- **trace 공용화.** x86 bootstrap의 trace 상태기계와 `Arm/Resume/Stop/FinalizeNativeInstructionTrace`를 루트 `native_instruction_trace.cpp`로 옮겼다. signal handler에서는 `HandleNativeInstructionTraceTrap`만 부른다. 상태 변수는 `sig_atomic_t`에서 `volatile std::uint32_t`로 바꿨다. `0x80000000` 이상 주소를 부호 있는 비교로 다루지 않게 하려는 것이다.
  ***Shared trace.** Moved the x86 bootstrap's trace state machine and `Arm/Resume/Stop/FinalizeNativeInstructionTrace` to the root `native_instruction_trace.cpp`; signal handlers call only `HandleNativeInstructionTraceTrap`. The state moved from `sig_atomic_t` to `volatile std::uint32_t` so addresses at or above `0x80000000` are never compared as signed.*
- **x86 bootstrap.** 공용 trace·SEH 코드를 쓰도록 바꿨다. handler를 직접 호출하는 방식과 FS 전환은 그대로다.
  ***x86 bootstrap.** Moved onto the shared trace and SEH code; the direct handler call and FS switching are unchanged.*
- **x64 게스트 복귀.** asm signal entry가 C handler를 호출하고, 반환값이 1이면 게스트 FS selector를 다시 적재한 뒤 `__restore_rt`로 돌아간다. C handler는 trace trap과 SEH를 처리한다. SEH handler는 record·CONTEXT를 중단된 ESP 아래 게스트 stack에 두고 `NativeCompatEnterGuest`로 중첩 호출하며, 호출 전후로 host `rsp` 슬롯을 저장·복원한다. import landing은 x86 bridge처럼 trace를 재개한다.
  ***x64 guest return.** The asm signal entry calls the C handler and, on a result of 1, reloads the guest FS selector before returning to `__restore_rt`. The C handler handles trace traps and SEH: the record and CONTEXT go on the guest stack below the interrupted ESP, the handler is called through a nested `NativeCompatEnterGuest`, and the host `rsp` slot is saved and restored around it. The import landing resumes the trace like the x86 bridge.*
- **GetVersion 진단.** 루트 `native_getversion_observation.cpp`로 되돌리고, 작업 355의 x64 미지원 stub을 지웠다.
  ***GetVersion diagnostic.** Moved back to the root `native_getversion_observation.cpp`, deleting Task 355's x64 unsupported stub.*
- **probe.** compat-mode probe에 SEH 검사 7번을 추가했다. 게스트가 frame을 등록하고 `INT3`를 실행하면, handler가 CONTEXT의 EIP·EAX를 고치고 게스트가 frame을 해제한다. x64 in-process probe에는 x86과 같은 trace 검사를 추가했다.
  ***Probes.** Added SEH check 7 to the compat-mode probe (the guest registers a frame and executes `INT3`, the handler edits CONTEXT EIP and EAX, and the guest unregisters the frame), and the x86 trace check to the x64 in-process probe.*

## 검증 / Validation

환경: WSL2 Ubuntu 24.04, 커널 `5.15.167.4-microsoft-standard-WSL2`. 실제 CHD는 `roms/ez2dj4th/4thTrax.chd`를 읽기 전용으로 사용했다.

*Environment: WSL2 Ubuntu 24.04, kernel `5.15.167.4-microsoft-standard-WSL2`. The real CHD `roms/ez2dj4th/4thTrax.chd` was used read-only.*

| 항목 / Item | 결과 / Result |
| --- | --- |
| x64 compat-mode probe | `fsgsbase`·`arch_prctl` 두 경로 모두 검사 1~7 통과(SEH resume `code+kSehGuest+25`, TEB chain `0xFFFFFFFF` 복원) / checks 1–7 pass on both paths |
| x64 in-process probe | `trace frames=1 first=0x11001008 signal=SIGILL`, facade 검사, exit 51, 재실행 통과 / trace, facade, exit 51, and re-run pass |
| x64·x86 CTest | 각각 3/3 통과 / 3/3 each |
| x86 in-process·guest-module probe | `imports=2 dynamic=2 exit=51 signal=4`, exit 0 |
| `scripts/test_linux_native_helper_probe.sh` | exit 0. 두 host 모두 `result=51`, fault, stop, rejection 통과 / exit 0; both hosts pass |
| x64 Clang (임시 build dir, 확인 후 삭제 / temporary, removed) | 두 probe 통과. 실제 CHD continuation도 `#0016`까지 같음 / both probes pass; the real-CHD continuation also reaches `#0016` |

실제 4th CHD의 다섯 진단은 x64와 x86이 같다.

*The five real-4th-CHD diagnostics match between x64 and x86:*

| 진단 / Diagnostic | x64 = x86 |
| --- | --- |
| `continue` | SEH count 1, handler `0x00af159b`, resume `0x00af11af`. API 16개, `#0016 GetProcAddress(0x6f000000, "ExitProcess")`에서 정지 / 16 APIs, stop at `#0016` |
| `getversion-call` | return `0x00aefd82`, trace 43 frame, breakpoint `0x00af0b99` / 43 trace frames |
| `first-import` | return `0x00ae028a` |
| `first-resolver` | return `0x00af0b99`, `GetVersion` `0x6f002026` |
| `createfile-call` | return `0x00aeffbc`, identity 세 값 일치 / three identities match |

trace 43 frame을 폭별로 비교했다. ESP·EBP·EBX를 가리면 frame #019의 EFLAGS(x64 `0x302`, x86 `0x382`)만 다르다. EBX는 두 host 모두 게스트 entry에서 정하지 않아 host 잔여값(x64 `0x00ae0240`, x86 `0xfffd86d0`)이 들어간다. 해석은 [분석 문서](../analysis/ez2dj4th-linux-inprocess-first-import.md)의 작업 356 절에 두었다.

*Comparing the 43 trace frames across widths with ESP, EBP, and EBX masked leaves only the EFLAGS at frame #019 (x64 `0x302`, x86 `0x382`). Neither host defines EBX at guest entry, so a host leftover flows in (x64 `0x00ae0240`, x86 `0xfffd86d0`). The interpretation is in the Task 356 section of the [analysis](../analysis/ez2dj4th-linux-inprocess-first-import.md).*

Clang에 `-Werror`를 켜면 기존 `src/storage/fat32_chd.cpp:754` 경고에서 멈추므로, Clang 확인은 경고를 오류로 다루지 않고 실행했다. 이 경고는 이번 변경과 무관하다. Windows x86 빌드는 Linux 전용 파일과 CMake 분기만 바뀌었으므로 실행하지 않았다.

*Clang with `-Werror` stops on the existing `src/storage/fat32_chd.cpp:754` warning, so the Clang check ran without warnings as errors; that warning is unrelated. The Windows x86 build was not run because only Linux-only files and CMake branches changed.*

## 결과 / Outcome

[작업 353 설계](../design/20260924-353-linux-x64-compat-mode-adapter.md)의 네 단계가 모두 끝났다. Linux x64 host는 별도 helper 없이 같은 프로세스 안에서 원본 PE32를 실행한다. 실제 4th CHD에서 도달하는 경계와 진단 결과도 Linux x86과 같다.

*All four stages of the [Task 353 design](../design/20260924-353-linux-x64-compat-mode-adapter.md) are complete: the Linux x64 host runs the original PE32 in the same process without a separate helper and reaches the same real-4th-CHD boundaries and diagnostic results as Linux x86.*

## 다음 / Next

- 게스트 entry 레지스터를 정의한다(예: Windows NT 관례의 EBX=PEB). 두 폭이 host 잔여값에 의존하지 않게 하려는 것이다. 원본의 의존 여부는 미확정이다.
  *Define guest entry registers (for example EBX = PEB per the Windows NT convention) so neither width depends on host leftovers; whether the original depends on them is unresolved.*
- 다음 게임 경계는 두 폭 공통이다. `kernel32` facade의 `ExitProcess`를 추가하는 것으로, TODO의 작업 340 다음 항목이다.
  *The next game boundary is common to both widths: add `ExitProcess` to the `kernel32` facade, the next item under Task 340 in the TODO.*
