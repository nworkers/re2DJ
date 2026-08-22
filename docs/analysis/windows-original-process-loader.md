# Windows Original Process Loader Observation

## 한국어

**확인됨 — 2026-08-22, 실제 `roms\ez2dj1stse` 입력.** `ez2dj1stse_unpacked` target의 `ez2dj1.exe`를 x64 host가 `CREATE_SUSPENDED`로 생성한 뒤, WOW64 PEB의 image-base field를 읽었다. 값은 정적으로 파싱한 PE32 image base와 같은 `0x00400000`이었다. child의 primary thread는 resume하지 않았고, 확인 뒤 `TerminateProcess`로 종료했다.

**확인됨.** 같은 파일을 별도 Win32 helper에서 수동 `VirtualAlloc` mapping하면 `0x00400000`은 `C_949.NLS` mapped view와 충돌한다. 따라서 원본 EXE를 Windows loader의 **주 이미지**로 생성하는 방식은 수동 mapper의 fixed-base 충돌을 피하는 실행 후보이다.

**확인됨 — 2026-08-22, 실제 `roms\ez2dj1stse` 입력.** `CREATE_SUSPENDED` 직후 첫 `KERNEL32.dll` IAT slot(`RVA 0x01aba354`)에는 원본 image 내부 `0x01aba6ea`가 남아 있었다. loader-resolved 외부 함수 주소가 아니므로 suspended 시점은 IAT patch 경계가 아니다.

**확인됨 — 2026-08-22, 실제 `roms\ez2dj1stse` 입력.** `DEBUG_ONLY_THIS_PROCESS`의 첫 `EXCEPTION_BREAKPOINT`에서도 같은 IAT slot이 원본 image 내부 value를 유지했다. 이 event 역시 IAT patch 경계가 아니다.

**확인됨 — 2026-08-22, 실제 `roms\ez2dj1stse` 입력.** primary WOW64 thread의 entry `0x0043A640` hardware breakpoint는 entry 직전 `0x4000001E` single-step으로 정지했고, IAT 144 slot·7 DLL은 모두 loader-resolved 외부 주소였다. 이 지점은 IAT patch 후보이다.

**확인됨 — 2026-08-22, 실제 `roms\ez2dj1stse` 입력.** Win32 x86 launcher가 `DEBUG_ONLY_THIS_PROCESS`로 원본을 생성한 뒤 entry `0x0043A640`의 첫 바이트를 child memory에서만 일시적으로 `INT3`로 교체했다. `EXCEPTION_BREAKPOINT`에서 즉시 원래 바이트를 복원한 뒤 검사한 결과, 주 이미지 기준 주소는 `0x00400000`이고 IAT 144 slot·7 DLL은 모두 loader-resolved 외부 주소였다. 원본 entry는 실행하지 않고 child를 종료했다.

**미확정.** 같은 x86 launcher에서 DR0 hardware breakpoint는 설정 직후 thread context에 유지됐지만 entry single-step을 받지 못했고, 원본 child는 loader event 뒤 exit code `-1`로 종료했다. 이 현상이 debug-register delivery, 원본의 초기화 경로, 또는 다른 조건 중 무엇 때문인지는 아직 확인하지 않았다. injected x86 runtime의 통신과 IAT patch는 아직 검증하지 않았다.

**확인됨 — 2026-08-22, 실제 `roms\ez2dj1stse` 입력.** entry `INT3` 정지에서 primary thread를 suspend하고 debug event를 계속한 뒤, 같은 x86 launcher의 `kernel32!LoadLibraryW`를 remote thread start address로 사용해 최소 runtime DLL을 child에 적재했다. loader thread는 module base `0x7c130000`을 반환했고, primary thread는 suspend 상태이므로 original entry는 실행되지 않았다. 이 주소 사용은 이 same-bitness 입력의 실험 결과일 뿐 일반 정책으로 확정하지 않는다.

## English

**Confirmed — 2026-08-22, live `roms\ez2dj1stse` input.** The x64 host created `ez2dj1.exe` for target `ez2dj1stse_unpacked` with `CREATE_SUSPENDED`, then read the image-base field from its WOW64 PEB. The value was `0x00400000`, matching the statically parsed PE32 image base. The child's primary thread was never resumed and was terminated after verification.

**Confirmed.** When the same file is manually mapped with `VirtualAlloc` in a separate Win32 helper, `0x00400000` conflicts with a mapped `C_949.NLS` view. Creating the original EXE as the Windows loader's **main image** is therefore an execution candidate that avoids the manual mapper's fixed-base conflict.

**Confirmed — 2026-08-22, live `roms\ez2dj1stse` input.** Immediately after `CREATE_SUSPENDED`, the first `KERNEL32.dll` IAT slot (`RVA 0x01aba354`) retained internal original-image value `0x01aba6ea`. It is not a loader-resolved external function address, so the suspended point is not an IAT-patch boundary.

**Confirmed — 2026-08-22, live `roms\ez2dj1stse` input.** At the first `EXCEPTION_BREAKPOINT` under `DEBUG_ONLY_THIS_PROCESS`, the same IAT slot retained its internal original-image value. This event is also not an IAT-patch boundary.

**Confirmed — 2026-08-22, live `roms\ez2dj1stse` input.** A hardware breakpoint at primary WOW64-thread entry `0x0043A640` stopped with single-step `0x4000001E` immediately before entry, and all 144 IAT slots across seven DLLs were loader-resolved external addresses. This is an IAT-patch candidate.

**Confirmed — 2026-08-22, live `roms\ez2dj1stse` input.** A Win32 x86 launcher created the original under `DEBUG_ONLY_THIS_PROCESS`, then temporarily replaced only entry `0x0043A640`'s first byte in child memory with `INT3`. At `EXCEPTION_BREAKPOINT`, it restored the original byte immediately and verified a `0x00400000` main-image base plus loader-resolved external addresses for all 144 IAT slots across seven DLLs. The child was terminated without executing original entry.

**Unresolved.** In the same x86 launcher, a DR0 hardware breakpoint was retained when read back from the thread context but never delivered an entry single-step; the original child exited with code `-1` after loader events. It is not yet known whether this is debug-register delivery, the original's initialization path, or another condition. x86 runtime communication and IAT patching remain unverified.

**Confirmed — 2026-08-22, live `roms\ez2dj1stse` input.** At the entry `INT3` stop, the launcher suspended the primary thread, continued the debug event, and used the same x86 launcher's `kernel32!LoadLibraryW` as a remote-thread start address to load the minimal runtime DLL into the child. The loader thread returned module base `0x7c130000`; because the primary thread remained suspended, original entry did not execute. This address use is experimental evidence for this same-bitness input, not an established general policy.

**Confirmed — 2026-08-22, live `roms\ez2dj1stse` input with explicit execution approval.** The launcher injected the x86 runtime, replaced the resolved `GetCommandLineA` IAT slot with the runtime log-and-forward thunk, resumed original entry, and received the expected debugger output event. The runtime module base for this run was `0x7c140000`. The launcher terminated the child immediately after the event; this confirms one original import can cross the runtime handoff and tail-jump back to the native API.

**Confirmed — 2026-08-22, live `roms\ez2dj1stse` input with explicit execution approval.** By user policy, the launcher wrote `ez2dj1.exe` into the runtime's exported command-line buffer, then replaced the original `GetCommandLineA` IAT slot with the HLE thunk rather than the forwarding thunk. After entry resumed, the expected HLE debugger output event was received. This run loaded the runtime at `0x7c150000` and terminated the child immediately after the event.

**Confirmed — 2026-08-22, live `roms\ez2dj1stse` input with explicit execution approval.** The `GetWindowsDirectoryA` IAT slot was replaced with an HLE thunk that returns the absolute `windows` support path beside `re2dj.exe`, independent of CWD. Entry resumed and produced the expected HLE debugger output event; runtime base was `0x7c160000`.
