# 보호 stub API 관찰 trace 작업 로그

관련 작업 지시: [보호 stub API 관찰 trace 작업 지시](../work-orders/20260823-042-protected-api-observation-trace.md)  
관련 설계: [보호 stub API 관찰 trace](../design/20260823-042-protected-api-observation-trace.md)

## 구현 결과

`re2dj_windows_x86_launcher_probe`에 `--api-trace`를 추가했습니다. 이 옵션은 software entry breakpoint와 native `ExitProcess` breakpoint를 사용하는 기존 `--break-exit-process` 경로 위에서 다음을 수행합니다.

1. `LOAD_DLL_DEBUG_EVENT`의 file path에서 `kernel32.dll`과 `kernelbase.dll` base를 기록합니다.
2. 새 헬퍼 `remote_module_exports`가 child process memory에서 PE32 export directory를 해석해 watched API(`LoadLibraryA`, `GetProcAddress`, `VirtualAlloc`, `VirtualProtect`, `VirtualFree`, `GetVersion`, `GetVersionExA`, `CreateFileA`, `FreeLibrary`)를 VA로 바꾸고 software breakpoint를 설치합니다. forwarded export는 건너뛰고, 같은 이름이 두 모듈에서 실제 코드로 존재하면 양쪽을 모두 관찰합니다(이 host의 kernel32 export는 kernelbase로의 `jmp` stub이어서 논리 호출마다 두 기록이 남습니다).
3. API hit에서 thread·caller·stack args 4개와 `LoadLibraryA`·`GetProcAddress`·`CreateFileA`의 ANSI 문자열 인자를 JSONL에 남긴 뒤, 원래 byte 복원 → EIP 복귀 → TF 1 step → INT3 재기록으로 삼키고 재무장합니다. 재무장은 thread별 pending map으로 관리합니다.
4. first-chance `EXCEPTION_ILLEGAL_INSTRUCTION`에서 full GP register, 64 dword stack과 main image section 분류, fault page 128 byte dump, allocation region walk를 기록합니다. 이 dump는 `--api-trace` 없이도 동작합니다.
5. 확장: entry 이후 동적으로 적재된 모듈의 `UNLOAD_DLL_DEBUG_EVENT`에서 primary thread에 TF를 설정하고 illegal instruction까지 instruction 주소·바이트를 48개 ring buffer로 수집합니다. 수집 중 이벤트 예산은 `kUnloadStepCap`으로 넓어집니다.

## 검증 결과

1. `cmake --build --preset windows-x86-debug --config Debug` 성공.
2. `ctest --test-dir build\windows-x86 -C Debug --output-on-failure` 성공: 2/2 통과.
3. canonical `ez2dj.exe`를 `--api-trace`로 실행해 다음을 관찰했습니다.

### 확인됨: post-entry API 흐름과 caller

```text
GetVersion                caller 0x01ed49d9
CreateFileA("\\.\LPTDI1", GENERIC_READ, FILE_SHARE_READ)
                          caller 0x01ed41f1
GetVersion                caller 0x01ed2582
LoadLibraryA("WSOCK32.DLL")   caller 0x01ed2599
GetProcAddress(wsock32, "WSAGetLastError")  caller 0x01ed25b7
FreeLibrary(wsock32)      caller 0x01ed25c9
→ 두 Winsock DLL 언로드 직후 0xC000001D
```

caller 주소는 실행마다 동일했습니다. `CreateFileA` 문자열은 JSON sanitizing으로 `\`가 `.`로 바뀐 `....LPTDI1`이며, 원본은 `\\.\LPTDI1` 병렬포트 디바이스 경로입니다. fault 전에 `VirtualAlloc`·`VirtualProtect` 호출은 한 번도 관찰되지 않았습니다.

### 확인됨: fault는 WOW64 win32k 시스템 콜 전환의 64비트 쪽에서 발생

언로드 종반 5,917 single step을 수집한 결과, 마지막 sample은 다음과 같습니다.

```text
0x770fdf3d  push 0; push [0x771c71b8]; call 0x77109b90
0x77109b90  mov eax,0x7000e; mov edx,0x771455c0; call edx; ret 8
0x771455c0  jmp dword ptr [0x771cb014]
0x77087000  jmp far 0x33:0x77087009   ; WOW64 64-bit 전환 gate
→ 0x003d1004 illegal instruction
```

즉 `FreeLibrary`의 `LdrUnloadDll` 종반에서 win32k 시스템 콜이 64비트 모드로 전환된 직후, 실행 가능하지 않은 private RW page에서 명령 fetch가 실패했습니다. fault는 guest로 돌아오기 전에 발생하므로, guest caller가 직접 branch한 것이 아닙니다.

### 확인됨: fault 시점 register와 page 내용은 실행마다 동일한 구조

```text
eax=0x001affcc  ebx=fault page base  ecx=edx=esi=edi=0x01ed23cf(entry)
ebp=kernel32 내부 주소  esp=0x001aff80  eip=fault page base+4
fault page: {0x00010000, 0xffffffff, 0x00400000(image base), ntdll 포인터들, ...}
```

fault page는 allocation base `0x00200000`짜리 기존 process heap의 실행마다 달라지는 committed RW sub-region 안에 있습니다. `EAX=0x001affcc`는 debugger 없는 raw run의 종료 code와 같은 값으로, 종료 code가 error code가 아니라 예외 시점 register 잔재라는 이전 추정을 뒷받침합니다.

### 확인됨: .gtide는 자기 수정 코드이고 정적 바이트와 실행 바이트가 다르다

정적 덤프에서 caller 주소들의 opcode가 관찰된 호출과 어긋나며, `eb 01 e8`류 anti-disassembly 점프와 XOR 복호화 루프가 전반에 깔려 있습니다. 보호 이미지의 실행 경로는 파일만으로 해석할 수 없습니다.

### 확인됨: 보호 이미지 import 표면과 .gdata 문자열

`.gidata` import directory는 원본 게임 전체 표면(KERNEL32 97개급, USER32, GDI32, ADVAPI32 `RegFlushKey`, DSOUND ordinal 1, WINMM mixer, DDRAW)에 보호 특화 API(`DeviceIoControl`, `_lopen`/`_lread`/`_lclose`/`_lcreat`/`_llseek`/`OpenFile`, `DeleteFileA`, `MoveFileA`, `lstrcmpiA`/`lstrcpyA`/`lstrlenA`, `DebugBreak`, `OutputDebugStringA`, `FatalAppExitA`, `UnhandledExceptionFilter`, `RaiseException`, `RtlUnwind`, `IsBadReadPtr`/`IsBadWritePtr`, `HeapValidate`, `CreateMutexA`/`ReleaseMutex`, `CreateProcessA`, `TerminateThread` 등)를 더한 집합입니다. `.gdata`에는 `\\.\TDSD.VXD`, `\\.\LPTDI0`, `MSVBVM50.DLL` 2회, EUC-KR 메시지 바이트, 해시성 blob이 있습니다.

## 결론

invalid target으로의 전이는 guest 코드의 직접 branch가 아니라, `FreeLibrary`가 유발한 DLL 언로드 종반의 win32k 시스템 콜이 WOW64 64비트 모드로 전환된 직후 실행 가능하지 않은 heap page를 fetch하면서 발생합니다. stub은 진입 직후 `\\.\LPTDI1` 병렬포트 디바이스와 Winsock 최소 프로브를 수행하므로, 이 종료 경로가 하드웨어 동글·환경 검사 실패의 결과인지 현대 WOW64 환경과의 부정합인지는 여전히 미확정입니다. 다음 단계 후보는 fault 시 64비트 컨텍스트(`Wow64GetThreadContext`) 포획, win32k 시스템 콜 번호 0x7000e의 서비스 식별, `EBX`에 fault page base를 남긴 코드 추적입니다.

---

# Protected Stub API Observation Trace Work Log

Related work order: [Protected Stub API Observation Trace Work Order](../work-orders/20260823-042-protected-api-observation-trace.md)  
Related design: [Protected Stub API Observation Trace](../design/20260823-042-protected-api-observation-trace.md)

## Implementation result

Added `--api-trace` to `re2dj_windows_x86_launcher_probe`. On top of the existing `--break-exit-process` path (software entry breakpoint plus native `ExitProcess` breakpoint) it now:

1. Records the `kernel32.dll` and `kernelbase.dll` bases from `LOAD_DLL_DEBUG_EVENT` file paths.
2. Resolves watched APIs (`LoadLibraryA`, `GetProcAddress`, `VirtualAlloc`, `VirtualProtect`, `VirtualFree`, `GetVersion`, `GetVersionExA`, `CreateFileA`, `FreeLibrary`) through the new `remote_module_exports` helper, which parses the PE32 export directory from child process memory, skips forwarded exports, and watches both modules when both hold real code (this host's kernel32 exports are `jmp` stubs into kernelbase, so each logical call logs twice).
3. On API hits, records thread, caller, four stack arguments, and ANSI string arguments of `LoadLibraryA`, `GetProcAddress`, and `CreateFileA` to JSONL, then swallows the hit by restoring the original byte, rewinding EIP, trap-flagging one instruction, and rearming the INT3, with per-thread pending tracking.
4. On first-chance `EXCEPTION_ILLEGAL_INSTRUCTION`, records full GP registers, a 64-dword stack with main-image section classification, a 128-byte fault page dump, and an allocation region walk. This dump also runs without `--api-trace`.
5. Extension: on an `UNLOAD_DLL_DEBUG_EVENT` for a module loaded dynamically after entry, sets TF on the primary thread and collects instruction addresses and bytes into a 48-entry ring buffer until the illegal instruction, with the event budget widened by `kUnloadStepCap` while collecting.

## Verification result

1. `cmake --build --preset windows-x86-debug --config Debug` succeeded.
2. `ctest --test-dir build\windows-x86 -C Debug --output-on-failure` succeeded: 2/2 passed.
3. Ran canonical `ez2dj.exe` with `--api-trace` and observed the following.

### Confirmed: post-entry API flow and callers

```text
GetVersion                caller 0x01ed49d9
CreateFileA("\\.\LPTDI1", GENERIC_READ, FILE_SHARE_READ)
                          caller 0x01ed41f1
GetVersion                caller 0x01ed2582
LoadLibraryA("WSOCK32.DLL")   caller 0x01ed2599
GetProcAddress(wsock32, "WSAGetLastError")  caller 0x01ed25b7
FreeLibrary(wsock32)      caller 0x01ed25c9
→ 0xC000001D right after both Winsock DLLs unload
```

The caller addresses are identical across runs. The `CreateFileA` string logs as `....LPTDI1` because JSON sanitizing replaces backslashes; the original is the `\\.\LPTDI1` parallel-port device path. No `VirtualAlloc` or `VirtualProtect` call was observed before the fault.

### Confirmed: the fault occurs on the 64-bit side of a WOW64 win32k syscall transition

Collecting 5,917 unload-tail single steps, the final samples are:

```text
0x770fdf3d  push 0; push [0x771c71b8]; call 0x77109b90
0x77109b90  mov eax,0x7000e; mov edx,0x771455c0; call edx; ret 8
0x771455c0  jmp dword ptr [0x771cb014]
0x77087000  jmp far 0x33:0x77087009   ; WOW64 64-bit transition gate
→ 0x003d1004 illegal instruction
```

The tail of `FreeLibrary`'s `LdrUnloadDll` therefore makes a win32k system call, and immediately after the transition into 64-bit mode the instruction fetch fails on a non-executable private RW page. The fault happens before control returns to the guest, so the guest did not branch there directly.

### Confirmed: fault-time registers and page content share one structure across runs

```text
eax=0x001affcc  ebx=fault page base  ecx=edx=esi=edi=0x01ed23cf(entry)
ebp=kernel32-internal address  esp=0x001aff80  eip=fault page base+4
fault page: {0x00010000, 0xffffffff, 0x00400000(image base), ntdll pointers, ...}
```

The fault page lies inside the pre-existing process heap allocation based at `0x00200000`, in a committed RW sub-region whose position varies per run. `EAX=0x001affcc` equals the raw-run exit code, supporting the earlier inference that the exit code is a residual register value rather than an error code.

### Confirmed: .gtide is self-modifying and its runtime bytes differ from file bytes

Static opcodes at the caller addresses disagree with the observed calls, and the section is laced with `eb 01 e8`-style anti-disassembly jumps and XOR decrypt loops. The protected image's execution path cannot be interpreted from the file alone.

### Confirmed: protected import surface and .gdata strings

The `.gidata` import directory carries the full original game surface (KERNEL32 ~97, USER32, GDI32, ADVAPI32 `RegFlushKey`, DSOUND ordinal 1, WINMM mixers, DDRAW) plus protection-flavored APIs (`DeviceIoControl`, `_lopen`/`_lread`/`_lclose`/`_lcreat`/`_llseek`/`OpenFile`, `DeleteFileA`, `MoveFileA`, `lstrcmpiA`/`lstrcpyA`/`lstrlenA`, `DebugBreak`, `OutputDebugStringA`, `FatalAppExitA`, `UnhandledExceptionFilter`, `RaiseException`, `RtlUnwind`, `IsBadReadPtr`/`IsBadWritePtr`, `HeapValidate`, `CreateMutexA`/`ReleaseMutex`, `CreateProcessA`, `TerminateThread`, and more). `.gdata` holds `\\.\TDSD.VXD`, `\\.\LPTDI0`, `MSVBVM50.DLL` twice, EUC-KR message bytes, and hash-like blobs.

## Conclusion

The transfer to the invalid target is not a direct guest branch: the tail of the DLL unload triggered by `FreeLibrary` makes a win32k system call, and right after the WOW64 transition into 64-bit mode the CPU fetches from a non-executable heap page. The stub performs a `\\.\LPTDI1` parallel-port device probe and a minimal Winsock probe right after entry, so whether this termination path is the consequence of failed hardware-dongle/environment checks or of a mismatch with the modern WOW64 environment remains unresolved. Next-step candidates: capture the 64-bit context at the fault (`Wow64GetThreadContext`), identify the win32k service behind syscall number 0x7000e, and trace what leaves the fault page base in `EBX`.
