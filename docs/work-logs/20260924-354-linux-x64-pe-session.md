# 작업 354 작업 로그 — Linux x64 PE session과 in-process runner / Task 354 work log — Linux x64 PE session and in-process runner

설계: [20260924-354-linux-x64-pe-session.md](../design/20260924-354-linux-x64-pe-session.md)
작업 지시서: [20260924-354-linux-x64-pe-session.md](../work-orders/20260924-354-linux-x64-pe-session.md)

## 변경 / Changes

- **루트로 이동(`git mv`).** `native_pe_image`, `native_import_thunks`, `native_pe_session`, `native_fault_observation`, `native_in_process_runner`의 source·header, 그리고 `native_process_bootstrap.h`, `native_import_bridge.h`를 루트로 옮겼다. x86 파일과 루트 진단 파일의 include 경로를 함께 고쳤다.
  ***Moved to the root (`git mv`).** The sources and headers of `native_pe_image`, `native_import_thunks`, `native_pe_session`, `native_fault_observation`, and `native_in_process_runner`, plus `native_process_bootstrap.h` and `native_import_bridge.h`; include paths in the x86 files and root diagnostics were updated with them.*
- **instruction trace 분리.** `NativeInstructionTrace`와 `Arm/Resume/Stop/FinalizeNativeInstructionTrace` 선언을 `x86/native_instruction_trace.h`로 옮겼다. 루트 `native_process_bootstrap.h`에는 두 폭 공통 class만 남는다.
  ***Instruction trace split.** `NativeInstructionTrace` and the `Arm/Resume/Stop/FinalizeNativeInstructionTrace` declarations moved to `x86/native_instruction_trace.h`, leaving only the class shared by both widths in the root `native_process_bootstrap.h`.*
- **저주소 할당.** 루트 `native_low_memory.h`를 추가했다. 구현은 `x86/native_low_memory.cpp`(일반 `mmap`)와 `x64/native_low_memory.cpp`(작업 353의 4 GiB 미만 탐색을 이동)로 나뉜다. import thunk 영역이 이것을 쓰고, 32비트를 넘는 bridge·cleanup 주소는 거절한다.
  ***Low allocation.** Added the root `native_low_memory.h`, implemented by `x86/native_low_memory.cpp` (plain `mmap`) and `x64/native_low_memory.cpp` (Task 353's below-4-GiB search, moved). The import-thunk region uses it and rejects bridge/cleanup addresses wider than 32 bits.*
- **x64 전역 전환 page.** `NativeCompatModeRuntime`의 전환 code·data page를 처음 쓸 때 만드는 프로세스 전역으로 바꿨다. `Run`마다 게스트 FS selector와 FS 복원 방식을 state에 쓴다. 호출에 handler가 없으면 `ConfigureNativeImportGateHandler`로 설정한 handler를 쓴다. 시험 전환은 설정된 handler를 거치지 않도록 전용 거절 handler를 쓴다.
  ***x64 global transition page.** The transition code and data pages of `NativeCompatModeRuntime` became process-global and are created on first use. Each `Run` writes the guest FS selector and FS restore method into the state; a call without a handler uses the one set by `ConfigureNativeImportGateHandler`. The trial transition names its own rejecting handler so it never consults the configured one.*
- **x64 bridge·bootstrap.** `x64/native_import_bridge.cpp`는 루트 bridge API를 구현한다. `NativeImportGateBridgeAddress()`는 전역 `gate32`를 돌려준다. `x64/native_process_bootstrap.cpp`는 `NativeCompatModeRuntime`을 감싸고, TLS callback을 `(image_base, 1, 0)` 인자로 실행한다.
  ***x64 bridge and bootstrap.** `x64/native_import_bridge.cpp` implements the root bridge API, with `NativeImportGateBridgeAddress()` returning the global `gate32`. `x64/native_process_bootstrap.cpp` wraps `NativeCompatModeRuntime` and runs TLS callbacks with `(image_base, 1, 0)`.*
- **x64 probe.** `x64/native_in_process_probe.cpp`를 x86 probe와 같은 target 이름(`re2dj_linux_native_in_process_probe`)으로 추가했다. in-process probe는 이제 두 폭 모두 CTest에 등록된다.
  ***x64 probe.** Added `x64/native_in_process_probe.cpp` under the same target name as the x86 probe (`re2dj_linux_native_in_process_probe`); the in-process probe is now registered with CTest on both widths.*

## 검증 / Validation

환경: WSL2 Ubuntu 24.04, 커널 `5.15.167.4-microsoft-standard-WSL2`.

*Environment: WSL2 Ubuntu 24.04, kernel `5.15.167.4-microsoft-standard-WSL2`.*

| 항목 / Item | 결과 / Result |
| --- | --- |
| `linux-x64-debug` 빌드와 CTest / build and CTest | 경고 없음, 3/3 통과(`re2dj_unit_tests`, `re2dj_linux_native_in_process_probe`, `re2dj_linux_compat_mode_probe`) / no warnings, 3/3 pass |
| x64 `re2dj_linux_native_in_process_probe` | `imports=2 exit=51 fault=SIGILL@0x11001008 fs=0xefafe000 rerun=ok` |
| x64 Clang (임시 build dir, 확인 후 삭제 / temporary build dir, removed) | 두 probe 통과 / both probes pass |
| `linux-x86-debug` 빌드와 CTest / build and CTest | 3/3 통과(`re2dj_linux_native_in_process_probe`가 x86에서도 CTest에 새로 등록됨) / 3/3 pass, with the x86 in-process probe newly registered |
| x86 `re2dj_linux_native_in_process_probe` | `imports=2 dynamic=2 exit=51 signal=4`, 변경 전과 같음 / unchanged |
| `scripts/test_linux_native_helper_probe.sh` | x64·x86 host 모두 IPC `result=51`, fault `signal=4`, stop, capability rejection 통과. helper는 이동한 thunk·session·저주소 할당 코드로 빌드됨 / both hosts pass IPC `result=51`, fault `signal=4`, stop, and capability rejection, with the helper built from the moved thunk, session, and low-allocation code |
| 실제 4th CHD, x86 `--linux-in-process-continue` / real 4th CHD on x86 | API 16개, SEH handler `0x00af159b` resume `0x00af11af`, `#0016 GetProcAddress(0x6f000000, "ExitProcess")`에서 정지, `kernel32`·`CreateFileA`·`GetVersion` identity 모두 일치. 작업 352와 같다 / 16 APIs, SEH handler `0x00af159b` resuming at `0x00af11af`, stop at `#0016 GetProcAddress(0x6f000000, "ExitProcess")`, all three identities match; identical to Task 352 |
| 실제 4th CHD, x64 `--linux-in-process-continue` / real 4th CHD on x64 | 3단계 전이므로 `requires an i386 host`로 명시적 거절(변경 없음) / explicitly rejected with `requires an i386 host` until stage 3 (unchanged) |

실제 CHD는 `roms/ez2dj4th/4thTrax.chd`를 읽기 전용으로 사용했다. Clang에 `-Werror`를 켜면 기존 `src/storage/fat32_chd.cpp:754` 경고에서 빌드가 멈춘다. 그래서 Clang 확인은 경고를 오류로 다루지 않고 실행했다. 이 경고는 이번 변경과 무관하다. Windows x86 빌드는 Linux 전용 파일과 CMake 분기만 바뀌었으므로 실행하지 않았다.

*The real CHD `roms/ez2dj4th/4thTrax.chd` was used read-only. Clang with `-Werror` stops on the existing `src/storage/fat32_chd.cpp:754` warning, so the Clang check ran without warnings as errors; that warning is unrelated to this change. The Windows x86 build was not run because only Linux-only files and CMake branches changed.*

## 확인된 사실 / Confirmed facts

- **확인됨**: 같은 `NativePeSession`·`NativeInProcessRunner` 코드가 x64에서 PE32를 `0x11000000`으로 재배치해 실행한다. TLS callback의 `fs:[0x18]` self와 TEB stack 범위 검사도 통과한다. 이름·ordinal import 두 번이 전역 `gate32`를 거쳐 handler에 도달한다.
  ***Confirmed**: the same `NativePeSession` and `NativeInProcessRunner` code relocates a PE32 to `0x11000000` and runs it on x64, the TLS callback's `fs:[0x18]` self and TEB stack-range checks pass, and two imports (by name and by ordinal) reach the handler through the global `gate32`.*
- **확인됨**: x64에서 fault observation이 guest TEB(`fs_base`)와 빈 SEH chain(`0xFFFFFFFF`)을 기록한다. 같은 프로세스에서 bootstrap을 다시 만든 뒤의 재실행도 성공한다.
  ***Confirmed**: on x64, fault observation records the guest TEB (`fs_base`) and an empty SEH chain (`0xFFFFFFFF`), and a re-run with a recreated bootstrap in the same process succeeds.*

## 다음 / Next

3단계: facade module image·set과 dynamic thunk를 x64에 연결하고, `original_runner.cpp`와 두 observation 파일의 `#if` 분기를 정리한다. 목표는 x64에서 실제 4th CHD가 x86과 같은 `#0016`까지 도달하는 것이다. x86의 `#0016`에는 게스트 SEH 디스패치가 필요하므로, 4단계의 SEH 디스패치와 순서를 어떻게 잡을지는 3단계 설계에서 정한다.

*Stage 3: connect facade module images and sets and dynamic thunks on x64, and resolve the `#if` branches in `original_runner.cpp` and the two observation files, so that the real 4th CHD reaches the same `#0016` on x64 as on x86. Because reaching `#0016` on x86 requires guest SEH dispatch, the stage 3 design decides how to order it relative to stage 4's SEH dispatch.*
