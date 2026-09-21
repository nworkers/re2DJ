# EZ2DJ 4th Linux in-process 첫 import / EZ2DJ 4th Linux in-process first import

## 확인됨 / Confirmed

2026-09-19 WSL Linux x86 `NativeInProcessRunner`에서 사용자가 제공한 `4thTrax.chd`의 `EZ2DJ/EZ2DJ.EXE`를 임시 파일로 materialize해 실행했습니다. 첫 handler는 stack argument `"kernel32"`를 확인하고 EAX=1 및 4-byte stdcall cleanup을 반환했습니다. guest는 원래 caller return address `0x00ae028a`로 복귀했고, 그 위치에 둔 process-local `INT3`는 다음 EIP `0x00ae028b`에서 SIGTRAP으로 관측됐습니다.

*Confirmed — On 2026-09-19, `EZ2DJ/EZ2DJ.EXE` from the user-provided `4thTrax.chd` was materialized to a temporary file and run in WSL Linux x86 `NativeInProcessRunner`. The first handler verified stack argument `"kernel32"` and returned EAX=1 with four-byte stdcall cleanup. The guest returned to original caller address `0x00ae028a`; a process-local `INT3` placed there was observed as SIGTRAP at next EIP `0x00ae028b`.*

## 미확정 / Unresolved

EAX=1은 caller 복귀 ABI를 관측하기 위한 일회성 pseudo handle이며 실제 kernel32 module handle 의미나 이후 `GetProcAddress` 호환을 검증하지 않습니다. breakpoint 이후 API도 실행하지 않았습니다.

*Unresolved — EAX=1 is a one-time pseudo handle used to observe caller-return ABI. It does not verify real kernel32 module-handle semantics or later `GetProcAddress` compatibility. No API after the breakpoint was executed.*

## 확인됨: 첫 dynamic resolver / Confirmed: first dynamic resolver

2026-09-20 Linux x86 CLI의 `--linux-in-process-first-resolver`는 pseudo kernel32 identity 뒤 `GetProcAddress(kernel32, "GetVersion")`를 관측했습니다. caller return은 `0x00af0b99`, SIGTRAP EIP는 `0x00af0b9a`였습니다. 이는 기존 bounded runtime trace의 caller와 일치합니다.

*Confirmed — On 2026-09-20, Linux x86 CLI `--linux-in-process-first-resolver` observed `GetProcAddress(kernel32, "GetVersion")` after pseudo kernel32 identity. Caller return was `0x00af0b99` and SIGTRAP EIP was `0x00af0b9a`. This matches the caller in existing bounded runtime trace.*

## 확인됨 — GetVersion thunk 연속 실행 경계 / Confirmed — GetVersion thunk continuation boundary

2026-09-20 Linux x86 CLI `--linux-in-process-getversion-call`은 `GetProcAddress(kernel32, "GetVersion")`에 process-local executable thunk를 반환한 뒤 계속 실행했습니다. dynamic gate에는 도달하지 않았고, guest는 thunk 호출 전에 signal 11 (`SIGSEGV`), EIP `0x00af0c22`에서 중단했습니다. 이 실행에서 GetVersion API 호출은 확인되지 않았습니다.

*Confirmed — On 2026-09-20, Linux x86 CLI `--linux-in-process-getversion-call` returned a process-local executable thunk for `GetProcAddress(kernel32, "GetVersion")` and continued execution. The dynamic gate was not reached; the guest stopped before the thunk call with signal 11 (`SIGSEGV`) at EIP `0x00af0c22`. GetVersion API invocation is not confirmed for this run.*

## 미확정 — 이후 호출 경로 / Unresolved — Later call paths

이 중단은 현재의 최소 resolver 연속 실행 경로에서 thunk 호출을 관측하지 못했다는 사실만 뜻합니다. 이후 보호 경로 또는 다른 실행 조건에서 GetVersion이 호출되는지, 그리고 그것이 필요한 OS 버전 의미는 미확정입니다.

*This stop means only that the minimum resolver-continuation path did not observe a thunk call. Whether a later protected path or different execution condition calls GetVersion, and what OS-version semantics it requires, remain unresolved.*
## 확인됨 — GetVersion thunk 전 null read fault / Confirmed — Null-read fault before GetVersion thunk

2026-09-20 Linux x86 `--linux-in-process-getversion-call` fault context에서 signal 11, EIP `0x00af0c22`, fault address `0x00000000`, x86 error code `0x00000004`가 관측되었습니다. live instruction window의 EIP 위치 bytes `8a 08`은 `MOV CL, byte ptr [EAX]`로 해석되고, 같은 context의 `EAX=0`과 일치합니다. 그러므로 현재 최소 resolver 연속 실행 경로는 dynamic GetVersion thunk에 도달하기 전에 null address read로 중단합니다.

*Confirmed — On 2026-09-20, Linux x86 `--linux-in-process-getversion-call` fault context recorded signal 11, EIP `0x00af0c22`, fault address `0x00000000`, and x86 error code `0x00000004`. The live instruction-window bytes at EIP, `8a 08`, decode as `MOV CL, byte ptr [EAX]`, matching `EAX=0` in the same context. The current minimum resolver-continuation path therefore stops on a null-address read before reaching the dynamic GetVersion thunk.*

`0x4`는 user-mode non-present page read와 일치합니다. 이 관측은 EAX가 왜 zero였는지, 어떤 앞선 API 또는 보호 상태가 그 값을 만들어야 하는지는 판정하지 않습니다. 그 값의 producer와 필요한 HLE 경계는 미확정입니다.

*`0x4` matches a user-mode read from a non-present page. This observation does not determine why EAX was zero or which preceding API or protection state should have produced its value. The producer and required HLE boundary remain unresolved.*
## 확인됨 — null read 직전의 조건 분기 / Confirmed — Conditional branch before the null read

2026-09-20 task 337은 live fault window를 EIP 전 64 bytes와 후 16 bytes로 넓혔습니다. EIP `0x00af0c22`는 window offset 64이고, 그 직전의 확인된 실행 순서는 `0x00af0c1a: XOR ECX, ECX`, `0x00af0c1c: JB 0x00aef590`, `0x00af0c22: MOV CL, byte ptr [EAX]`입니다. fault context의 EFLAGS `0x00010246`은 CF=0이고 `XOR`와 `MOV`가 이를 바꾸지 않으므로, `JB`는 분기하지 않고 null read로 진행한 것이 확인됩니다.

*Confirmed — On 2026-09-20, task 337 expanded the live fault window to 64 bytes before EIP and 16 after it. EIP `0x00af0c22` is window offset 64, and the confirmed immediate execution sequence is `0x00af0c1a: XOR ECX, ECX`, `0x00af0c1c: JB 0x00aef590`, `0x00af0c22: MOV CL, byte ptr [EAX]`. Fault-context EFLAGS `0x00010246` has CF=0, and neither `XOR` nor `MOV` changes it afterward, confirming that `JB` did not branch and execution continued to the null read.*

## Task 337 한계 — EAX producer / Task 337 limit — EAX producer

Task 337의 80-byte fault window만으로는 EAX를 0으로 만든 producer를 확정할 수 없었습니다. 이후 Task 338의 실제 instruction trace가 그 producer를 현재 최소 진단의 미처리 CreateFileA dynamic resolver 반환으로 확인했습니다. 그 결과는 바로 다음 절에 기록합니다.

*Task 337's 80-byte fault window alone could not establish the producer that made EAX zero. Task 338's real instruction trace subsequently identifies that producer as the unhandled CreateFileA dynamic resolver return in the current minimum diagnostic. The evidence is recorded in the next section.*
## 확인됨 — resolver-continuation 실제 instruction trace / Confirmed — real resolver-continuation instruction trace

2026-09-20 task 338의 Linux i386 GetVersion-call 진단은 GetProcAddress(kernel32, GetVersion)의 guest return 0x00af0b99에서 trace를 시작했습니다. 37개 guest frame 뒤 기존 null read EIP 0x00af0c22까지 도달했으며 trace limit에는 도달하지 않았습니다.

*On 2026-09-20, Task 338's Linux i386 GetVersion-call diagnostic began tracing at guest return 0x00af0b99 from GetProcAddress(kernel32, GetVersion). It reached the existing null read at EIP 0x00af0c22 after 37 guest frames without reaching the trace limit.*

frame 7의 0x00af09f0은 native import thunk로 넘어가는 지점이고, host bridge를 거쳐 frame 9의 guest return 0x00af09f6으로 돌아왔을 때 EAX는 0입니다. 진단 handler는 이 요청의 stack 인자를 kernel32와 CreateFileA로 확인했습니다. 이후 EAX는 frame 30부터 36의 null read까지 0으로 유지됩니다. 따라서 현재 최소 Linux resolver 진단에서는 CreateFileA 동적 resolver 요청을 처리하지 않아 반환한 0이 0x00af0c22 null read로 이어졌음이 확인되었습니다.

*Frame 7 at 0x00af09f0 transfers to a native import thunk, and EAX is zero when execution returns through the host bridge to guest frame 9 at 0x00af09f6. The diagnostic handler verified kernel32 and CreateFileA as the request stack arguments. EAX then remains zero from frame 30 through the null read at frame 36. Therefore, in the current minimum Linux resolver diagnostic, the zero returned because the dynamic CreateFileA resolver request is not handled leads to the 0x00af0c22 null read.*

이는 CreateFileA HLE 하나만으로 이후의 전체 보호 초기화가 성공한다는 뜻은 아닙니다. 실제 파일·장치 계약과 뒤이은 동적 API 요청은 별도로 확인해야 합니다. 다만 GetVersion thunk 자체보다 먼저 구현·검증할 다음 HLE 경계가 GetProcAddress(kernel32, CreateFileA)임은 이 실행에서 확인되었습니다.

*This does not establish that a CreateFileA HLE alone will make later protection initialization succeed. Its real file/device contract and later dynamic API requests still require independent confirmation. It does establish that GetProcAddress(kernel32, CreateFileA), rather than the GetVersion thunk itself, is the next HLE boundary to implement and verify for this run.*

## 확인됨 — 첫 CreateFileA 동적 thunk 호출 / Confirmed — first dynamic CreateFileA thunk call

2026-09-21 task 339의 Linux i386 `--linux-in-process-createfile-call` 진단은 `GetProcAddress(kernel32, CreateFileA)`에 별도 executable thunk를 반환했습니다. 원본은 그 주소를 실제로 호출했고 7개 stdcall 인자를 전달했습니다. 첫 파일 이름은 ASLR에 따라 실행마다 주소가 달라지는 guest stack 안의 `\\.\NTICE`이고, caller return은 `0x00aeffbc`였습니다. thunk가 `INVALID_HANDLE_VALUE`와 28바이트 cleanup을 반환한 뒤 process-local breakpoint는 EIP `0x00aeffbd`에서 관측되었습니다.

*On 2026-09-21, Task 339's Linux i386 `--linux-in-process-createfile-call` diagnostic returned a separate executable thunk for `GetProcAddress(kernel32, CreateFileA)`. The original code actually called that address and supplied seven stdcall arguments. The first file name was `\\.\NTICE` on the guest stack, whose address varies per run under ASLR, and the caller return was `0x00aeffbc`. After the thunk returned `INVALID_HANDLE_VALUE` with 28-byte cleanup, the process-local breakpoint was observed at EIP `0x00aeffbd`.*

확인된 scalar 인자는 desired access `0xc0000000`(`GENERIC_READ | GENERIC_WRITE`), share mode `0x00000003`(`FILE_SHARE_READ | FILE_SHARE_WRITE`), null security attributes, creation disposition `0x00000003`(`OPEN_EXISTING`), flags/attributes 0, null template handle입니다.

*The confirmed scalar arguments are desired access `0xc0000000` (`GENERIC_READ | GENERIC_WRITE`), share mode `0x00000003` (`FILE_SHARE_READ | FILE_SHARE_WRITE`), null security attributes, creation disposition `0x00000003` (`OPEN_EXISTING`), flags/attributes zero, and a null template handle.*

## 미확정 — NTICE handle 이후 경로 / Unresolved — path after the NTICE handle

이번 진단은 파일이나 장치를 열지 않고 고정 실패 handle로 복귀했으므로 `\\.\NTICE`의 성공·실패 의미, guest last-error, handle 수명, 이후 `DeviceIoControl` 또는 다음 동적 resolver 순서는 미확정입니다. 다음 구현 경계는 이 확인된 계약을 공용 guest module/handle service와 Linux VFS/device policy에 연결하는 것입니다.

*This diagnostic opens no file or device and returns a fixed failure handle, so the success/failure semantics of `\\.\NTICE`, guest last-error state, handle lifetime, and any later `DeviceIoControl` or dynamic-resolver sequence remain unresolved. The next implementation boundary is to connect this confirmed contract to the shared guest module/handle service and Linux VFS/device policy.*
