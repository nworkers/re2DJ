# 작업 355 작업 로그 — Linux x64 facade와 in-process 진단 연결 / Task 355 work log — Linux x64 facades and in-process diagnostics

설계: [20260924-355-linux-x64-facade-diagnostics.md](../design/20260924-355-linux-x64-facade-diagnostics.md)
작업 지시서: [20260924-355-linux-x64-facade-diagnostics.md](../work-orders/20260924-355-linux-x64-facade-diagnostics.md)

## 결정 / Decision

2026-09-24 사용자 결정으로 게스트 SEH 디스패치를 이번 단계에서 분리했다. 따라서 x64의 완료 경계는 게스트 자신의 `INT3`다. instruction trace도 signal handler가 게스트로 복귀할 수 있어야 하므로 같은 4단계로 미뤘다.

*Per the user's 2026-09-24 decision, guest SEH dispatch was split out of this stage, so the x64 completion boundary is the guest's own `INT3`. The instruction trace also needs the signal handler to return into the guest, so it moves to the same stage 4.*

## 변경 / Changes

- `native_guest_module_image`, `native_guest_module_set`, `native_kernel32_diagnostic`, `native_dynamic_thunk`를 `x86/`에서 루트로 옮겼다(`git mv`). dynamic thunk와 진단 stop stub은 `MapNativeLowMemory`로 할당한다. facade image mapping은 이미 `0x6F000000`부터 고정 base를 탐색하므로 바꾸지 않았다.
  *Moved `native_guest_module_image`, `native_guest_module_set`, `native_kernel32_diagnostic`, and `native_dynamic_thunk` from `x86/` to the root (`git mv`). Dynamic thunks and the diagnostic stop stub are allocated with `MapNativeLowMemory`; facade image mapping already probes fixed bases from `0x6F000000` and is unchanged.*
- `original_runner.cpp`, `native_create_file_observation.cpp`, `native_continuation_observation.cpp`의 `#if defined(__i386__)`를 모두 제거했다. 설계 345가 미뤄 둔 분리가 이것으로 끝났다.
  *Removed every `#if defined(__i386__)` from `original_runner.cpp`, `native_create_file_observation.cpp`, and `native_continuation_observation.cpp`, completing the split design 345 deferred.*
- GetVersion 진단과 instruction-trace 복사 함수를 `x86/native_getversion_observation.cpp`로 옮겼다. `x64/native_getversion_observation.cpp`는 이유를 담은 명시적 오류를 반환한다.
  *Moved the GetVersion diagnostic and its instruction-trace copy helper to `x86/native_getversion_observation.cpp`; `x64/native_getversion_observation.cpp` returns an explicit error stating why.*
- x64 `re2dj_linux_native_in_process_probe`에 합성 facade 검사를 추가했다. `kernel32` facade를 mapping한 뒤, 32비트 게스트 코드가 `GetVersion` export thunk를 직접 호출하고 `NativeDynamicThunk`를 거쳐서도 호출한다.
  *Added a synthetic facade check to the x64 `re2dj_linux_native_in_process_probe`: after mapping the `kernel32` facade, 32-bit guest code calls the `GetVersion` export thunk directly and through a `NativeDynamicThunk`.*

## 검증 / Validation

환경: WSL2 Ubuntu 24.04, 커널 `5.15.167.4-microsoft-standard-WSL2`. 실제 CHD는 `roms/ez2dj4th/4thTrax.chd`를 읽기 전용으로 사용했다.

*Environment: WSL2 Ubuntu 24.04, kernel `5.15.167.4-microsoft-standard-WSL2`. The real CHD `roms/ez2dj4th/4thTrax.chd` was used read-only.*

| 항목 / Item | 결과 / Result |
| --- | --- |
| `linux-x64-debug` 빌드·CTest / build and CTest | 경고 없음, 3/3 통과 / no warnings, 3/3 pass |
| x64 합성 facade 검사 / x64 synthetic facade check | `kernel32=0x6f000000 GetVersion=0x6f002026 -> 0x23f00206 (facade and dynamic thunk)` |
| `linux-x86-debug` 빌드·CTest / build and CTest | 3/3 통과, `re2dj_linux_native_guest_module_probe` exit 0 / 3/3 pass, guest-module probe exit 0 |
| `scripts/test_linux_native_helper_probe.sh` | x64·x86 host 모두 `result=51`, fault, stop, rejection 통과 / both hosts pass |
| x64 Clang (임시 build dir, 확인 후 삭제 / temporary, removed) | probe 두 개와 `re2dj` 빌드·통과 / both probes and `re2dj` build and pass |

실제 4th CHD 결과는 다음과 같다.

*Real 4th CHD results:*

| 진단 / Diagnostic | x64 | x86 |
| --- | --- | --- |
| `first-import` | return `0x00ae028a`, SIGTRAP `0x00ae028b` | 같음 / same |
| `first-resolver` | return `0x00af0b99`, `GetVersion` `0x6f002026` | 같음 / same |
| `createfile-call` | return `0x00aeffbc`, identity 3개 일치 / three identities match | 같음 / same |
| `getversion-call` | 명시적 거절(instruction trace 없음) / explicitly rejected (no instruction trace) | return `0x00aefd82`, trace 43 frame |
| `continue` | API 13개, 게스트 `INT3` SIGTRAP EIP `0x00af1136`, ESI `'FG'`, EDI `'JM'`, SEH handler `0x00af159b` / 13 APIs, guest `INT3` | API 16개, SEH resume `0x00af11af`, `#0016 ExitProcess`. 작업 352·354와 같음 / 16 APIs, same as Tasks 352 and 354 |

x64 `continue`의 `#0001`–`#0013`은 x86과 API, 순서, caller 복귀 주소, 반환값이 같다. 다른 값은 host 배치에 따른 게스트 stack 주소(`CreateFileA` 경로 포인터 `0xefbfffa0`, SEH frame `0xefbffe0c`)와 TEB뿐이다. 해석은 [분석 문서](../analysis/ez2dj4th-linux-inprocess-first-import.md)의 작업 355 절에 확인됨/추정으로 나눠 기록했다.

*On x64, `continue` calls `#0001`–`#0013` match x86 in API, order, caller return address, and return value; only the host-placed guest stack addresses (`CreateFileA` path pointer `0xefbfffa0`, SEH frame `0xefbffe0c`) and the TEB differ. The interpretation is in the Task 355 section of the [analysis](../analysis/ez2dj4th-linux-inprocess-first-import.md), split into confirmed and inferred.*

Clang에 `-Werror`를 켜면 기존 `src/storage/fat32_chd.cpp:754` 경고에서 빌드가 멈추므로, Clang 확인은 경고를 오류로 다루지 않고 실행했다. 이 경고는 이번 변경과 무관하다. Windows x86 빌드는 Linux 전용 파일과 CMake 분기만 바뀌었으므로 실행하지 않았다.

*Clang with `-Werror` stops on the existing `src/storage/fat32_chd.cpp:754` warning, so the Clang check ran without warnings as errors; that warning is unrelated. The Windows x86 build was not run because only Linux-only files and CMake branches changed.*

## 다음 / Next

4단계: x64 게스트 SEH 디스패치와 instruction trace를 구현한다. signal handler에서 32비트 handler를 중첩 호출하고, 게스트로 복귀할 수 있어야 한다. host `rsp` slot stack과, sigreturn 시 게스트 FS를 복원하는 방법을 설계한다. 완료 기준은 실제 4th CHD가 x64에서 `#0016`에 도달하는 것이다.

*Stage 4: implement x64 guest SEH dispatch and the instruction trace, which need nested calls into the 32-bit handler from the signal handler and a return into the guest. Design the host `rsp` slot stack and how the guest FS is restored on sigreturn; completion is the real 4th CHD reaching `#0016` on x64.*
