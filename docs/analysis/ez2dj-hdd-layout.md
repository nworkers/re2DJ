# EZ2DJ HDD 레이아웃과 실행 파일 식별 / EZ2DJ HDD Layout and Executable Identification

주제: 사용자가 제공한 HDD 덤프의 디렉터리 구조, 어떤 파일이 게임 실행 파일인지, 그 실행 파일의 PE 특성.

*Topic: the directory structure of a user-supplied HDD dump, which file is the game executable, and that executable's PE characteristics.*

측정 대상 덤프 두 개:

| 식별자 | 내용 | 파일 수 |
| --- | --- | --- |
| 1st SE | EZ2DJ 1st Trax Special Edition | 245 디렉터리 / 16,613 파일 |
| 3rd | EZ2DJ 3rd Trax | 199 디렉터리 / 25,054 파일 |

측정 방법: `re2dj_hdd_probe <dir>`, `re2dj_pe_analyzer <file>`, 그리고 import 테이블 확인용 일회성 스크립트.

*Two dumps were measured with `re2dj_hdd_probe`, `re2dj_pe_analyzer`, and a one-off script for the import tables.*

---

## 1. 확인됨: 식별 도구의 정확성 / Confirmed: the tooling is correct

`re2dj_pe_analyzer`를 `C:\Windows\SysWOW64\notepad.exe`에 실행해 Microsoft `dumpbin /headers`와 대조했다. machine, magic, image base, entry point, size of image, section alignment, file alignment, 섹션 수, subsystem이 모두 일치했다.

*Verified `re2dj_pe_analyzer` against Microsoft `dumpbin /headers` on a real 32-bit PE32 GUI executable; every compared field matched.*

두 덤프 모두 `truncated : no`로 완주했고, 발견한 실행 파일 다섯 개 전부 PE 헤더를 읽어냈다.

*Both dumps were walked to completion and all five executables found had readable PE headers.*

---

## 2. 확인됨: 1st SE 덤프 구조 / Confirmed: 1st SE dump layout

```text
<root>/
├── ez2dj.exe          561,152 B  2000-01-01  보호됨 / protected
├── ez2dj1.exe         524,288 B  1999-12-24  보호되지 않음 / not protected
├── Test.exe         1,859,633 B  1999-12-22  서비스·테스트 도구 / service tool
├── PlzPowerOff.exe     98,304 B  1999-03-30  종료 화면 / shutdown screen
├── Tdsd.vxd111         30,278 B  2000-01-19  Windows 9x VxD
├── ez2dj.ini            4,142 B             난이도·모드·곡 목록 / difficulty, modes, song lists
├── System.ini           1,322 B
├── rank_0.dat / rank_1.dat / rank_2.dat  각 400 B  랭킹 저장 / ranking storage
├── Songs/             68개 디렉터리 / 68 directories
└── System/            화면 단위 자산 / per-screen assets
    ├── Title/  MusicSelect/  Result/  Ranking/  NameEntry/
    ├── ClubMix/  RadioMix 계열/  LevelSelect/  ChannelSelect/
    └── Opening/  Demonstration/  EyeCatch/  GameOver/  ending/ ...
```

`Songs/` 아래 디렉터리 이름이 `ez2dj.ini`의 `[BATTLEMODE]`, `[ClubMix]`, `[RADIOMIX]` 항목에 나오는 곡 식별자와 일치한다. 즉 **곡 선택과 모드 구성이 INI로 외부화되어 있다.**

*The directory names under `Songs/` match the song identifiers listed in the `[BATTLEMODE]`, `[ClubMix]`, and `[RADIOMIX]` sections of `ez2dj.ini`, so **song selection and mode composition are externalised into INI files.***

## 확인됨: 3rd 덤프 구조 / Confirmed: 3rd dump layout

```text
<root>/
├── EZ2DJ.EXE        1,216,512 B  2001-09-24  보호됨 / protected
├── EZ2DJ.INI
├── FONTEN.DAT / FONTKR.DAT      영문·한글 폰트 / English and Korean fonts
├── cache.reg / cache.txt
├── BG/            배경 영상·이미지 / background media
├── Sound/
└── system/
```

1st SE와 3rd는 디렉터리 구조가 서로 다르다. **타깃 프로파일을 버전별로 분리한 설계가 실제로 필요하다는 것이 확인되었다.**

*The 1st SE and 3rd layouts differ, which **confirms that per-version target profiles are actually needed** rather than merely anticipated.*

---

## 3. 확인됨: 실행 파일 PE 특성 / Confirmed: executable PE characteristics

모든 실행 파일이 `PE32 / i386 / Windows GUI / ImageBase 0x00400000`이다.

*Every executable is `PE32 / i386 / Windows GUI` based at `0x00400000`.*

| 파일 | 진입점 RVA | 진입점이 놓인 섹션 | SizeOfImage | 보호 |
| --- | --- | --- | --- | --- |
| `ez2dj1.exe` (1st SE) | `0x0003a640` | `.text` | `0x01ad1000` | **없음** |
| `ez2dj.exe` (1st SE) | `0x01ad23cf` | `.gtide` | `0x01ada000` | **있음** |
| `EZ2DJ.EXE` (3rd) | `0x00642240` | `.protect` | `0x0067c000` | **있음** |
| `Test.exe` (1st SE) | `0x0001ada0` | `.text` | — | 없음 |
| `PlzPowerOff.exe` (1st SE) | `0x00001e6e` | `.text` | — | 없음 |

### 확인됨: ez2dj1.exe는 선호 주소에 고정되어 있다

`ez2dj1.exe`에는 이름이 `.reloc`인 섹션이 있지만 optional header의 base relocation data directory는 `{RVA 0, Size 0}`이다. Stage 2 로더로 선호 주소 `0x00400000` 적재는 성공하고 다른 주소 적재는 재배치 정보 부재로 거부된다. 따라서 이 bring-up 빌드는 현재 확인된 형태 그대로라면 선호 주소에 고정해서 적재해야 한다.

*Confirmed: `ez2dj1.exe` has a section named `.reloc`, but its optional-header base-relocation data directory is `{RVA 0, Size 0}`. The Stage 2 loader maps it successfully at preferred base `0x00400000` and rejects a different base because no relocation records are advertised. This bring-up build must therefore be loaded at its preferred base in the form inspected.*

### 확인됨: ez2dj.exe와 EZ2DJ.EXE는 보호되어 있다

`ez2dj.exe`의 섹션은 `.text .rdata .data .idata .reloc` 뒤에 **`.gtide` `.gdata` `.gidata`** 세 개가 더 붙어 있고, 진입점이 마지막 코드 섹션 `.gtide` 안에 있다. import 디렉터리도 `.gidata`(`0x01ad8000`)로 옮겨져 있다.

`EZ2DJ.EXE`(3rd)는 `.protect` 섹션에 진입점이 있고, 이름 자체가 목적을 말한다.

*The protected builds carry extra sections after the normal five, hold their entry point in the last of them, and relocate the import directory into a packer-owned section. The 3rd's section is literally named `.protect`.*

**확인됨 — 2026-08-23.** canonical `ez2dj.exe`는 software `INT3` breakpoint에서 static entry VA `0x01ed23cf`에 도달한다. 같은 정지점에서 x86 runtime DLL injection도 성공했다. 반면 hardware entry breakpoint는 전달되지 않았으므로, hardware trace만으로 entry 이전 종료를 결론 낼 수 없다.

**확인됨 — 2026-08-23.** debugger 없이 같은 working directory에서 `ez2dj.exe`를 직접 실행하면 10초 안에 `0x001affcc`로 종료한다. hardware breakpoint trace에서는 `umpdc.dll`, `wsock32.dll`, `ws2_32.dll` 적재, Winsock DLL 두 개의 해제, 세 thread의 code 0 종료, process exit `0x001affcc` 순서가 보였다. software breakpoint가 entry에 도달하므로 이 동적 DLL 흐름과 종료는 entry 이후에 일어난다. `0x001affcc`는 이 host의 Win32 error message로 해석되지 않았다.

**확인됨 — 2026-08-23.** static entry에서 runtime을 주입하고 `ExitProcess` IAT slot을 observation wrapper로 바꾼 뒤 10초 동안 재개했을 때, slot은 wrapper 주소를 유지했고 `ExitProcess` wrapper 또는 process exit event는 오지 않았다. 이 runtime-injected run은 raw run과 다른 timing 또는 실행 상태를 보인다.

**확인됨 — 2026-08-23.** runtime을 주입하지 않고 static entry 뒤 native `ExitProcess` target에 software breakpoint를 둔 trace에서, Winsock DLL 해제 다음에 실행마다 달라지는 private allocation address(예: `0x00257004`)에서 `EXCEPTION_ILLEGAL_INSTRUCTION` (`0xc000001d`)가 발생했다. 해당 page는 `MEM_PRIVATE | PAGE_READWRITE`이며 첫 바이트는 `ff ff ff ff 00 00 40 00 ...`으로 유효한 x86 code가 아닌 데이터다. 이 예외를 `DBG_EXCEPTION_NOT_HANDLED`로 guest에 넘기면 세 thread와 process가 `0xc000001d`로 종료하며, `ExitProcess` breakpoint는 발생하지 않는다. 즉 현재 관찰에서 직접적인 종료 경로는 `ExitProcess` import가 아니라 처리되지 않은 illegal-instruction exception이다.

**추정 — 2026-08-23.** raw run의 exit code `0x001affcc`는 debugger run의 exception ESP `0x001aff80` 근처 stack address와 같은 범위다. 종료 code가 안정적인 Win32 error가 아니라 예외/종료 경로에서 남은 값일 가능성이 있으나, 두 run의 stack state가 동일하다고 확인하지 않았으므로 확정하지 않는다.

**추정 — 2026-08-23.** 보호된 이미지 안에 `WSOCK32.DLL` 문자열이 있고 hardware trace도 Winsock 적재를 보이므로, entry 이후의 protected path가 Winsock을 동적으로 사용한다고 추정한다. `umpdc.dll`은 host system DLL이므로 이 관찰만으로 게임이나 보호 stub이 직접 load했다고 단정하지 않는다.

**미확정.** private RW data page로 제어를 넘긴 caller와 조건, Winsock 사용 목적, 그리고 이 잘못된 전이가 보호 검증 실패·누락된 Windows 9x 환경·현대 Windows 호환성 중 무엇에서 비롯되는지는 아직 알 수 없다. runtime injection이 raw 실행 경로를 바꾸는 이유도 미확정이다. exception stack의 return word는 Windows exception dispatcher와 allocation 주소만 보여 guest caller를 식별하지 못했다. 다음 증거는 invalid target을 설정한 guest instruction 또는 그 직전 API/indirect-call trace다.

**Confirmed — 2026-08-23.** Canonical `ez2dj.exe` reaches static-entry VA `0x01ed23cf` under a software `INT3` breakpoint, and x86 runtime DLL injection succeeds at that stop. The hardware entry breakpoint is not delivered, so its trace cannot establish a pre-entry exit.

**Confirmed — 2026-08-23.** Directly starting `ez2dj.exe` in the same working directory without a debugger exits with `0x001affcc` within ten seconds. The hardware-breakpoint trace showed `umpdc.dll`, `wsock32.dll`, and `ws2_32.dll` loading; the two Winsock DLLs unloading; three threads exiting with code zero; and process exit `0x001affcc`. Because the software breakpoint reaches entry, this dynamic-DLL activity and exit occur after entry. `0x001affcc` did not resolve to a Win32 error message on this host.

**Confirmed — 2026-08-23.** After runtime injection at static entry and replacement of the `ExitProcess` IAT slot with an observation wrapper, a ten-second resumed run retained the wrapper address in that slot and delivered neither the wrapper nor a process-exit event. This runtime-injected run has different timing or execution state from the raw run.

**Confirmed — 2026-08-23.** In a trace without runtime injection that placed a software breakpoint on the native `ExitProcess` target after static entry, an `EXCEPTION_ILLEGAL_INSTRUCTION` (`0xc000001d`) occurred after Winsock DLL unload at a run-varying private-allocation address (for example, `0x00257004`). The page was `MEM_PRIVATE | PAGE_READWRITE`, and its first bytes were `ff ff ff ff 00 00 40 00 ...`, data rather than valid x86 code. Passing this exception to the guest with `DBG_EXCEPTION_NOT_HANDLED` ended three threads and the process with `0xc000001d`; the `ExitProcess` breakpoint never fired. The direct observed termination path is therefore an unhandled illegal-instruction exception, not the `ExitProcess` import.

**Inferred — 2026-08-23.** The raw-run exit code `0x001affcc` is in the same range as the debugger-run exception ESP `0x001aff80`. It may be a residual value from exception or termination handling rather than a stable Win32 error, but the two runs' stack state has not been proven identical.

**Inferred — 2026-08-23.** The protected image contains `WSOCK32.DLL` strings and the hardware trace shows Winsock loading, so a protected path after entry likely uses Winsock dynamically. `umpdc.dll` is a host system DLL, and this observation alone does not prove that the game or protection stub directly loaded it.

**Unresolved.** The guest caller and condition that transfer control to the private RW data page, the purpose of Winsock use, and whether this invalid transfer arises from a protection-check failure, missing Windows 9x environment, or modern-Windows compatibility are unknown. It is also unresolved why runtime injection changes the raw execution path. The exception-stack return words showed only Windows exception-dispatch and allocation addresses, not a guest caller. The next evidence needed is the guest instruction that sets the invalid target or an API/indirect-call trace immediately before it.

**Confirmed — 2026-08-23.** A software-entry instruction trace rearmed TF on every primary-thread debug event and collected 200,000 single steps from canonical `ez2dj.exe`. It reached protected-image code around `0x01ed2dce` through `0x01ed2e67`, including byte-wise comparison and loop instructions, but did not reach the illegal instruction before the configured limit. The bounded trace therefore establishes that the facility observes sustained guest execution; it does not identify the invalid-target caller. The trace's debugger overhead and its perturbation of timing remain material limitations.

**Unresolved — 2026-08-23.** A less intrusive observation point is still needed to capture the branch into the run-varying private RW target. Possible techniques include an allocation/protection observation trace or a targeted data watch once the target-producing storage is identified; neither has yet been verified against the original process.

**Confirmed — 2026-08-23.** At the first-chance illegal-instruction event, a bounded scan of committed `MEM_PRIVATE` and `MEM_IMAGE` memory completed before guest exception delivery. In one run it scanned 197 regions and 53,836,035 bytes for fault `0x00359004`; there were zero exact-address matches and 20 page-base (`0x00359000`) matches. The fault page and base vary between runs, while the allocation base remained `0x00200000`. The observed page-base references include stack/heap and system-image regions, so none is yet a proven target-producing storage location.

**Inferred — 2026-08-23.** The absence of an exact fault-address value in the first-chance bounded scan is consistent with an indirect target computed in a register, transient storage overwritten before fault delivery, or an unscanned region. It does not distinguish these explanations and does not establish the protection failure cause.

**Confirmed — 2026-08-23.** With `--api-trace`, the protected stub's post-entry API flow is: `GetVersion` from `0x01ed49d9`; `CreateFileA("\\.\LPTDI1", GENERIC_READ, FILE_SHARE_READ)` from `0x01ed41f1`; `GetVersion` from `0x01ed2582`; `LoadLibraryA("WSOCK32.DLL")` from `0x01ed2599`; `GetProcAddress(wsock32, "WSAGetLastError")` from `0x01ed25b7`; `FreeLibrary` from `0x01ed25c9`. Caller addresses repeat across runs, and no `VirtualAlloc`/`VirtualProtect` call occurs before the fault. The `\\.\LPTDI1` parallel-port device probe matches the `.gdata` strings `\\.\TDSD.VXD` and `\\.\LPTDI0` and the disabled `Tdsd.vxd111` driver file in the dump root, so the protected path performs hardware/I-O environment checks at startup.

*Confirmed — 2026-08-23. With `--api-trace`, the post-entry flow is GetVersion → CreateFileA on `\\.\LPTDI1` → GetVersion → LoadLibraryA("WSOCK32.DLL") → GetProcAddress("WSAGetLastError") → FreeLibrary, with stable caller addresses across runs and no VirtualAlloc/VirtualProtect before the fault. The parallel-port device probe matches the .gdata device strings and the disabled Tdsd.vxd111 file, so the protected path performs hardware/I-O environment checks at startup.*

**Confirmed — 2026-08-23.** Unload-tail single-step capture (5,917 steps) shows the illegal instruction is not a guest branch: during the tail of `LdrUnloadDll`, a win32k system-call stub (`mov eax,0x7000e; mov edx,&thunk; call edx; ret 8`) runs `jmp dword ptr [ntdll data]` onto the WOW64 transition gate (`jmp far 0x33:...`), and immediately after entering 64-bit mode the CPU fetches from a non-executable private RW page. Fault-time registers are stable across runs: `EBX` = fault page base, `ECX=EDX=ESI=EDI` = entry VA `0x01ed23cf`, `EAX` = stack address `0x001affcc`, which equals the raw-run exit code and confirms that exit code is a residual register value. The fault page content is a deterministic structure `{0x00010000, 0xffffffff, image base 0x00400000, ntdll pointers, ...}` inside the pre-existing process-heap allocation based at `0x00200000`.

*Confirmed — 2026-08-23. Unload-tail single-stepping shows the fault happens on the 64-bit side of a WOW64 win32k syscall transition inside the LdrUnloadDll tail, not through a guest branch. Registers are stable across runs; EAX holds the raw-run exit-code value, confirming it as residual register state; the fault page carries a deterministic structure within the pre-existing process heap.*

**Confirmed — 2026-08-23.** The `.gtide` stub is self-modifying: static bytes at runtime caller addresses disagree with observed calls, and the section is laced with `eb 01 e8`-style anti-disassembly jumps and XOR decrypt loops. Static analysis alone cannot interpret the executed path.

*Confirmed — 2026-08-23. The .gtide stub is self-modifying — static opcodes disagree with runtime calls, with anti-disassembly jumps and XOR decrypt loops throughout — so static analysis alone cannot interpret the executed path.*

**Confirmed — 2026-08-23.** Precision observation narrows the transition further. The syscall stub is exactly `ntdll!ZwSetEvent` (nearest-export resolution hits the export entry at offset 0; the `mov eax,0x7000e; mov edx,&thunk; call edx; ret 8` pattern matches NtSetEvent's two arguments), invoked by an internal routine that pushes NULL and a global ntdll event handle. At the fault the synthesized context reports 32-bit user segments (`cs=0x0023, fs=0x0053`). Crossing the gate flips the reported GPRs to save-area values and silently ends single-step reporting — the trap flag does not survive the WOW64 transition, so the 64-bit side and the return into 32-bit code remain untraced by software stepping.

*Confirmed — 2026-08-23. The pre-transition syscall is exactly ntdll!ZwSetEvent, called with a global event handle; at the fault the reported context uses CS=0x0023/FS=0x0053; and single-step reporting dies at the gate because TF does not survive the WOW64 transition.*

**Confirmed — 2026-08-23.** Resume tracing pins the death inside heap teardown. A one-shot software breakpoint on the detected `call edx` stub's return address (`0x77109b9c`) catches the 32-bit resume after the gate; the trap flag re-arms there and sampling continues for hundreds of instructions. `ZwSetEvent` returns success (EAX=0) with wsock32's image base preserved in EBX/EDI, and the path then runs `RtlAcquireSRWLockExclusive` → `RtlDestroyHeap` → a five-argument `ZwProtectVirtualMemory(-1, &base, &size, protect, &old)` → a second gate crossing → immediate #UD on a fresh private RW page (`0x0027c004` in the tracing run). The termination is therefore inside unload-tail heap destruction plus page-protection work, not inside ZwSetEvent, and the fault page's reservation at allocation base `0x00200000` overlapping the destroyed heap is the strongest correlation (inferred).

*Confirmed — 2026-08-23. Resume tracing via a software breakpoint on the stub return address shows ZwSetEvent returning success, then RtlAcquireSRWLockExclusive → RtlDestroyHeap → ZwProtectVirtualMemory → second gate → immediate #UD on a fresh private RW page; the crash lives inside unload-tail heap teardown, with fault-page/heap overlap as the strongest correlation (inferred).*

**Unresolved — 2026-08-23.** Why the second gate's 64-bit processing transfers to a private RW page, whether this termination is deliberate anti-tamper after failed dongle/environment checks or collateral of running Win9x-era checks on modern Windows, and whether `RtlDestroyHeap` belongs to ws2_32's detach path. Next evidence: dump the top 64 stack words at the resume hit to find return addresses inside the dynamically loaded ws2_32 range, correlate `RtlDestroyHeap`'s heap-handle argument with the fault allocation, and search fault-time memory for `{entry VA ×4}`-style planted patterns behind the stable register signature.

*Unresolved — 2026-08-23. The post-gate transfer mechanism, deliberate-versus-collateral, and the owner of RtlDestroyHeap stay open. Candidate next evidence: deep stack dump at the resume hit, heap-handle/allocation correlation, and a memory search for planted entry-VA patterns.*

### 확인됨: ez2dj1.exe는 같은 프로그램의 보호되지 않은 빌드다

두 파일의 섹션 배치가 앞부분에서 정확히 일치한다.

| 섹션 | ez2dj1.exe | ez2dj.exe |
| --- | --- | --- |
| `.text` | va `0x00001000` vs `0x00052540` | 동일 |
| `.rdata` | va `0x00054000` vs `0x00007571` | 동일 |
| `.data` | va `0x0005c000` vs `0x01a5d2f8` | 동일 |
| `.idata` | va `0x01aba000` vs `0x00000fa4` | 동일 |
| `.reloc` | va `0x01abb000` vs `0x00015094` | 동일 |

import 목록도 사실상 같다(아래 4절). `ez2dj.exe`는 이 이미지에 보호 계층을 씌운 것이다.

*The two share their first five sections byte-for-byte in layout and share essentially the same import list, so `ez2dj.exe` is this image with a protection layer wrapped around it.*

> [!IMPORTANT]
> **`ez2dj1.exe`가 Stage 2·3의 첫 실행 대상이다.** 보호되지 않았으므로 로더가 언패킹 스텁을 실행하지 않고도 진짜 게임 코드에 도달한다. 보호된 빌드는 자기 수정 코드를 실행할 수 있어야 하므로 인터프리터 backend가 성숙한 뒤로 미룬다.
>
> ***`ez2dj1.exe` is the bring-up target for Stages 2 and 3.** Being unprotected, the loader reaches real game code without executing an unpacking stub. The protected builds need an execution backend that tolerates self-modifying code, so they wait until the interpreter is mature.*

### 추정: `.data`의 거대한 가상 크기

`.data`는 RawSize `0x0000d000`(52 KB)인데 VirtualSize가 `0x01a5d2f8`(약 27 MB)이다. 27 MB는 파일에서 오는 것이 아니라 0으로 채워지는 영역이다. 게임 자산을 담을 정적 버퍼로 추정한다. 근거는 크기와 `.data`라는 위치뿐이며, 실제 용도는 실행해 봐야 확인된다.

*Inferred: `.data` is 52 KB on disk but about 27 MB in memory, so roughly 27 MB is zero-filled. A static buffer for game assets is the likely purpose, but the only evidence is its size and placement, and real use needs a run to confirm.*

### 확인됨: 캐비닛이 실행하는 것은 `ez2dj.exe`다

1st SE 덤프의 `System.ini` `[boot]` 절에 결정적 항목이 있다.

```ini
shell=d:\ez2dj\ez2dj.exe
```

Windows 9x는 `[boot]`의 `shell=` 항목이 가리키는 프로그램을 Explorer 대신 띄운다. 즉 이 한 줄이 아케이드 캐비닛의 부팅 후 진입점을 정의한다. 여기서 세 가지가 동시에 확정된다.

| 항목 | 값 | 근거 |
| --- | --- | --- |
| 정식 실행 파일 | `ez2dj.exe` | `shell=` 값의 파일 이름 |
| 게스트 드라이브 문자 | `D:` | `shell=` 값의 드라이브 |
| 게스트 작업 디렉터리 | `\ez2dj` | `shell=` 값의 디렉터리 |

드라이브 문자와 작업 디렉터리는 이 문서와 `docs/EXE_DESIGN.*`에서 미확정으로 남아 있던 항목이다.

**`ez2dj1.exe`는 캐비닛이 실행한 것이 아니다.** 보호되지 않아 로더 개발에 유용할 뿐이므로, 그것으로 관찰한 동작을 원본 동작으로 인용하면 안 된다. 이 구분은 타깃 프로파일에 `bring_up_target` 플래그로 기록되어 있다.

*Confirmed: the cabinet runs `ez2dj.exe`. The `[boot]` section of `System.ini` reads `shell=d:\ez2dj\ez2dj.exe`, and Windows 9x launches whatever `shell=` names in place of Explorer, so that single line defines the cabinet's post-boot entry point and fixes the canonical executable, the guest drive letter `D:`, and the guest directory `\ez2dj` at once. The latter two had been unresolved. **`ez2dj1.exe` is not what the cabinet ran** — it is merely unprotected and therefore useful for loader development, so behavior observed through it must not be cited as original behavior. That distinction is recorded on the target profile as a `bring_up_target` flag.*

### 확인됨: 3rd는 I/O 카드를 쓴다

3rd 덤프의 `EZ2DJ.INI`에 `"UseIOCard" = 1`이 있다. 해상도는 `"Window Width" = 640`, `"Window Height" = 480`이고 `"FullScreen" = 1`이다.

*Confirmed: the 3rd dump's `EZ2DJ.INI` carries `"UseIOCard" = 1`, a 640x480 window size, and `"FullScreen" = 1`.*

### 미확정: 3rd의 게스트 경로

3rd 덤프에는 `System.ini`가 없다. 따라서 3rd의 게스트 드라이브 문자와 작업 디렉터리는 확인되지 않았다. 1st SE의 값을 복사해 넣지 않는다.

*Unresolved: the 3rd dump has no `System.ini`, so its guest drive letter and working directory are not confirmed. The 1st SE values are not copied across.*

### 미확정: Tdsd.vxd111

`Tdsd.vxd111` 파일이 1st SE 덤프에 있다. VxD는 Windows 9x 전용 커널 드라이버이므로 아케이드 I/O 보드 접근 경로일 가능성이 있다. 다만 `ez2dj1.exe`의 import에는 `DeviceIoControl`이 없고, 파일 이름의 `111` 확장자는 이 덤프에서 드라이버가 **비활성화되어 있음**을 시사한다.

**확인 방법:** 실행 중 `CreateFileA`가 요청하는 경로를 추적한다. `\\.\`로 시작하는 이름이 나오면 드라이버 경로다.

*Unresolved: `Tdsd.vxd111` is a Windows 9x kernel driver and could be the arcade I/O path, but `ez2dj1.exe` imports no `DeviceIoControl` and the `111` suffix suggests the driver is disabled in this dump. Confirm by tracing the paths `CreateFileA` asks for during a run; a name starting with `\\.\` is a driver path.*

---

## 4. import 목록 / Import list

별도 문서로 분리했다: [EZ2DJ import 표면](ez2dj-import-surface.md)

*Split into its own document: [EZ2DJ Import Surface](ez2dj-import-surface.md)*

---

## 5. 도구의 결함 / Defects in the tooling

**해결됨 — 기본 타깃 선택.** `re2dj --hdd <1st SE dump>`가 기본 타깃으로 `Test.exe`를 골랐다. 후보 순위가 파일 크기 내림차순이라 서비스 도구(1.86 MB)가 게임(561 KB)보다 먼저 왔기 때문이다.

크기는 "어느 것이 게임인가"의 근거가 못 된다. 순위 휴리스틱을 손보는 대신 **내장 타깃 프로파일**을 추가해 해결했다. 지금은 두 덤프 모두 정확한 기본 타깃(`ez2dj1stse` → `ez2dj.exe`, `ez2dj3rd` → `EZ2DJ.EXE`)을 고른다. 설계는 [20260822-005](../design/20260822-005-built-in-target-profiles.md)에 있다.

*Resolved: the default target used to be `Test.exe` for the 1st SE dump, because ranking broke ties by descending file size. Size is not evidence of which file is the game, so the fix was built-in target profiles rather than a better heuristic. Both dumps now select correctly.*

**미해결 — 비ASCII 경로 출력.** `re2dj_hdd_probe`가 비ASCII 문자가 든 디렉터리 경로를 콘솔에 깨진 형태로 출력한다. 해석 자체는 정상이고 출력만 깨진다. `std::filesystem::path::string()`이 Windows에서 활성 ANSI 코드 페이지로 변환하기 때문이다.

*Open: `re2dj_hdd_probe` prints a directory path containing non-ASCII characters as mojibake. Resolution itself works and only the output is wrong, because `std::filesystem::path::string()` converts through the active ANSI code page on Windows.*

`re2dj_hdd_probe`가 비ASCII 문자가 든 디렉터리 경로를 콘솔에 깨진 형태로 출력했다. 해석 자체는 정상이었고 출력만 깨졌다. `std::filesystem::path::string()`이 Windows에서 활성 ANSI 코드 페이지로 변환하기 때문이다. 별도로 다룬다.

*`re2dj_hdd_probe` printed a directory path containing non-ASCII characters as mojibake. Resolution itself worked and only the output was wrong, because `std::filesystem::path::string()` converts through the active ANSI code page on Windows. Handled separately.*

---

## 관련 문서 / Related documents

* [EZ2DJ import 표면](ez2dj-import-surface.md)
* [PE32 실행 형식](../kb/pe32-executable-format.md)
* [Win32 HLE 경계](../kb/win32-hle-boundary.md)
* [원본 실행 파일 분석 (누적)](../EXE_DESIGN.ko.md)
