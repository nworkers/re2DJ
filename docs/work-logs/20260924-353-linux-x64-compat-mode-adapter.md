# 작업 353 작업 로그 — Linux x64 compatibility-mode 전환 runtime / Task 353 work log — Linux x64 compatibility-mode transition runtime

설계: [20260924-353-linux-x64-compat-mode-adapter.md](../design/20260924-353-linux-x64-compat-mode-adapter.md)
작업 지시서: [20260924-353-linux-x64-compat-mode-adapter.md](../work-orders/20260924-353-linux-x64-compat-mode-adapter.md)

## 결정 / Decisions

2026-09-24 사용자 결정에 따라 Linux x64 adapter는 i386 helper IPC 확장이 아니라 같은 프로세스의 compatibility-mode 전환으로 간다. 이번 작업 단위는 설계 전체와 1단계(전환 runtime, 합성 probe)만 다룬다. 설계 307과 KB `x86-32-guest-on-64-bit-host.md`의 helper 전용 결론은 이 결정에 맞게 고쳤다.

*Per the user's 2026-09-24 decision, the Linux x64 adapter uses same-process compatibility-mode transitions rather than extending the i386 helper IPC. This task unit covers the whole design and stage 1 only (transition runtime and synthetic probe). The helper-only conclusions of design 307 and the KB `x86-32-guest-on-64-bit-host.md` were corrected to match.*

## 변경 / Changes

- `src/platform/linux/native_import_gate.h`, `native_guest_fault.h`: x86 bridge와 bootstrap에서 폭 중립 타입(`NativeImportGateEvent`/`Result`/`Handler`, `NativeGuestFault`)을 추출했다. x86 헤더가 이를 include하며, 동작 변경은 없다.
  *Extracted the width-neutral types (`NativeImportGateEvent`/`Result`/`Handler`, `NativeGuestFault`) from the x86 bridge and bootstrap; the x86 headers include them, with no behavior change.*
- `src/platform/linux/x64/native_compat_mode_transition.h/.cpp`: 복사용 blob(`exit32`, `gate32`, `trial32`, `exit64`, `gate64`)과 host routine(`NativeCompatEnterGuest`, `NativeCompatGuestExit`, `NativeCompatImportLanding`, `NativeCompatRestoreHostFs`, `NativeCompatSignalEntry`)을 top-level asm으로 작성했다.
  *Wrote the copied blob (`exit32`, `gate32`, `trial32`, `exit64`, `gate64`) and host routines (`NativeCompatEnterGuest`, `NativeCompatGuestExit`, `NativeCompatImportLanding`, `NativeCompatRestoreHostFs`, `NativeCompatSignalEntry`) as top-level asm.*
- `src/platform/linux/x64/native_compat_mode.h/.cpp`: `NativeCompatModeRuntime`을 추가했다. 4 GiB 미만 할당(`MapNativeLowMemory`), LDT FS, TEB/PEB, 전환 page patch와 W^X 전환, sigaltstack과 signal 설치, 시험 전환, `Run`, import dispatch, fault 기록을 담당한다.
  *Added `NativeCompatModeRuntime`: allocation below 4 GiB (`MapNativeLowMemory`), LDT FS, TEB/PEB, transition-page patching with a W^X switch, sigaltstack and signal installation, a trial transition, `Run`, import dispatch, and fault recording.*
- `src/platform/linux/x64/native_compat_mode_probe.cpp`와 CMake: Linux 64비트 분기에 runtime source와 `re2dj_linux_compat_mode_probe`를 추가하고 CTest에 등록했다.
  *Added the runtime sources and `re2dj_linux_compat_mode_probe` to the Linux 64-bit CMake branch and registered the probe with CTest.*

## 검증 / Validation

환경: WSL2 Ubuntu 24.04, 커널 `5.15.167.4-microsoft-standard-WSL2`, CPU `fsgsbase` 지원, GCC(기본 preset)와 Clang.

*Environment: WSL2 Ubuntu 24.04, kernel `5.15.167.4-microsoft-standard-WSL2`, CPU with `fsgsbase`, GCC (default presets) and Clang.*

| 항목 / Item | 결과 / Result |
| --- | --- |
| `linux-x64-debug` 전체 빌드 / full build | 경고·오류 없음 / no warnings or errors |
| `ctest --preset linux-x64-debug` | 2/2 통과(`re2dj_unit_tests`, `re2dj_linux_compat_mode_probe`) / 2/2 pass |
| `re2dj_linux_compat_mode_probe` (GCC) | `fsgsbase` 15/15, `arch_prctl` 16/16 ok, `compatibility-mode probe passed` |
| `re2dj_linux_compat_mode_probe` (Clang, 임시 build dir / temporary build dir) | `compatibility-mode probe passed`. build dir은 확인 뒤 삭제 / build dir removed afterwards |
| `linux-x86-debug` 빌드와 CTest / build and CTest | 2/2 통과(`re2dj_unit_tests`, `re2dj_linux_native_guest_module_probe`) / 2/2 pass |
| `re2dj_linux_native_in_process_probe` (x86) | exit 0, `imports=2 dynamic=2 exit=51 signal=4` |
| `linux-x86-helper` 빌드 / build | `re2dj_linux_native_ipc_helper` 링크 성공 / links |

probe 출력의 배치 예(`fsgsbase` 경로): `teb=0xefdfe000 stack=0xeff00000-0xf0000000 fs=0x0007 bridge=0xefbff007`. `arch_prctl` 경로도 같은 배치다(destructor가 LDT entry와 mapping을 해제한 뒤 다시 할당).

*Sample layout from the probe (`fsgsbase` path): `teb=0xefdfe000 stack=0xeff00000-0xf0000000 fs=0x0007 bridge=0xefbff007`. The `arch_prctl` path gets the same layout because the destructor releases the LDT entry and mappings before reallocation.*

Clang 빌드에서는 기존 `src/storage/fat32_chd.cpp:754`의 `-Wtautological-constant-out-of-range-compare` 경고가 보였다. 이번 변경과 무관하다.

*The Clang build showed an existing `-Wtautological-constant-out-of-range-compare` warning at `src/storage/fat32_chd.cpp:754`, unrelated to this change.*

Windows x86 빌드는 실행하지 않았다. 이번 변경은 Linux 전용 CMake 분기와 `src/platform/linux/` 아래 파일만 건드리며, Windows target은 이 파일들을 컴파일하지 않는다.

*The Windows x86 build was not run: this change touches only Linux-only CMake branches and files under `src/platform/linux/`, none of which Windows targets compile.*

## 확인된 사실 / Confirmed facts

WSL2 커널 5.15에서 확인한 사실은 다음과 같다.

*Confirmed on WSL2 kernel 5.15:*

- **확인됨**: 64비트 프로세스가 `lretq`로 CS `0x23`에 들어가 32비트 코드를 실행한다. `ret`가 4바이트를 꺼내 32비트 `exit32` stub으로 돌아오는 것이 그 증거다. `ljmp`/`lcall $0x33`로 64비트에 복귀하고, `lretl`로 게스트에 재진입한다.
  ***Confirmed**: a 64-bit process enters CS `0x23` with `lretq` and runs 32-bit code (evidenced by `ret` popping 4 bytes back into the 32-bit `exit32` stub), returns to 64-bit mode with `ljmp`/`lcall $0x33`, and re-enters the guest with `lretl`.*
- **확인됨**: `modify_ldt(0x11)`로 만든 LDT selector `0x0007`을 64비트 mode에서 FS에 적재하면, 게스트의 `fs:[0x18]`과 `fs:[0]`이 TEB 값을 읽는다.
  ***Confirmed**: loading the LDT selector `0x0007` created by `modify_ldt(0x11)` into FS in 64-bit mode lets the guest read the TEB through `fs:[0x18]` and `fs:[0]`.*
- **확인됨**: `wrfsbase` 경로와 `arch_prctl(ARCH_SET_FS)` 경로 모두 복귀 뒤 host FS base, `thread_local`, `errno`를 보존한다.
  ***Confirmed**: both the `wrfsbase` and `arch_prctl(ARCH_SET_FS)` paths preserve the host FS base, `thread_local`, and `errno` after return.*
- **확인됨**: compatibility mode의 `ud2`/`int3`는 64비트 rt frame으로 전달되고, `REG_CSGSFS`의 CS가 `0x23`이다. asm entry가 FS를 복원한 뒤 C handler의 `siglongjmp`로 복귀할 수 있다.
  ***Confirmed**: compatibility-mode `ud2`/`int3` are delivered with a 64-bit rt frame whose `REG_CSGSFS` CS is `0x23`, and after the asm entry restores FS the C handler can return with `siglongjmp`.*
- **미확정**: `ia32_emulation=0`이나 `modify_ldt`가 비활성인 커널에서의 동작, 순수 Linux(비WSL) 커널에서의 동작은 실행하지 않았다.
  ***Unresolved**: behavior on kernels with `ia32_emulation=0` or `modify_ldt` disabled, and on native (non-WSL) Linux kernels, was not exercised.*

## 다음 / Next

2단계: `NativePeSession`, import thunk 생성기, in-process runner가 저주소 할당과 bridge·cleanup 주소를 인자로 받게 한다. x64에서 x86과 같은 합성 PE fixture를 통과시킨다.

*Stage 2: make `NativePeSession`, the import-thunk emitter, and the in-process runner take low allocation and the bridge/cleanup addresses as arguments, and pass the same synthetic PE fixture on x64 as on x86.*
