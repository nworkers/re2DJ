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

## 확인됨 — resolver identity가 관측으로 확정됨 / Confirmed — resolver identity established by observation

작업 344는 이 진단의 pseudo `kernel32` handle `0x7F000001`과 API별 resolver 분기를 제거하고, 정적 IAT와 동적 `GetProcAddress`를 작업 343의 PE32 facade 하나로 합류시켰다. 실제 `roms/ez2dj4th/ez2dj4th.chd` 실행에서 세 경로가 같은 주소를 가리키는 것을 **관측**했다.

| 항목 | registry | 정적 IAT slot | 게스트가 받은 값 |
| --- | --- | --- | --- |
| `kernel32` module handle | `0x6f000000` | — | `0x6f000000` |
| `CreateFileA` | `0x6f002039` | `0x6f002039` | `0x6f002039` |
| `GetVersion` | `0x6f002026` | — | `0x6f002026` |

정적 IAT 값은 기록한 값을 신뢰하지 않고 image가 mapping된 상태에서 slot을 되읽어 얻었다. 게스트가 받은 값은 resolver가 실제로 반환한 값이며, 준비 단계에서 미리 채우지 않는다. 따라서 세 열이 일치한다는 것은 구조상 참인 명제가 아니라 이 실행에서 확인된 사실이다.

같은 실행이 `CreateFileA("\\.\NTICE")`에 7개 인자로 도달하고 caller 복귀 지점의 `INT3`에서 제한되었다. 인자 값은 위 절과 같다.

*Task 344 removed this diagnostic's pseudo `kernel32` handle `0x7F000001` and its per-API resolver branches, converging the static IAT and dynamic `GetProcAddress` on the single PE32 facade from Task 343. A real `roms/ez2dj4th/ez2dj4th.chd` run **observed** all three paths pointing at the same addresses, as tabulated above.*

*The static IAT column is read back from the slot while the image is mapped rather than trusting the value written, and the guest column is what the resolver actually returned, never pre-seeded during preparation. The three columns agreeing is therefore a fact confirmed in this run, not a proposition true by construction.*

*The same run reached `CreateFileA("\\.\NTICE")` with seven arguments and was bounded at the caller's return-site `INT3`, with the argument values recorded in the section above.*

**미확정.** `\.\NTICE` 이후 경로는 위 절과 같이 그대로 미확정이다. 이 작업은 resolver 경계만 확정했고 guest handle 의미는 다루지 않는다.

*Unresolved: the path after `\.\NTICE` remains as stated above. This task settles the resolver boundary only and does not address guest-handle semantics.*

## 확인됨 — facade 경로에서 GetVersion 실제 호출 / Confirmed — actual GetVersion call on the facade path

2026-09-23 작업 348은 `--linux-in-process-getversion-call`에서 pseudo handle과 진단 전용 `GetVersion` thunk를 제거하고, 작업 344의 `kernel32` facade로 모든 gate를 처리하게 했다. 실제 4th CHD 실행에서 원본은 `GetVersion` facade thunk `0x6f002026`을 실제로 호출했고, caller 복귀 `0x00aefd82`의 `INT3`에서 SIGTRAP EIP `0x00aefd83`으로 제한되었다. registry, 정적 IAT slot, 게스트가 받은 값은 작업 344와 같은 `0x6f000000`, `0x6f002039`, `0x6f002026`으로 일치했다.

`GetProcAddress(kernel32, "GetVersion")` 복귀 `0x00af0b99`에서 시작한 instruction trace는 43개 frame 뒤 `GetVersion` 호출에 도달했고 trace 한도에는 닿지 않았다. 확인된 순서는 다음과 같다.

| frame | EIP | 관측 |
| --- | --- | --- |
| #000 | `0x00af0b99` | EAX=`0x6f002026`. `GetVersion` resolver 반환 |
| #007–#009 | `0x00af09f0` → facade `0x6f002013` → `0x00af09f6` | 두 번째 resolver 요청. 복귀 EAX=`0x6f002039`(`CreateFileA` thunk). 작업 338에서는 같은 frame의 EAX가 0이었다 |
| #036–#037 | `0x00af0c22` | `MOV CL, byte ptr [EAX]`가 EAX=`0x6f002039`를 읽어 CL=`0x68`. 이전의 null read 지점이며 이번에는 fault가 없다 |
| #040–#042 | `0x00aefd7c` → `0x00aefd7f` → `0x6f002026` | `GetVersion` facade thunk 호출. 복귀 주소 `0x00aefd82` |

따라서 작업 338이 확인한 null read는 `CreateFileA` resolver가 0을 반환한 결과였다는 결론이 이 실행으로 다시 뒷받침된다. resolver가 실제 thunk를 반환하자 원본은 그 주소의 첫 바이트를 읽고 계속 진행해 `GetVersion`을 호출했다. 43개 frame 어디에도 `CreateFileA` thunk `0x6f002039`로의 진입은 없으므로, 이 실행에서 `GetVersion` 호출은 모든 `CreateFileA` 호출보다 앞선다. CreateFileA 진단의 `\\.\NTICE` 호출(caller 복귀 `0x00aeffbc`)이 같은 경로에서 `GetVersion` 뒤에 온다는 것은 **추정**이다. 두 진단을 한 실행에서 함께 관측하지는 않았다.

*On 2026-09-23, Task 348 removed the pseudo handle and diagnostic-only `GetVersion` thunk from `--linux-in-process-getversion-call` and let Task 344's `kernel32` facade handle every gate. On the real 4th CHD the original code actually called the `GetVersion` facade thunk `0x6f002026` and was bounded by the `INT3` at caller return `0x00aefd82`, with SIGTRAP EIP `0x00aefd83`. Registry, static IAT slot, and guest-received values agreed with Task 344: `0x6f000000`, `0x6f002039`, `0x6f002026`.*

*The instruction trace starting at the `GetProcAddress(kernel32, "GetVersion")` return `0x00af0b99` reached the `GetVersion` call after 43 frames without hitting the trace limit. Frames #007–#009 are the second resolver request through facade `0x6f002013`, now returning EAX `0x6f002039` (the `CreateFileA` thunk) where Task 338 saw zero. At frames #036–#037, `MOV CL, byte ptr [EAX]` at the former null-read site `0x00af0c22` reads `0x68` from `0x6f002039` without faulting. Frames #040–#042 call the `GetVersion` facade thunk from `0x00aefd7f`, returning to `0x00aefd82`.*

*This run re-confirms Task 338's conclusion that the null read came from the zero `CreateFileA` resolver result: once the resolver returned a real thunk, the original read the first byte at that address, continued, and called `GetVersion`. No frame among the 43 enters the `CreateFileA` thunk `0x6f002039`, so in this run the `GetVersion` call precedes every `CreateFileA` call. That the CreateFileA diagnostic's `\\.\NTICE` call (caller return `0x00aeffbc`) follows `GetVersion` on the same path is **inferred**; the two diagnostics were not observed together in one run.*

**추정.** `0x00af0c22`에서 API 주소의 첫 바이트를 읽는 것은 보호 코드가 흔히 쓰는 API 진입부 검사(예: `0xCC` breakpoint 탐지)와 모양이 같다. 그러나 읽은 값 `0x68`을 무엇과 비교하는지는 이 trace의 레지스터만으로 확인하지 않았다.

*Inferred: reading the first byte of an API address at `0x00af0c22` matches the shape of the API-entry checks protection code commonly uses, such as `0xCC` breakpoint detection. What `0x68` is compared against was not established from this trace's registers.*

**미확정.** `GetVersion` handler는 0을 반환하고 진단은 그 복귀에서 멈추므로, 원본이 요구하는 OS 버전 값과 그 값에 따른 분기는 확인하지 않았다. 호출이 여러 번 일어나는지도 확인하지 않았다.

*Unresolved: the `GetVersion` handler returns zero and the diagnostic stops at that return, so the OS-version value the original expects and any branch on it are not established, nor whether the call happens more than once.*

`--linux-in-process-first-resolver`는 facade 경로에서도 caller 복귀 `0x00af0b99`, SIGTRAP EIP `0x00af0b9a`를 유지했다. 게스트가 받은 `GetVersion` 주소는 registry 값 `0x6f002026`과 같고, 이 경계는 `CreateFileA` 요청보다 앞서므로 `CreateFileA` 열은 "요청 안 됨"으로 보고된다.

*`--linux-in-process-first-resolver` keeps caller return `0x00af0b99` and SIGTRAP EIP `0x00af0b9a` on the facade path. The guest-received `GetVersion` address equals the registry value `0x6f002026`, and since this boundary precedes the `CreateFileA` request, the `CreateFileA` column is reported as never requested.*

## 확인됨 — facade 연속 실행의 첫 13개 API와 guest INT3 / Confirmed — the first 13 APIs of a facade continuation and a guest INT3

2026-09-23 작업 349의 `--linux-in-process-continue`는 원본을 `kernel32` facade 위에서 멈추지 않고 실행했다. 실제 4th CHD에서 기록된 호출은 다음 13개다. 반환 주소는 ASLR과 무관하게 실행마다 같았다.

| # | API | caller 복귀 | 인자 / 문자열 | 반환 |
| --- | --- | --- | --- | --- |
| 1 | `GetModuleHandleA` | `0x00ae028a` | `"kernel32"` | `0x6f000000` |
| 2 | `GetModuleHandleA` | `0x00ae029a` | `"user32"` | `0` |
| 3 | `GetProcAddress` | `0x00af0b99` | `"GetVersion"` | `0x6f002026` |
| 4 | `GetProcAddress` | `0x00af09f6` | `"CreateFileA"` | `0x6f002039` |
| 5 | `GetVersion` | `0x00aefd82` | — | `0x23f00206` |
| 6 | `CreateFileA` | `0x00aeffbc` | `"\.\NTICE"` | `INVALID_HANDLE_VALUE` |
| 7 | `GetProcAddress` | `0x00b18787` | `"GetVersion"` | `0x6f002026` |
| 8 | `GetProcAddress` | `0x00b17f76` | `"CreateFileA"` | `0x6f002039` |
| 9 | `GetVersion` | `0x00b18f96` | — | `0x23f00206` |
| 10 | `CreateFileA` | `0x00b18cd2` | `"\.\NTICE"` | `INVALID_HANDLE_VALUE` |
| 11 | `GetProcAddress` | `0x00af15b9` | `"GetVersion"` | `0x6f002026` |
| 12 | `GetProcAddress` | `0x00af15d6` | `"CreateFileA"` | `0x6f002039` |
| 13 | `GetVersion` | `0x00af1f9c` | — | `0x23f00206` |

세 곳의 코드(`0x00aef…`/`0x00af0…`, `0x00b17…`/`0x00b18…`, `0x00af1…`)가 같은 "resolver 두 번 → `GetVersion` → `CreateFileA`" 순서를 되풀이한다. 두 `CreateFileA`는 모두 작업 339와 같은 access, share, disposition을 쓴다. 세 번째 묶음은 `GetVersion` 뒤 `CreateFileA`를 부르기 전에 끝난다.

13번 호출 뒤 게스트는 SIGTRAP(`si_code` 128, `SI_KERNEL`)으로 멈췄다. EIP는 `0x00af1136`이고, fault window에서 EIP 바로 앞 바이트는 `0xCC`다. 즉 **게스트 자신의 코드 `0x00af1135`에 있는 `INT3`가 실행되었다.** 진단은 guest 코드를 쓰지 않으며 이 실행의 정지 stub도 image 밖에 있으므로, 이 `INT3`는 진단이 둔 것이 아니다. 그 시점의 레지스터는 ESI=`0x00004647`, EDI=`0x00004a4d`, ECX=`0x00004450`, EAX=`0x40128300`였다.

`GetVersion`이 0을 돌려주는 임시 빌드로도 같은 실행을 했다. 13개 호출의 API, 순서, 복귀 주소와 마지막 SIGTRAP 위치·레지스터가 모두 같았고, 달라진 것은 `GetVersion` 반환값과 ASLR stack 주소뿐이었다. 따라서 **이 경계까지는 `GetVersion` 값이 분기를 바꾸지 않는다.**

`GetModuleHandleA("user32")`는 facade에 `user32`가 없어 0을 받았고 게스트는 그대로 진행했다. 4th EXE는 `USER32.dll`을 정적 import하므로 실제 Windows에서는 0이 아니었을 것이다. 이 차이가 이후 경로에 영향을 주는지는 **미확정**이다.

*On 2026-09-23, Task 349's `--linux-in-process-continue` ran the original on the `kernel32` facade without stopping it. On the real 4th CHD it recorded the 13 calls tabulated above, with return addresses identical across runs regardless of ASLR. Three code regions repeat the same "two resolver calls → `GetVersion` → `CreateFileA`" sequence; both `CreateFileA` calls use Task 339's access, share, and disposition, and the third group ends after `GetVersion` before any `CreateFileA`.*

*After call 13 the guest stopped with SIGTRAP (`si_code` 128, `SI_KERNEL`) at EIP `0x00af1136`, and the fault window shows `0xCC` immediately before EIP: **an `INT3` in the guest's own code at `0x00af1135` executed.** The diagnostic writes no guest code and its stop stub lies outside the image, so this `INT3` is not the diagnostic's. Registers at that point were ESI `0x00004647`, EDI `0x00004a4d`, ECX `0x00004450`, EAX `0x40128300`.*

*A temporary build returning zero from `GetVersion` produced the same 13 APIs, order, return addresses, and final SIGTRAP location and registers; only the `GetVersion` result and ASLR stack addresses differed. **Up to this boundary the `GetVersion` value does not change the branch.***

*`GetModuleHandleA("user32")` received zero because the facade has no `user32`, and the guest continued. The 4th EXE statically imports `USER32.dll`, so real Windows would not have returned zero; whether this divergence affects later paths is **unresolved**.*

## 확인됨 — INT3 시점의 게스트 SEH 프레임 및 핸들러 확인 / Confirmed — Guest SEH frame and handler confirmed at INT3

2026-09-23 작업 350은 Linux i386 연속 실행 진단에 TEB `FS:[0]` 및 SEH 체인 관찰을 추가했습니다. 실제 4th CHD 연속 실행에서 `0x00af1135` (`INT3`) 발생 시점의 SEH 상태를 관측한 결과는 다음과 같습니다.

* `TEB`: `0xf7ecb000` (유효한 게스트 TEB 매핑)
* `FS:[0]` (`Tib.ExceptionList`): `0xf7756e0c` (게스트 스택 범위 내)
* `Next`: `0xFFFFFFFF` (체인의 마지막)
* `Handler`: `0x00af159b` (게스트 PE 이미지의 `.protect` 섹션 내부 주소)
* `Handler` 진입 코드 바이트: `8b 44 24 0c` (`mov eax, [esp+0x0c]`)

Win32 x86 SEH 핸들러 규약 `ExceptionHandler(pRecord, pFrame, pContext, pDispatcher)`에서 `[esp+0x0c]`는 세 번째 인자인 `PCONTEXT ContextRecord`입니다. 즉, 등록된 핸들러는 진입 즉시 `ContextRecord` 포인터를 EAX에 로드하여 예외 문맥을 다루도록 작성되어 있습니다. 이 핸들러 주소는 직전에 11, 12번째 resolver(`GetVersion`, `CreateFileA`)를 호출했던 보호 코드 영역의 함수 진입점입니다.

따라서 게스트의 `INT3`는 SoftICE 안티 디버깅 탐지를 위해 의도적으로 발생시킨 것이며, SoftICE가 없을 경우 OS가 이를 Win32 `EXCEPTION_BREAKPOINT`로 변환하여 `FS:[0]`에 등록된 `0x00af159b` 핸들러로 전달할 것을 기대하고 있음이 **확인**되었습니다.

*On 2026-09-23, Task 350 added TEB `FS:[0]` and SEH chain observation to the Linux i386 continuation diagnostic. Observing the SEH state at `0x00af1135` (`INT3`) on the real 4th CHD yielded:*
* *`TEB`: `0xf7ecb000` (valid guest TEB mapping)*
* *`FS:[0]` (`Tib.ExceptionList`): `0xf7756e0c` (within guest stack bounds)*
* *`Next`: `0xFFFFFFFF` (end of chain)*
* *`Handler`: `0x00af159b` (within the guest PE image `.protect` section)*
* *`Handler` entry bytes: `8b 44 24 0c` (`mov eax, [esp+0x0c]`)*

*In the Win32 x86 SEH calling convention `ExceptionHandler(pRecord, pFrame, pContext, pDispatcher)`, `[esp+0x0c]` is the third argument, `PCONTEXT ContextRecord`. The registered handler begins by loading `ContextRecord` into EAX to manipulate exception context. This handler address is the function entry point of the protection code region that executed the 11th and 12th dynamic resolver calls.*

*This **confirms** that the guest's `INT3` was deliberately triggered for SoftICE anti-debugging, and without SoftICE, the guest expects the OS to deliver Win32 `EXCEPTION_BREAKPOINT` to the registered handler at `0x00af159b` via `FS:[0]`.*

## 확인됨 — 게스트 SEH 디스패치 성공 및 재개 후의 API 호출 (작업 351) / Confirmed — Guest SEH dispatch success and subsequent API calls (Task 351)

2026-09-23 작업 351은 Linux i386 in-process 런타임에 게스트 SEH 디스패처를 구현하고, 실제 4th CHD (`roms/ez2dj4th/ez2dj4th.chd`) 연속 실행을 통해 이를 검증했습니다.

`0x00af1135` (`INT3`) 발생 시 게스트의 `FS:[0]`에 등록된 핸들러 `0x00af159b`로 Win32 `EXCEPTION_BREAKPOINT`(`0x80000003`)와 `CONTEXT`를 전달하여 핸들러를 실행한 결과는 다음과 같습니다:

* **SEH 핸들러 반환**: `ExceptionContinueExecution` (`0`) 반환
* **재개 EIP**: 핸들러가 `ContextRecord.Eip`를 `0x00af1135`에서 `0x00af11af`로 직접 변경
* **실행 재개 성공**: 커널 `sigreturn`을 통해 게스트의 변경된 EIP 및 레지스터로 정상 복귀하여 실행 계속
* **이후 관측된 API 호출**:
  * **#0014**: `kernel32.dll!CreateFileA("\\.\FEnteDev")` (caller 복귀 `0x00af1ef9`) -> `INVALID_HANDLE_VALUE` (`0xFFFFFFFF`). FrogsICE 안티 디버깅 드라이버 탐지 시도이며, facade가 없는 핸들로 응답하여 디버거 미탐지로 통과.
  * **#0015**: `kernel32.dll!GetProcAddress(hModule=0x00000000, "GetActiveWindow")` (caller 복귀 `0x00ae4129`) -> `0x00000000` (미해결 조회로 정지)

15번 호출에서 `hModule`이 0인 이유는 앞서 2번 호출이었던 `GetModuleHandleA("user32")`에 대해 facade가 `user32` 모듈을 제공하지 않아 0을 반환했기 때문임이 **확인**되었습니다.

*On 2026-09-23, Task 351 implemented the guest SEH dispatcher in the Linux i386 in-process runtime and verified it against real 4th CHD (`roms/ez2dj4th/ez2dj4th.chd`) continuous execution.*

*When `0x00af1135` (`INT3`) triggered, delivering Win32 `EXCEPTION_BREAKPOINT` (`0x80000003`) and `CONTEXT` to the guest handler registered at `FS:[0]` (`0x00af159b`) produced the following confirmed results:*

* ***SEH Handler Disposition***: *Returned `ExceptionContinueExecution` (`0`)*
* ***Resumed EIP***: *Handler directly modified `ContextRecord.Eip` from `0x00af1135` to `0x00af11af`*
* ***Execution Resumed***: *Resumed cleanly via kernel `sigreturn` with updated guest registers and continued running*
* ***Subsequent API Calls Observed***:
  * ***#0014***: *`kernel32.dll!CreateFileA("\\.\FEnteDev")` (caller return `0x00af1ef9`) -> `INVALID_HANDLE_VALUE` (`0xFFFFFFFF`). Anti-debugging driver probe for FrogsICE; facade returned invalid handle, successfully simulating debugger absence.*
  * ***#0015***: *`kernel32.dll!GetProcAddress(hModule=0x00000000, "GetActiveWindow")` (caller return `0x00ae4129`) -> `0x00000000` (stopped at unresolved lookup)*

*It is **confirmed** that `hModule` was 0 at call #0015 because call #0002 `GetModuleHandleA("user32")` returned 0 from the facade, which currently provides only `kernel32.dll`.*

## 확인됨 — `user32` facade 제공 후의 다음 경계 (작업 352) / Confirmed — the next boundary once a `user32` facade exists (Task 352)

2026-09-23 작업 352는 `GetActiveWindow` 하나를 export하는 `user32` facade를 `kernel32` 뒤에 등록하고, 실제 4th CHD(`roms/ez2dj4th/4thTrax.chd`, 읽기 전용)로 `--linux-in-process-continue`를 다시 실행했다. 결과는 다음과 같다.

| # | 호출 | caller 복귀 | 결과 |
| --- | --- | --- | --- |
| 0002 | `GetModuleHandleA("user32")` | `0x00ae029a` | `0x6eff0000` (`user32` facade base. 전에는 0) |
| 0015 | `GetProcAddress(0x6eff0000, "GetActiveWindow")` | `0x00ae4129` | `0x6eff2000` (facade thunk) |
| 0016 | `GetProcAddress(0x6f000000, "ExitProcess")` | `0x00ae7030` | 0 → `kContinuationUnresolvedLookup`로 정지 |

* `#0001`~`#0014`의 API, 순서, caller 복귀 주소, SEH 전달(handler `0x00af159b`, 재개 `0x00af11af`)은 작업 351과 같다.
* 게스트는 `#0002`에서 받은 값을 그대로 `#0015`의 module 인자로 넘겼다. 작업 351에서 이 인자가 0이었던 원인이 `user32` 부재였다는 결론이 다시 뒷받침된다.
* 게스트는 `GetActiveWindow` 주소를 받은 뒤, 그 thunk를 **호출하지 않은 채** `#0016`에서 `kernel32`의 `ExitProcess`를 요청했다.
* `ExitProcess`는 4th의 packed 정적 import 목록에 없다([import surface §11](ez2dj-import-surface.md)). 따라서 게스트는 이 함수를 동적 해석으로만 얻는다.
* `kernel32` resolver identity(`0x6f000000`, `GetVersion` `0x6f002026`, `CreateFileA` `0x6f002039`)는 `user32` 조회가 추가된 뒤에도 그대로 일치한다.

**추정.** 두 장치 열기(`\\.\NTICE`, `\\.\FEnteDev`)가 모두 `INVALID_HANDLE_VALUE`를 받은 뒤, `GetActiveWindow`와 `ExitProcess`를 이어서 해석하는 모양은 보호 코드가 오류 경로를 준비하는 것과 맞는다. 예를 들어 active window를 부모로 삼아 `MessageBoxA`를 띄운 뒤 종료하는 경로일 수 있다. 반대로 쓰기 전에 API를 미리 모아 두는 것일 수도 있다. 이 실행만으로는 둘을 구분하지 못했다.

**미확정.** 게스트가 `GetActiveWindow`를 실제로 호출하는지, `ExitProcess` 해석이 실패 경로를 뜻하는지, 그리고 원래 진입점(OEP)에 도달하는지는 아직 확인하지 않았다. 다음 경계는 `kernel32` facade의 `ExitProcess`다.

*On 2026-09-23, Task 352 registered a `user32` facade exporting only `GetActiveWindow` after `kernel32` and reran `--linux-in-process-continue` on the real 4th CHD (`roms/ez2dj4th/4thTrax.chd`, read-only). `#0002 GetModuleHandleA("user32")` (caller return `0x00ae029a`) now receives the `user32` facade base `0x6eff0000` instead of zero; `#0015 GetProcAddress(0x6eff0000, "GetActiveWindow")` (caller return `0x00ae4129`) receives the facade thunk `0x6eff2000`; and `#0016 GetProcAddress(0x6f000000, "ExitProcess")` (caller return `0x00ae7030`) receives zero and stops the run with `kContinuationUnresolvedLookup`.*

*Calls `#0001`–`#0014` keep Task 351's APIs, order, caller return addresses, and SEH delivery (handler `0x00af159b`, resume `0x00af11af`). The guest passed the value from `#0002` straight into `#0015` as the module argument, again supporting the conclusion that the zero argument in Task 351 came from the missing `user32`. After receiving the `GetActiveWindow` address, the guest requested `kernel32`'s `ExitProcess` at `#0016` **without calling that thunk**. `ExitProcess` is absent from 4th's packed static imports ([import surface §11](ez2dj-import-surface.md)), so the guest obtains it only by dynamic resolution. The `kernel32` resolver identity (`0x6f000000`, `GetVersion` `0x6f002026`, `CreateFileA` `0x6f002039`) still matches with the `user32` lookup added.*

*Inferred: after both device opens (`\\.\NTICE`, `\\.\FEnteDev`) received `INVALID_HANDLE_VALUE`, resolving `GetActiveWindow` and then `ExitProcess` fits a protection stub preparing an error path, for example a `MessageBoxA` parented to the active window followed by termination. It could equally be resolving APIs ahead of use; this run does not distinguish the two.*

*Unresolved: whether the guest actually calls `GetActiveWindow`, whether resolving `ExitProcess` means a failure path, and whether the original entry point (OEP) is reached. The next boundary is `ExitProcess` in the `kernel32` facade.*

## 확인됨 — Linux x64 compatibility mode에서 같은 경계 (작업 355) / Confirmed — the same boundaries in Linux x64 compatibility mode (Task 355)

2026-09-24, 작업 355에서 Linux x86-64 host가 원본 4th를 compatibility mode(CS `0x23`)로 같은 프로세스 안에서 실행했다. 실제 4th CHD(`roms/ez2dj4th/4thTrax.chd`)는 읽기 전용으로 썼다. 이 host에는 아직 게스트 SEH 디스패치가 없다(작업 353 설계 4단계).

| 진단 / Diagnostic | x64 결과 / x64 result | x86 비교 / vs x86 |
| --- | --- | --- |
| `--linux-in-process-first-import` | return `0x00ae028a`, SIGTRAP `0x00ae028b` | 같음 / same |
| `--linux-in-process-first-resolver` | `GetVersion`, return `0x00af0b99`, `GetVersion` `0x6f002026` | 같음 / same |
| `--linux-in-process-createfile-call` | `\.\NTICE`, return `0x00aeffbc`, identity `0x6f000000`/`0x6f002039`/`0x6f002026` | 같음 / same |
| `--linux-in-process-continue` | API 13개 뒤 게스트 `INT3`에서 SIGTRAP, EIP `0x00af1136`. ESI `0x00004647`(`'FG'`), EDI `0x00004a4d`(`'JM'`). SEH frame handler `0x00af159b`, next `0xffffffff` | 작업 349·350의 경계와 같음. x86은 이후 SEH 디스패치로 `#0016`까지 진행 / same as the Tasks 349–350 boundary; x86 now continues to `#0016` through SEH dispatch |
| `--linux-in-process-getversion-call` | instruction trace 미구현으로 명시적 거절 / explicitly rejected, no instruction trace yet | x86은 trace 43 frame / x86 traces 43 frames |

* `#0001`–`#0013`의 API, 순서, caller 복귀 주소, 반환값이 x86과 같다. `GetVersion`은 x64에서도 `0x23f00206`을 돌려준다.
* host 배치에 따라 달라지는 값만 다르다. `CreateFileA` 경로 문자열 포인터(x64 `0xefbfffa0`, x86 `0xf7756fa0`), SEH frame 주소(x64 `0xefbffe0c`, x86 `0xf7756e0c`), TEB 주소가 그렇다. 게스트 stack과 TEB를 host가 서로 다른 주소에 두기 때문이다.

**추정.** 게스트 코드의 분기가 host 폭과 무관하게 같은 경로를 탔다. 따라서 `#0014` 이후도 x64에 SEH 디스패치를 붙이면 x86과 같아질 것으로 본다. 다만 이것은 아직 실행으로 확인하지 않았다.

*On 2026-09-24, Task 355 ran the original 4th inside a Linux x86-64 host process in compatibility mode (CS `0x23`), using the real 4th CHD (`roms/ez2dj4th/4thTrax.chd`) read-only. This host has no guest SEH dispatch yet (stage 4 of the Task 353 design). The first-import, first-resolver, and CreateFileA diagnostics match x86 exactly. The continuation records the same 13 APIs with identical order, caller return addresses, and return values (`GetVersion` still returns `0x23f00206`), then stops with SIGTRAP at EIP `0x00af1136` on the guest's own `INT3`, with ESI `'FG'`, EDI `'JM'`, and SEH handler `0x00af159b` — the Tasks 349–350 boundary. Only host-placement values differ: the `CreateFileA` path-string pointer, the SEH frame address, and the TEB address, because the host places the guest stack and TEB elsewhere. The GetVersion diagnostic is explicitly rejected because the x64 host has no instruction trace yet.*

*Inferred: guest branching followed the same path regardless of host width, so `#0014` onward is expected to match x86 once x64 gains SEH dispatch; this has not been run yet.*

## 확인됨 — x64 SEH 디스패치 후 x86과 같은 `#0016` (작업 356) / Confirmed — the same `#0016` as x86 after x64 SEH dispatch (Task 356)

2026-09-24, 작업 356에서 x64 host에 게스트 SEH 디스패치와 instruction trace를 붙였다. 그 뒤 실제 4th CHD(읽기 전용)의 결과는 다음과 같다.

* `--linux-in-process-continue`: SEH dispatch count 1, handler `0x00af159b`, resume `0x00af11af`. API 16개 뒤 `#0016 GetProcAddress(0x6f000000, "ExitProcess")`에서 정지한다. API, 순서, caller 복귀 주소, 반환값이 모두 x86과 같고, 달라지는 것은 stack 위치에 따른 `CreateFileA` 경로 포인터뿐이다.
* `--linux-in-process-getversion-call`: return `0x00aefd82`, trace 43 frame으로 x86과 같다. 43 frame의 EIP 순서, EAX·ECX·EDX·ESI·EDI는 모두 같다. ESP·EBP는 host가 배치한 stack 주소라 다르다.
* EBX는 43 frame 내내 x64 `0x00ae0240`, x86 `0xfffd86d0`로 다르다. 두 host 모두 게스트 entry에서 EBX를 정하지 않아 host에 남은 값이 들어간다. 그 영향으로 frame #019(`0x00af0240`)의 EFLAGS가 x64 `0x302`, x86 `0x382`(SF)로 한 번 다르고, 다음 frame에서 같아진다.

**추정.** frame #019의 SF 차이는 `0x00af023c`의 명령이 EBX 또는 stack 주소로 flags를 만든 결과로 보인다. 이후 경로가 같으므로 게스트는 이 값에 따라 분기하지 않았다.

**미확정.** 원본이 Windows에서 entry 시점 레지스터(예: EBX의 PEB 주소)에 의존하는지는 확인하지 않았다. 두 host 모두 entry 레지스터를 정의하지 않는다는 점은 후속 과제다.

*On 2026-09-24, Task 356 gave the x64 host guest SEH dispatch and the instruction trace. On the real 4th CHD (read-only), `--linux-in-process-continue` dispatches SEH once (handler `0x00af159b`, resume `0x00af11af`) and stops after 16 APIs at `#0016 GetProcAddress(0x6f000000, "ExitProcess")`, with every API, order, caller return, and return value equal to x86 except the stack-placed `CreateFileA` path pointer. `--linux-in-process-getversion-call` matches x86 (return `0x00aefd82`, 43 trace frames): EIP order and EAX, ECX, EDX, ESI, and EDI are identical across all 43 frames, while ESP and EBP are host-placed stack addresses. EBX differs throughout (x64 `0x00ae0240`, x86 `0xfffd86d0`) because neither host defines EBX at guest entry and a leftover host value flows in; as a result the EFLAGS at frame #019 (`0x00af0240`) differ once (x64 `0x302`, x86 `0x382`, the SF bit) and agree again at the next frame.*

*Inferred: the SF difference at frame #019 comes from the instruction at `0x00af023c` deriving flags from EBX or a stack address; the identical later path shows the guest did not branch on it.*

*Unresolved: whether the original depends on Windows entry-time registers (such as EBX holding the PEB) is unverified; that neither host defines entry registers is follow-up work.*

## 확인됨 — `ExitProcess` 해석 뒤의 다음 경계 (작업 358) / Confirmed — the next boundary after resolving `ExitProcess` (Task 358)

2026-09-24, 작업 358에서 `kernel32` facade에 `ExitProcess`를 추가했다. 그 뒤 실제 4th CHD(읽기 전용)의 in-process continuation은 x86과 x64에서 호출 기록이 완전히 같다(stack 위치에 따른 `CreateFileA` 경로 포인터 제외). `#0001`–`#0015`는 작업 356과 같다.

| # | 호출 / Call | caller 복귀 / Caller return | 결과 / Result |
| --- | --- | --- | --- |
| 0016 | `GetProcAddress(0x6f000000, "ExitProcess")` | `0x00ae7030` | `0x6f00204c` (facade thunk) |
| 0017 | `GetProcAddress(0x6f000000, "GetModuleHandleA")` | `0x00ae56ee` | `0x6f002000` (facade thunk) |
| 0018 | `GetModuleHandleA("DDRAW.DLL")` | `0x00ae6ee7` | 0 (등록된 module 아님 / not a registered module) |
| 0019 | `user32.dll!MessageBoxA` (정적 import / static import) | `0x00ae4d9c` | 미처리 → `kContinuationUnhandledImport`로 정지 / unhandled → stop |

* 게스트는 `ExitProcess` 주소를 받은 뒤 **호출하지 않았다**. 대신 `GetModuleHandleA`를 동적으로 해석해 `DDRAW.DLL`을 찾았다.
  *After receiving the `ExitProcess` address the guest did **not** call it; it dynamically resolved `GetModuleHandleA` and looked up `DDRAW.DLL`.*
* `MessageBoxA`는 facade export가 아니라 image의 정적 IAT로 부른다. `user32` facade는 `GetActiveWindow`만 제공하므로 이 slot은 원래 import thunk에 남아 있다.
  *`MessageBoxA` is called through the image's static IAT, not a facade export; the `user32` facade provides only `GetActiveWindow`, so this slot keeps its original import thunk.*

**추정.** 다음 흐름은 보호 stub의 오류 경로로 보인다. 장치 두 개(`\.\NTICE`, `\.\FEnteDev`)의 열기가 실패하고 `DDRAW.DLL`도 적재되어 있지 않으면, `MessageBoxA`로 오류를 표시하고 미리 해석해 둔 `ExitProcess`로 끝낸다. `GetActiveWindow`는 message box의 owner 창을 얻는 데 쓰였을 수 있다.

**미확정.** `MessageBoxA`의 인자(문구)와 그 뒤 실제로 `ExitProcess`를 부르는지는 아직 관찰하지 않았다. `DDRAW.DLL`이 적재되어 있다면 이 경로를 피하는지도 확인하지 않았다.

*Inferred: the flow fits a protection-stub error path — with both device opens (`\.\NTICE`, `\.\FEnteDev`) failed and `DDRAW.DLL` not loaded, it shows an error with `MessageBoxA` and ends with the pre-resolved `ExitProcess`; `GetActiveWindow` may supply the message box owner. Unresolved: the `MessageBoxA` arguments (its text), whether `ExitProcess` is actually called afterwards, and whether a loaded `DDRAW.DLL` avoids this path have not been observed.*

## 확인됨 — Hardlock 오류 대화상자와 `ExitProcess(9)` (작업 359) / Confirmed — the Hardlock error box and `ExitProcess(9)` (Task 359)

2026-09-24, 작업 359에서 `user32` facade에 `MessageBoxA`를 추가했다. 그 결과 정적 IAT slot이 facade thunk로 재결합되었다. 실제 4th CHD(읽기 전용)의 in-process 실행은 두 host 폭에서 결과가 같다. stack 위치에 따라 달라지는 포인터만 다르다.

| # | 호출 / Call | caller 복귀 / Caller return | 결과 / Result |
| --- | --- | --- | --- |
| 0019 | `MessageBoxA(0, text, caption, 0x2010)` | `0x00ae4d9c` | `IDOK`(1) |
| 0020 | `GetProcAddress(0x6f000000, "ExitProcess")` | `0x00aeed26` | `0x6f00204c` |
| 0021 | `ExitProcess(9)` | `0x00aeeba2` | 게스트 프로세스 종료 → `kProcessExit` / guest process exit |

* **확인됨.** `#0019`의 문구는 `"Error 1009 : Cannot open Hardlock driver.\r\n"`, 제목은 `"Hardlock  "`(끝에 공백 두 개)이다. `uType` `0x2010`은 `MB_TASKMODAL | MB_ICONHAND`이고, 버튼 종류는 `MB_OK`다. 소유 창 인자는 0이다. 문구 포인터는 게스트 stack에 있고, 제목은 image 안(`0x00ae72f0`)에 있다.
  ***Confirmed.** `#0019` shows text `"Error 1009 : Cannot open Hardlock driver.\r\n"` with caption `"Hardlock  "` (two trailing spaces); `uType` `0x2010` is `MB_TASKMODAL | MB_ICONHAND` with the `MB_OK` button set, and the owner argument is 0. The text pointer is on the guest stack and the caption is inside the image (`0x00ae72f0`).*
* **확인됨.** 대화상자 뒤 게스트는 `ExitProcess`를 **다시** 해석하고(`#0020`, caller `0x00aeed26`) 종료 코드 9로 호출한다. `#0016`에서 얻은 주소를 쓰지 않았다. in-process 실행은 이 호출에서 정상 종료로 끝난다.
  ***Confirmed.** After the box the guest resolves `ExitProcess` **again** (`#0020`, caller `0x00aeed26`) rather than using the `#0016` address, and calls it with exit code 9; the in-process run ends there as a normal exit.*
* **확인됨.** 같은 문구와 제목은 Windows host에서 3rd를 실행했을 때 나온 Hardlock 대화상자와 같다([EXE 구조 분석](ez2dj-exe-structures.md), [작업 101](../design/20260830-101-ez2dj3rd-hardlock-execution.md)).
  ***Confirmed.** The same text and caption match the Hardlock dialog seen when running 3rd on the Windows host ([EXE structures](ez2dj-exe-structures.md), [Task 101](../design/20260830-101-ez2dj3rd-hardlock-execution.md)).*

**추정.** `\.\NTICE`와 `\.\FEnteDev` 열기가 모두 `INVALID_HANDLE_VALUE`를 받았으므로, 보호 stub이 Hardlock driver가 없다고 판단하고 Error 1009 경로로 종료한 것으로 본다. `DDRAW.DLL` 조회는 이 판단과 따로 진행된 것으로 보인다.

**미확정.** Linux에서 이 경계를 넘으려면 Hardlock 장치 HLE가 필요하다. 이는 Windows 작업 127의 Function `0x0e` 변환과 같은 과제로 보이며, 두 장치 열기와 이후 `DeviceIoControl` 경계를 공용 HLE로 옮기는 방식은 아직 설계하지 않았다.

*Inferred: with both `\.\NTICE` and `\.\FEnteDev` opens returning `INVALID_HANDLE_VALUE`, the protection stub concludes no Hardlock driver exists and exits through Error 1009; the `DDRAW.DLL` lookup looks independent of that decision. Unresolved: getting past this boundary on Linux needs a Hardlock device HLE, which appears to be the same problem as the Windows Task 127 Function `0x0e` transform; moving the two device opens and the following `DeviceIoControl` boundary into shared HLE has not been designed.*

## 확인됨 — Linux에서 `\.\FEnteDev` 열기와 Hardlock initialize (작업 361) / Confirmed — `\.\FEnteDev` open and Hardlock initialize on Linux (Task 361)

2026-09-24, 작업 361에서 공용 `GuestDeviceSet`과 `kernel32`의 `CreateFileA`(장치)·`DeviceIoControl`·`CloseHandle`·last error를 Linux facade에 연결했다. 사용자 `cfg/hardlock.ini`의 재료를 적용하고(값은 기록하지 않음) 실제 4th CHD(읽기 전용)를 실행했다. 결과는 x86과 x64가 같다. stack 위치에 따른 포인터만 다르다.

| # | 호출 / Call | 결과 / Result |
| --- | --- | --- |
| 0006, 0010 | `CreateFileA("\.\NTICE")` | `INVALID_HANDLE_VALUE`, last error 123 |
| 0014 | `CreateFileA("\.\FEnteDev")` | handle `0x00001004` |
| 0015 | `GetProcAddress(kernel32, "CloseHandle")` | `0x6f002072` |
| 0016 | `GetProcAddress(kernel32, "DeviceIoControl")` | `0x6f00205f` |
| 0017 | `DeviceIoControl(0x1004, 0x9c402468, 0, 0, 0, 0, &returned, 0)` | `TRUE`, 0 byte (initialize) |
| 0018 | `CloseHandle(0x1004)` | `TRUE` |
| 0019 | `GetProcAddress(user32, "GetActiveWindow")` | `0x6eff2000` |
| 0020 | `GetProcAddress(user32, "CreateCursor")` | 0 → 미해석 lookup으로 정지 / unresolved → stop |

* **확인됨.** 보호 코드는 `CloseHandle`과 `DeviceIoControl`을 동적으로 해석한다. Hardlock 장치는 한 번 열어 initialize 한 번을 보내고, 바로 닫는다.
  ***Confirmed.** The protection resolves `CloseHandle` and `DeviceIoControl` dynamically, opens the Hardlock device once, sends one initialize, and closes it.*
* **확인됨.** 이번 실행에서는 initialize 뒤, 다음 경계(`CreateCursor` 해석) 전까지 `wtsapi32` 조회가 없었다. Windows 분석에서는 initialize 뒤에 WTS 세션 조회와 handshake `0x450`이 이어진다([Hardlock runtime 분석](ez2dj4th-hardlock-runtime.md)).
  ***Confirmed.** This run made no `wtsapi32` query between the initialize and the next boundary (the `CreateCursor` lookup), while the Windows analysis follows the initialize with a WTS session query and handshake `0x450`.*

**추정.** `CreateCursor`는 user32의 커서 생성 함수다. 해석 대상 목록이 Windows와 같다면, WTS 조회와 handshake는 이 해석 뒤에 오는 것으로 보인다.

**미확정.** Windows에서 initialize와 WTS 조회 사이에 같은 `CreateCursor` 해석이 있는지는 Windows 기록(Hardlock 요청만 남음)으로 확인할 수 없다. `CreateCursor`를 제공한 뒤의 경계로 확인한다.

*Inferred: `CreateCursor` is user32's cursor-creation function; if the resolution order matches Windows, the WTS query and handshake come after it. Unresolved: whether Windows performs the same `CreateCursor` resolution between the initialize and the WTS query cannot be seen in the Windows record, which keeps only Hardlock requests; the boundary after providing `CreateCursor` will tell.*

## 확인됨 — Hardlock API 시작 환경과 handshake (작업 363) / Confirmed — Hardlock API startup environment and handshakes (Task 363)

2026-09-24, 작업 363에서 `GuestProcess`, 없는 이름 선언, `kernel32`·`user32` export, `advapi32`·`wtsapi32` facade를 더했다. 그 뒤 실제 4th CHD(읽기 전용)를 실행했다. x64와 x86은 같은 59개 호출을 같은 순서로 부르고, stack·heap 주소만 다르다. 사용자 `cfg/hardlock.ini`를 적용했고 값은 기록하지 않았다.

*On 2026-09-24 Task 363 added `GuestProcess`, declared absent names, `kernel32`/`user32` exports, and the `advapi32` and `wtsapi32` facades, then ran the real 4th CHD (read-only). x64 and x86 make the same 59 calls in the same order, differing only in stack and heap addresses; the user's `cfg/hardlock.ini` was applied and no values were recorded.*

| # | 호출 / Call | 결과 / Result |
| --- | --- | --- |
| 0020–0024 | `GetProcAddress`: `CreateCursor`, `DestroyCursor`, `SetCursor`, `GetCurrentProcess`, `GetTickCount` | 주소, 호출 없음 / addresses, not called |
| 0025 | `GetCurrentProcessId()` | `0x00000F00` |
| 0026 | `GetEnvironmentVariableA("HL_SEARCH", …, 88)` | 0, last error 203 |
| 0027–0028 | `SetErrorMode(0x8000)` 두 번 / twice | 0, 그다음 / then `0x8000` |
| 0030–0033 | `LoadLibraryA("advapi32.dll")`, `RegOpenKeyA`·`RegQueryValueExA`·`RegCloseKey` 해석 / resolved | 호출 없음 / not called |
| 0036–0037 | `GetVersionExA`(156 byte) | TRUE, 6.2.9200 NT |
| 0038–0042 | `LoadLibraryA("wtsapi32.dll")`, `WTSQuerySessionInformationA(0, -1, 4, …)`, `WTSFreeMemory` | TRUE, 세션 / session 0, 해제 / freed |
| 0043 | `LoadLibraryA("wfapi.dll")` | NULL (없는 module / absent) |
| 0044 | `SetErrorMode(0)` | `0x8000` |
| 0046–0049 | `LoadLibraryA("kernel32.dll")`, `GetProcAddress("IsTNT")`, `GetProcAddress("Borland32")`, `FreeLibrary` | base, NULL, NULL(없는 이름 / absent), TRUE |
| 0050 | `CreateFileA("\.\FEnteDev")` | handle `0x00001008` |
| 0053, 0055 | `DeviceIoControl(0x9c402450, 6 byte in-place)` | handshake 두 번 / two handshakes |
| 0056 | `DeviceIoControl(0x9c40244c, 256 byte in-place)` | descriptor |
| 0058–0059 | `GetProcAddress`: `GetCurrentProcessId`, `OpenProcess` | 주소, NULL → 정지 / address, NULL → stop |

```text
hardlock requests: total=4 initialize=1 handshake=2 descriptor=1 transform=0 other=0 rejected=0 last=descriptor/completed
continuation    : stopped at unresolved lookup GetProcAddress(6f000000, OpenProcess), return 0x00ae2718
```

* **확인됨.** 작업 361이 "다음 경계" 앞에 두었던 WTS 조회가 여기서 나타났다. 게스트는 `wtsapi32.dll`을 `LoadLibraryA`로 올리고, `WTSSessionId`(4)를 현재 세션에 묻는다. 세션 0을 받고 buffer를 `WTSFreeMemory`로 돌려준 뒤 handshake로 진행한다. Windows의 진행 조건(세션 0)과 같다.
  ***Confirmed.** The WTS query Task 361 expected past its boundary appears here: the guest loads `wtsapi32.dll` with `LoadLibraryA`, asks the current session for `WTSSessionId` (4), receives session 0, returns the buffer through `WTSFreeMemory`, and proceeds to the handshakes, matching the Windows advance condition (session 0).*
* **확인됨.** `HL_SEARCH`, advapi32 레지스트리 함수, `wfapi.dll`, `IsTNT`·`Borland32`는 Hardlock API가 검색 설정, Citrix, DOS extender를 확인하는 단계로 보인다(목적은 **추정**). 레지스트리 함수는 해석만 되고 호출되지 않았다.
  ***Confirmed.** `HL_SEARCH`, the advapi32 registry functions, `wfapi.dll`, and `IsTNT`/`Borland32` appear to be the Hardlock API checking its search settings, Citrix, and DOS extenders (purpose **inferred**); the registry functions are resolved but not called.*
* **확인됨(작업 363 설계의 실험).** `OpenProcess` 뒤에는 `VirtualProtect`, `ReadProcessMemory`, `WriteProcessMemory`, `VirtualAlloc`, `VirtualFree`를 해석한다. 이것이 없으면 `"Error 1003 : Internal Error."` 뒤 `ExitProcess(3)`로 끝난다. 자기 image를 고치는 단계로 보인다(**추정**).
  ***Confirmed (Task 363 design experiment).** After `OpenProcess` the guest resolves `VirtualProtect`, `ReadProcessMemory`, `WriteProcessMemory`, `VirtualAlloc`, and `VirtualFree`; without them it ends with `"Error 1003 : Internal Error."` and `ExitProcess(3)`. This looks like the stage that modifies its own image (**inferred**).*

## 확인됨 — image 복호화 루프 완료 (작업 364) / Confirmed — image decryption loop completes (Task 364)

2026-09-25, 작업 364에서 자기 process 메모리 API를 더한 뒤 실제 4th CHD(읽기 전용)를 실행했다. 두 폭 모두 1,390번 호출하고, 주소를 정규화하면 호출 기록이 같다.

*On 2026-09-25, after Task 364 added the own-process memory APIs, the real 4th CHD (read-only) made 1,390 calls on both widths, with identical records after address normalization.*

| # | 호출 / Call | 결과 / Result |
| --- | --- | --- |
| 0061 | `OpenProcess(0x38, FALSE, 0x0F00)` | handle `0x100c` |
| 0067 | `VirtualAlloc(NULL, 0x8000, MEM_COMMIT, PAGE_READWRITE)` | arena 안 64 KiB 경계 / on a 64 KiB boundary in the arena |
| 0068–1379 | page마다 `VirtualProtect(page, …, PAGE_READWRITE)` → 수정 → 원래 값으로 복원. 사이사이 descriptor `0x44c`, `LocalAlloc(0, 0x108)`, transform `0x458`, `LocalFree` / per page: `VirtualProtect(…, PAGE_READWRITE)` → modify → restore, with descriptor, `LocalAlloc`, transform, and `LocalFree` in between | `.text`는 `0x20`, `.rdata`는 `0x02`, 쓰기 섹션은 `0x08`로 복원 / `.text` restored to `0x20`, `.rdata` to `0x02`, writable sections to `0x08` |
| 1380 | `VirtualFree(block, 0x8000, MEM_DECOMMIT)` | TRUE |
| 1384 | `OpenProcess(0x38, FALSE, 0x0F00)` (두 번째 층 / second layer) | handle `0x1010` |
| 1385–1390 | `GetProcAddress`: `VirtualProtect`, `VirtualAlloc`, `VirtualFree`, `ReadProcessMemory`, `WriteProcessMemory`, `GetCurrentThreadId` | 마지막이 NULL → 정지 / the last is NULL → stop |

```text
hardlock requests: total=76 initialize=1 handshake=2 descriptor=37 transform=36 other=0 rejected=0 last=transform/completed
continuation    : stopped at unresolved lookup GetProcAddress(6f000000, GetCurrentThreadId), return 0x00ae9440
```

* **확인됨.** Hardlock 요청 수는 Windows 기록(initialize 1, handshake 2, descriptor 37, Function `0x0e` transform 36)과 같다. 공용 Hardlock HLE와 사용자 재료로 Linux에서도 image 복호화 루프가 끝까지 돈다.
  ***Confirmed.** The Hardlock request counts equal the Windows record, so with the shared Hardlock HLE and the user's material the image decryption loop also runs to the end on Linux.*
* **확인됨.** 보호 코드는 `.text`부터 `.reloc` 끝까지 page마다 쓰기를 열고, 받은 이전 값으로 되돌린다. `.protect` 섹션은 건드리지 않는다. 각 섹션을 두 번씩 지난다.
  ***Confirmed.** The protection opens every page from `.text` to the end of `.reloc` for writing and restores the previous value it received, never touching `.protect`, passing each section twice.*
* **미확정.** 복호화 결과가 Windows와 byte 단위로 같은지는 아직 비교하지 않았다. transform 수가 같다는 것까지만 확인했다.
  ***Unresolved.** Whether the decrypted bytes match Windows byte for byte is not yet compared; only the transform count is confirmed.*

## 확인됨 — envelope 종료와 원본 CRT 진입 (작업 367) / Confirmed — envelope end and entry into the original CRT (Task 367)

2026-09-25, 작업 367 뒤 실제 4th CHD(읽기 전용)를 두 폭에서 실행했다. 1,579번 호출하고, 주소를 정규화하면 두 폭의 기록이 같다.

*On 2026-09-25, after Task 367, the real 4th CHD (read-only) made 1,579 calls on both widths, identical after address normalization.*

| # | 호출 / Call | 결과 / Result |
| --- | --- | --- |
| 1390–1397 | `GetProcAddress`: `GetCurrentThreadId`, `Sleep`, `GetTickCount`, `ExitProcess`, `SetTimer`, `KillTimer`, `GetModuleHandleA`, `LoadLibraryA` | 주소 / addresses |
| 1398–1399 | `VirtualAlloc(NULL, 0x284, MEM_COMMIT, RW)`, `VirtualAlloc(NULL, 0x508, MEM_COMMIT, RWX)` | 64 KiB 경계 / 64 KiB boundaries |
| 1400–1570 | `.idata` descriptor마다 `GetModuleHandleA` + `GetProcAddress`(ordinal 포함) / per `.idata` descriptor, including ordinals | 10개 DLL 모두 주소 / all ten DLLs resolve |
| 1571–1572 | `GetVersion` | `0x23f00206` |
| 1573–1576 | `VirtualProtect(0x6f00204c, 5, RW)` → `PAGE_EXECUTE_READ` 복원, 두 번 / restored, twice | `0x6f00204c` = facade `ExitProcess` thunk |
| 1577 | `SetTimer(NULL, 0, 0x8000, 0x00aeaddb)` | timer ID 1 |
| 1578 | `GetVersion` (호출 위치 / from `0x004c4424`, `.text`) | 원본 CRT / original CRT |
| 1579 | `HeapCreate(HEAP_NO_SERIALIZE, 0x1000, 0)` | 해석 전용 → 정지 / resolve-only → stop |

* **확인됨.** envelope는 원본 `.idata`의 descriptor 표를 따라 import를 다시 만든다. 이미 처리한 kernel32·user32 descriptor의 이름 표는 dump 시점에 0으로 지워져 있었다.
  ***Confirmed.** The envelope rebuilds imports by following the original `.idata` descriptor table; at dump time the name tables of the processed kernel32 and user32 descriptors were already zeroed.*
* **확인됨.** envelope는 facade의 `ExitProcess` thunk 첫 5 byte 범위의 보호를 바꾸고 되돌린다. thunk의 첫 5 byte는 `push imm32`(gate 번호)라서 다른 곳으로 옮겨도 그대로 동작한다. 그 사이에 hook을 쓴 것으로 **추정**한다. 실제로 쓴 byte는 확인하지 않았다.
  ***Confirmed.** The envelope changes and restores the protection over the first five bytes of the facade's `ExitProcess` thunk, which are a relocatable `push imm32` (the gate number); writing a hook in between is **inferred**, as the written bytes were not examined.*
* **확인됨.** `0x004c4424`는 `.text`(`0x401000`–`0x4dc022`) 안에 있다. `GetVersion` 다음 `HeapCreate`로 이어지는 흐름은 MSVC CRT 시작 순서다. 따라서 여기부터 원본 프로그램이다.
  ***Confirmed.** `0x004c4424` lies in `.text` (`0x401000`–`0x4dc022`), and `GetVersion` followed by `HeapCreate` is the MSVC CRT start-up order, so the original program runs from here.*

## 확인됨 — 원본 CRT 시작 완료 (작업 368) / Confirmed — original CRT start-up completes (Task 368)

2026-09-25, 작업 368 뒤 실제 4th CHD(읽기 전용)는 두 폭에서 1,623번 호출하고, 주소를 정규화하면 기록이 같다. `#1578 GetVersion`부터 `#1622 SetUnhandledExceptionFilter`까지가 MSVC CRT 시작이다. 순서는 [작업 368 설계](../design/20260925-368-original-crt-startup.md)에 있다. 이어 게임 코드 `0x00406fe3`에서 `CreateEventA(NULL, FALSE, FALSE, NULL)`을 부른다.

*On 2026-09-25, after Task 368, the real 4th CHD (read-only) makes 1,623 calls on both widths, identical after address normalization. `#1578 GetVersion` through `#1622 SetUnhandledExceptionFilter` is the MSVC CRT start-up, in the order the [Task 368 design](../design/20260925-368-original-crt-startup.md) lists; game code at `0x00406fe3` then calls `CreateEventA(NULL, FALSE, FALSE, NULL)`.*

* **확인됨.** CRT는 코드 페이지 949를 쓴다. `GetACP` 결과로 `GetCPInfo`, `MultiByteToWideChar`, `GetStringTypeW`, `LCMapStringW`(locale `0x412`, 한국어)를 부른다.
  ***Confirmed.** The CRT works in code page 949, calling `GetCPInfo`, `MultiByteToWideChar`, `GetStringTypeW`, and `LCMapStringW` (locale `0x412`, Korean) with `GetACP`'s result.*
* **확인됨.** CRT는 `GetProcAddress`로 `IsProcessorFeaturePresent`를 찾는다. 그 호출 위치(`0x00aebbc4`)는 `.protect` 안이다. envelope가 원본의 `GetProcAddress` import도 자기 코드로 거치게 한 것으로 **추정**한다.
  ***Confirmed.** The CRT looks up `IsProcessorFeaturePresent` through `GetProcAddress`, from a site (`0x00aebbc4`) inside `.protect`; the envelope appears to route the original's `GetProcAddress` import through its own code (**inferred**).*

## 확인됨 — 게임의 Hardlock 로그인과 WinMain 진입 (작업 369) / Confirmed — the game's Hardlock login and WinMain entry (Task 369)

2026-09-25, 작업 369 뒤 실제 4th CHD(읽기 전용)는 두 폭에서 1,700번 호출하고, 주소를 정규화하면 기록이 같다.

*On 2026-09-25, after Task 369, the real 4th CHD (read-only) makes 1,700 calls on both widths, identical after address normalization.*

| # | 호출 / Call | 비고 / Note |
| --- | --- | --- |
| 1623–1652 | `CreateEventA`, `VirtualAlloc`(8 MiB, 256 KiB, 1 MiB), `HeapSize` 반복 / repeated | C++ 정적 초기화 / static initialization |
| 1653–1684 | `GetCurrentProcessId` … `CreateFileA("\.\FEnteDev")`, handshake 2, descriptor 1 | 게임 `.text`(`0x004b…`)의 Hardlock API / the Hardlock API in the game's `.text` |
| 1685–1690 | `GetLocalTime`, `GetSystemTime`, `GetTimeZoneInformation`, `WideCharToMultiByte` × 2 | CRT 시간대 / CRT time zone |
| 1696 | `CreateEventA(…, TRUE, …)` | |
| 1698–1699 | `GetStartupInfoA`, `GetModuleHandleA(NULL)` → `0x00400000` | WinMain 인자 준비 / WinMain arguments |
| 1700 | `timeBeginPeriod(1)` (`0x00406cdc`) | WinMain의 첫 호출 → 정지 / WinMain's first call → stop |

* **확인됨.** 게임 코드에도 Hardlock API가 link되어 있다. envelope와 같은 순서로 다시 로그인하고, handshake 2회와 descriptor 1회를 더 보낸다(합계 handshake 4, descriptor 38).
  ***Confirmed.** The game code links the Hardlock API as well and logs in again in the envelope's order, adding two handshakes and one descriptor (four handshakes and 38 descriptors in all).*
* **미확정.** Windows 기록의 합계(작업 364 비교: descriptor 37)는 게임 로그인 전에 끝난 실행이다. 게임 로그인까지 포함한 Windows 합계와는 아직 비교하지 않았다.
  ***Unresolved.** The Windows record's totals (compared in Task 364: 37 descriptors) come from a run that ended before the game's login; totals including that login are not yet compared on Windows.*
