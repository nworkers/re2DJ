# Linux 실행 기준선 / Linux Runtime Baseline

## 확인됨 / Confirmed

2026-09-18 작업 308에서 `7445a49` 이후의 compiler portability 수정 작업 트리를 WSL2 Ubuntu 24.04.1, kernel `5.15.167.4-microsoft-standard-WSL2`, GCC 13.3.0, CMake 3.28.3, Ninja 1.11.1로 검증했다. 이는 합성 실행·빌드 검증이며 원본 게임 검증이 아니다. [작업 로그](../work-logs/20260918-308-linux-wsl-baseline.md).

*Task 308 validated the compiler-portability working tree following 7445a49 under WSL2 Ubuntu 24.04.1, kernel 5.15.167.4-microsoft-standard-WSL2, GCC 13.3.0, CMake 3.28.3, and Ninja 1.11.1. This is synthetic/build evidence, not original-game validation. [Work log](../work-logs/20260918-308-linux-wsl-baseline.md).*

- x64 전체 Debug build와 i386 helper build가 warnings-as-errors로 성공했다. `file`로 제품 ELF64와 helper ELF32를 확인했다.
- Linux CTest 1/1 통과. native host probe는 load `0x11000000`, imports 2, arguments 41·42, result 51, child 0을 보고했다.
- fault fixture는 signal 4, EIP `0x1000100f`, guest ESP `0xf79a8ffc`를 보고했다. ESP 값은 이 실행의 관찰값이며 고정 계약이 아니다.
- WSLg 1.0.65에서 OpenGL blend probe의 10개 픽셀 검사가 통과했다. `glxinfo -B`는 Mesa 25.2.8, llvmpipe, accelerated=no를 보고했다.

*The full x64 Debug and i386 helper builds passed with warnings-as-errors; file identified ELF64 and ELF32 respectively. Linux CTest passed 1/1. The native probe reported load 0x11000000, two imports, arguments 41/42, result 51, and child exit zero. The fault fixture reported signal 4, EIP 0x1000100f, and ESP 0xf79a8ffc; that ESP is an observation, not a fixed contract. WSLg 1.0.65 passed all ten OpenGL pixel checks using Mesa 25.2.8 llvmpipe with acceleration disabled.*

## 추정 / Inferred

소프트웨어 렌더링이므로 현재 WSL 화면 테스트의 성능 수치를 실제 GPU 환경의 성능으로 일반화할 수 없다. 실제 게임 프레임과 오디오 지연은 별도 측정이 필요하다.

*Software rendering prevents generalizing these WSL display results to GPU performance. Actual game pacing and audio latency require separate measurement.*

## 미확정 / Unresolved

x86 제품 CLI·SDL 실행, native 32비트 Linux kernel, Clang build, WSL GPU 가속, 실제 오디오·입력, 원본 공식 target의 Linux 실행은 이번에 검증하지 않았다. 후속 검증은 [L1~L7 계획](../work-orders/20260918-307-linux-x86-x64-wsl.md)을 따른다.

*The x86 product CLI/SDL path, native 32-bit Linux kernel, Clang builds, WSL GPU acceleration, actual audio/input, and official original-game execution on Linux were not validated. Follow the [L1–L7 plan](../work-orders/20260918-307-linux-x86-x64-wsl.md).*

## L1 확인됨 / L1 Confirmed

작업 309에서 WSL Ubuntu 24.04.1의 multilib 환경으로 Linux x86 product host를 추가 검증했다. x86 product와 host probe는 ELF32 i386이며, 기존 x64 product는 ELF64 x86-64, helper는 ELF32 i386이다. x86·x64 host probe는 같은 helper를 실행해 result 51, child 0, fault signal 4를 각각 보고했고, x86 OpenGL blend probe도 10개 검사를 통과했다. x86 CTest는 1/1 통과했다.

*Task 309 additionally verified the Linux x86 product host in WSL Ubuntu 24.04.1 with multilib. The x86 product and host probe are ELF32 i386; the existing x64 product is ELF64 x86-64 and the helper is ELF32 i386. Both host probes ran the same helper and reported result 51, child 0, and fault signal 4. The x86 OpenGL blend probe passed all ten checks, and x86 CTest passed 1/1.*

현재 WSL에는 i386 `libxss`와 `libxtst` 개발 패키지가 없어 x86 preset은 SDL XScreenSaver·XTest 통합을 끈다. 이 설정은 product build와 OpenGL probe를 통과시키지만 해당 선택 기능의 런타임 동작을 확인한 것은 아니다. `libxss-dev:i386 libxtst-dev:i386` 설치 후 두 옵션을 켠 별도 검증이 남아 있다.

*The current WSL installation lacks i386 `libxss` and `libxtst` development packages, so the x86 preset disables SDL XScreenSaver and XTest integration. The product build and OpenGL probe pass with this configuration, but those optional integrations are not runtime-validated. A separate check with `libxss-dev:i386 libxtst-dev:i386` and both options enabled remains.*

## L2 확인됨 / L2 Confirmed

작업 311에서 Linux x64·x86 product host와 공용 i386 helper의 guest-memory transport를 검증했다. protocol v3의 packet layout과 version은 유지하면서 memory transfer 상한을 64KiB로 공유했고, helper는 현재 PE image와 pending import stack 안의 범위만 허용한다. synthetic host probe는 image 안에서 정확히 64KiB를 읽고 같은 데이터를 썼으며, 64KiB 초과 요청과 image 바깥 주소 요청을 거부한 뒤 import를 계속 완료했다. x64·x86 CTest는 각각 1/1 통과했고 두 host probe 모두 result 51, child 0, fault signal 4를 보고했다. 이는 원본 게임 실행이 아닌 synthetic IPC 검증이다. [설계](../design/20260918-311-linux-guest-memory-window.md), [작업 로그](../work-logs/20260918-311-linux-guest-memory-window.md).

*Task 311 verified guest-memory transport between the Linux x64/x86 product hosts and the shared i386 helper. Protocol v3 packet layout and version remain unchanged; the shared memory-transfer limit is 64 KiB, and the helper allows only the current PE image and pending import stack ranges. The synthetic host probe read and wrote exactly 64 KiB inside the image, rejected an oversized request and an address outside the image, and then completed the import sequence. x64 and x86 CTest each passed 1/1, and both host probes reported result 51, child 0, and fault signal 4. This is synthetic IPC evidence, not original-game execution. [Design](../design/20260918-311-linux-guest-memory-window.md), [work log](../work-logs/20260918-311-linux-guest-memory-window.md).*

## L2 dispatcher 확인됨 / L2 Dispatcher Confirmed

작업 312에서 공용 `ImportDispatcher`가 loader-confirmed named/ordinal import metadata와 `ExecutionBackend`만으로 x86 `ESP + 4` argument를 읽어 handler 결과를 completion으로 보내는 것을 검증했다. module 이름은 ASCII 대소문자를 구분하지 않고, name import는 정확히 비교한다. 합성 probe의 두 import는 dispatcher를 통해 계속 실행되어 result 51, child 0으로 끝났다. unit test는 named·ordinal lookup, `__stdcall`·`__cdecl` cleanup, duplicate/unknown binding, stack address overflow, backend read failure와 handler failure를 확인했다. x64·x86 CTest는 각각 1/1 통과했다. 실제 Win32 API binding이나 원본 게임 실행 증거는 아니다. [설계](../design/20260918-312-linux-import-dispatcher.md), [작업 로그](../work-logs/20260918-312-linux-import-dispatcher.md).

*Task 312 verified that the shared `ImportDispatcher` reads x86 `ESP + 4` arguments through only loader-confirmed named/ordinal import metadata and `ExecutionBackend`, then sends handler results as completion. Module names compare ASCII case-insensitively and name imports compare exactly. The synthetic probe continued both imports through the dispatcher and ended with result 51 and child exit zero. Unit tests cover named/ordinal lookup, `__stdcall`/`__cdecl` cleanup, duplicate/unknown bindings, stack-address overflow, backend-read failure, and handler failure. x64 and x86 CTest each passed 1/1. This is not evidence of actual Win32 API bindings or original-game execution. [Design](../design/20260918-312-linux-import-dispatcher.md), [work log](../work-logs/20260918-312-linux-import-dispatcher.md).*

## ez2dj4th first boundary 확인됨 / ez2dj4th First Boundary Confirmed

**확인됨, 2026-09-18 작업 321.** WSL Ubuntu 24.04의 Linux x64 CLI가 로컬 `roms/ez2dj4th/4thTrax.chd`에서 FAT32 `EZ2DJ/EZ2DJ.EXE`를 read-only temporary staging으로 materialize하고 같은 i386 helper를 실행했습니다. profile metadata는 image base `0x00400000`, entry `0x00ae0240`을 보고했고, 원본 코드는 첫 event로 `kernel32.dll!GetModuleHandleA` import gate를 보고했습니다.

***Confirmed, 2026-09-18 Task 321.** The Linux x64 CLI on WSL Ubuntu 24.04 materialized FAT32 `EZ2DJ/EZ2DJ.EXE` from local `roms/ez2dj4th/4thTrax.chd` into read-only temporary staging and ran the same i386 helper. Profile metadata reported image base `0x00400000` and entry `0x00ae0240`; original code reported `kernel32.dll!GetModuleHandleA` as its first import-gate event.*

이는 CHD lookup, staging, PE mapping, native helper entry와 첫 import metadata의 연결만 확인합니다. `GetModuleHandleA`의 반환값·호출 규약, protection continuation, 동적 import, device/Hardlock 및 게임 실행은 아직 미확정입니다. 미등록 import는 CLI 정책에 따라 completion하지 않고 helper를 종료했습니다.

*This confirms only the connection from CHD lookup through staging, PE mapping, native-helper entry, and first import metadata. `GetModuleHandleA` return value, calling convention, protection continuation, dynamic imports, device/Hardlock behavior, and game execution remain unresolved. The CLI completed no unregistered import and stopped the helper by policy.*

## ez2dj4th first import stack 확인됨 / ez2dj4th First Import Stack Confirmed

**확인됨, 2026-09-19 작업 322.** 같은 read-only staged CHD 경로에서 Linux x64와 Linux x86 CLI는 모두 첫 `kernel32.dll!GetModuleHandleA` gate의 guest stack words를 `return slot 0x00ae028a`, `arg0 0x00ae0f2c`로 보고했습니다. 관찰은 import completion 전에 helper가 허용한 pending-stack 범위에서 정확히 8바이트를 읽어 수행했습니다.

***Confirmed, 2026-09-19 Task 322.** On the same read-only staged-CHD path, both Linux x64 and Linux x86 CLIs reported the guest stack words at the first `kernel32.dll!GetModuleHandleA` gate as `return slot 0x00ae028a` and `arg0 0x00ae0f2c`. The observation read exactly eight bytes from the helper-permitted pending-stack range before import completion.*

`arg0`가 0이 아니라는 사실만 확인되었습니다. 이 작업은 그 주소를 문자열로 역참조하지 않았고, module 이름, API 반환값, `__stdcall` stack cleanup 또는 이후 원본 코드 진행을 확정하지 않았습니다. 다음 binding 설계는 제한된 guest-string 읽기와 process/module registry 계약을 먼저 정의해야 합니다.

*Only the nonzero value of `arg0` is confirmed. This task did not dereference that address as a string or establish a module name, API return value, `__stdcall` stack cleanup, or further original-code progress. The next binding design must first define bounded guest-string reading and the process/module-registry contract.*

## ez2dj4th first import argument text 확인됨 / ez2dj4th First Import Argument Text Confirmed

**확인됨, 2026-09-19 작업 324.** Linux x64와 Linux x86 CLI는 첫 `kernel32.dll!GetModuleHandleA` gate의 nonzero `arg0 0x00ae0f2c`에서 최대 4096바이트를 읽어 첫 NUL까지 `kernel32`을 동일하게 보고했습니다. read는 guest `ExecutionBackend` 경계에서 수행됐고 helper는 completion 없이 중지됐습니다.

***Confirmed, 2026-09-19 Task 324.** Linux x64 and Linux x86 CLIs both read at most 4096 bytes from nonzero `arg0 0x00ae0f2c` at the first `kernel32.dll!GetModuleHandleA` gate and reported `kernel32` through the first NUL. The read occurred at the guest `ExecutionBackend` boundary and the helper stopped without completion.*

이 관찰은 raw NUL-terminated byte sequence만 확인합니다. 그 텍스트를 API module-name argument로 해석하는 정책, module registry, 반환 handle, stack cleanup과 original-code continuation은 아직 미확정입니다.

*This observation confirms only the raw NUL-terminated byte sequence. Policy that interprets the text as an API module-name argument, module registry, return handle, stack cleanup, and original-code continuation remain unresolved.*

## L2 protected-page probe repair 확인됨 / L2 Protected-page Probe Repair Confirmed

**확인됨, 2026-09-19 작업 323.** protected-page fixture는 first page에 허용된 `{5}` 쓰기 뒤 다음 import에서 `{5, 8, 7, 6}`을 기대하도록 고쳐졌습니다. 초기값 `{9, 8, 7, 6}`, second-page write 거부, protection restore, full-range protection 및 free 검사는 그대로입니다. `bash scripts/test_linux_native_helper_probe.sh`는 x64와 x86 host 모두에서 다시 통과했습니다.

***Confirmed, 2026-09-19 Task 323.** The protected-page fixture now expects `{5, 8, 7, 6}` at the next import after its allowed `{5}` first-page write. Initial `{9, 8, 7, 6}`, rejected second-page write, protection restore, full-range protection, and free checks remain. `bash scripts/test_linux_native_helper_probe.sh` passes again for both x64 and x86 hosts.*

## L2 import stop 제어 확인됨 / L2 Import Stop Control Confirmed

작업 313에서 Linux `NativeHelperBackend`의 pending `ImportCompletionAction::kStop`을 helper continuation packet이 아닌 terminal host 제어로 고정했다. backend는 pipe를 닫고 i386 helper를 종료한 뒤 `Stopped` 상태가 되며, 후속 `WaitForEvent()`를 거부한다. 합성 stop probe는 첫 import event에서 `kStop`을 요청해 이 terminal 경계를 x64·x86 host 각각에서 확인했다. 이는 guest `ExitProcess` 구현이나 원본 게임 실행 증거가 아니다. [설계](../design/20260918-313-linux-import-stop-control.md), [작업 로그](../work-logs/20260918-313-linux-import-stop-control.md).

*Task 313 fixes pending `ImportCompletionAction::kStop` in the Linux `NativeHelperBackend` as terminal host control rather than a helper continuation packet. The backend closes pipes, terminates the i386 helper, enters `Stopped`, and rejects a later `WaitForEvent()`. The synthetic stop probe requested `kStop` at the first import event and confirmed that terminal boundary on each x64 and x86 host. This is not an implementation of guest `ExitProcess` or evidence of original-game execution. [Design](../design/20260918-313-linux-import-stop-control.md), [work log](../work-logs/20260918-313-linux-import-stop-control.md).*
