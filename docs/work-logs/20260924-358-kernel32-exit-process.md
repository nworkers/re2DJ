# 작업 358 작업 로그 — `kernel32!ExitProcess`와 게스트 종료 경계 / Task 358 work log — `kernel32!ExitProcess` and the guest exit boundary

설계: [20260924-358-kernel32-exit-process.md](../design/20260924-358-kernel32-exit-process.md)
작업 지시서: [20260924-358-kernel32-exit-process.md](../work-orders/20260924-358-kernel32-exit-process.md)

## 변경 / Changes

- **공용 계약.** `ImportReturn`에 `exit_process`/`exit_code`를 추가했다. `ImportDispatcher`는 이를 `ImportCompletionAction::kStop`으로 바꾼다.
  ***Shared contract.** Added `exit_process`/`exit_code` to `ImportReturn`; `ImportDispatcher` maps it to `ImportCompletionAction::kStop`.*
- **`kernel32!ExitProcess`.** stdcall, 인자 1개. 인자를 `exit_code`로 넘긴다. 단위 테스트는 descriptor 5개 export와 `ExitProcess` 반환값, 잘못된 인자 형태의 거절을 검사한다.
  ***`kernel32!ExitProcess`.** stdcall with one argument, passed as `exit_code`; unit tests cover the five-export descriptor, the `ExitProcess` result, and rejection of a wrong argument shape.*
- **Linux in-process.** `NativeImportGateResult`에 같은 필드를 추가하고 `NativeGuestModuleSet`이 전달한다. `ExitNativeGuestProcess`, `GuestProcessExited`, `GuestExitCode`를 두 폭에 구현했다. x86은 bridge에서, x64는 import dispatch에서 진행 중인 `Run`으로 `siglongjmp`한다. bootstrap은 이를 정상 완료로 보고한다. `NativePeSession`은 종료한 TLS callback 뒤를 실행하지 않는다. `NativeInProcessRunner`는 `process_exited`와 종료 코드를 보고한다.
  ***Linux in-process.** Added the same fields to `NativeImportGateResult`, passed through by `NativeGuestModuleSet`, and implemented `ExitNativeGuestProcess`, `GuestProcessExited`, and `GuestExitCode` on both widths: x86 jumps from the bridge and x64 from the import dispatch back to the current `Run` with `siglongjmp`, and the bootstrap reports a normal completion. `NativePeSession` runs nothing after a TLS callback that exited, and `NativeInProcessRunner` reports `process_exited` with the exit code.*
- **출력.** continuation은 종료를 `kProcessExit`와 `ExitProcess(0x…)`로 기록하고, CLI는 이를 continuation 경계로 출력한다.
  ***Output.** The continuation records the exit as `kProcessExit` with `ExitProcess(0x…)`, and the CLI prints it as a continuation boundary.*
- **x86 TLS 슬롯 누수 수정.** 새 종료 검사가 한 프로세스에서 세 번째 bootstrap을 만들자 "cannot allocate guest FS descriptor"로 실패했다. x86 bootstrap 소멸자는 `set_thread_area`에 `seg_not_present`만 켜서 넘기고 있었다. kernel은 `read_exec_only`와 `seg_not_present`가 모두 켜진 "빈" 형태만 슬롯 반환으로 처리하므로, 기존 코드로는 3개뿐인 TLS GDT 슬롯이 반환되지 않았다(glibc가 하나 사용). `read_exec_only`를 함께 켜도록 고쳤다. 제품은 프로세스당 한 번만 실행해서 이 문제가 드러나지 않았다.
  ***x86 TLS slot leak fix.** The new exit check failed with "cannot allocate guest FS descriptor" once a process created its third bootstrap: the x86 bootstrap destructor passed `set_thread_area` a descriptor with only `seg_not_present` set, but the kernel releases a slot only for the "empty" pattern with both `read_exec_only` and `seg_not_present`, so the three TLS GDT slots (one used by glibc) were never returned. The destructor now sets `read_exec_only` too; the product runs once per process, which hid the leak.*
- **probe.** 두 폭의 in-process probe에 합성 종료 검사를 추가했다. 첫 import가 `exit_process`(7)를 돌려주면, 실행이 fault 없이 끝나고 import 호출 1회, exit code 7이어야 한다. x64에서는 이어서 정상 실행(exit 51)도 확인한다.
  ***Probes.** Added a synthetic exit check to both widths' in-process probes: when the first import returns `exit_process` (7), the run completes without a fault after one import with exit code 7, and on x64 a following normal run still exits with 51.*

## 검증 / Validation

환경은 WSL2 Ubuntu 24.04와 Windows 11 host(Visual Studio, Win32)다. 실제 CHD는 `roms/ez2dj4th/4thTrax.chd`를 읽기 전용으로 사용했다.

*Environment: WSL2 Ubuntu 24.04 and the Windows 11 host (Visual Studio, Win32). The real CHD `roms/ez2dj4th/4thTrax.chd` was used read-only.*

| 항목 / Item | 결과 / Result |
| --- | --- |
| Linux x64·x86 debug, x86 helper build | 경고·오류 없음 / no warnings or errors |
| Linux x64·x86 CTest | 각각 3/3 통과(`kernel32` 단위 테스트 포함) / 3/3 each, including the `kernel32` unit tests |
| x64 in-process probe | `linux-x64-exit-probe: ExitProcess(7) after 1 import`, 이어진 실행 exit 51 / following run exits with 51 |
| x86 in-process probe | `imports=2 dynamic=2 exit=51 signal=4 process-exit=7` |
| `scripts/test_linux_native_helper_probe.sh` | exit 0, 두 host 모두 `result=51` / both hosts |
| `--linux-in-process-*` 네 진단 / four diagnostics | 두 폭 모두 작업 356과 같음(trace 43 frame 포함) / same as Task 356 on both widths, including the 43-frame trace |
| `--linux-helper` (x64) | `first boundary: import kernel32.dll!GetModuleHandleA`, 변경 없음 / unchanged |
| Windows x86 build | 오류·경고 없음 / no errors or warnings |
| Windows x86 CTest | 6개 중 5개 통과. `re2dj_windows_vfs_runtime_probe` 실패("audio trace omitted streaming start"). 변경을 stash한 `HEAD`에서도 같은 메시지로 실패해 기존 문제로 확인했다(TODO의 기존 항목). 이 probe는 바뀐 코드를 포함하지 않는다 / 5 of 6 pass; `re2dj_windows_vfs_runtime_probe` fails identically at `HEAD` with the changes stashed, a pre-existing TODO item that does not include the changed code |

실제 4th CHD의 기본 `re2dj ez2dj4th` 실행은 x64와 x86이 같다. SEH 1회 뒤 API 19개를 기록하고 `#0019 user32.dll!MessageBoxA`(정적 import)에서 미처리로 정지한다. `#0016`은 `0x6f00204c`를, `#0017 GetProcAddress("GetModuleHandleA")`는 `0x6f002000`을 받고, `#0018 GetModuleHandleA("DDRAW.DLL")`은 0을 받는다. 해석은 [분석 문서](../analysis/ez2dj4th-linux-inprocess-first-import.md)의 작업 358 절에 두었다.

*The default `re2dj ez2dj4th` run on the real 4th CHD is identical on x64 and x86: after one SEH dispatch it records 19 APIs and stops unhandled at `#0019 user32.dll!MessageBoxA` (a static import), with `#0016` receiving `0x6f00204c`, `#0017 GetProcAddress("GetModuleHandleA")` receiving `0x6f002000`, and `#0018 GetModuleHandleA("DDRAW.DLL")` receiving 0. The interpretation is in the Task 358 section of the [analysis](../analysis/ez2dj4th-linux-inprocess-first-import.md).*

## 다음 / Next

`user32` facade에 `MessageBoxA`를 제공한다. 그러면 정적 IAT slot이 facade thunk로 재결합된다. 문구를 관찰하고, 게스트가 뒤이어 `ExitProcess`를 실제로 호출하는지 확인한다. 호출한다면 이번 작업의 종료 경로가 실제 원본에서 처음 쓰이게 된다.

*Provide `MessageBoxA` in the `user32` facade so its static IAT slot is rebound to a facade thunk, observe its text, and see whether the guest then actually calls `ExitProcess`, which would exercise this task's exit path on the real original for the first time.*
