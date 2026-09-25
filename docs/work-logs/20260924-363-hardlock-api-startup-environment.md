# 작업 363 작업 로그 — Hardlock API 시작 환경 / Task 363 work log — Hardlock API startup environment

설계: [20260924-363-hardlock-api-startup-environment.md](../design/20260924-363-hardlock-api-startup-environment.md)
작업 지시서: [20260924-363-hardlock-api-startup-environment.md](../work-orders/20260924-363-hardlock-api-startup-environment.md)

## 조사 / Investigation

설계 전에, 커밋하지 않은 실험 build(Linux x86)에서 요구되는 export를 임시로 하나씩 넣었다. 각 단계마다 실제 4th를 다시 실행해 다음 경계를 확인했다. 결과는 설계의 표에 있고, 실험 코드는 모두 되돌렸다. 정적 import 표에서 첫 `KERNEL32.dll` 묶음 25개가 Hardlock envelope의 함수임도 확인했다. 파일, library, 시간, `Sleep` 함수가 들어 있다. 다른 DLL은 함수 하나씩만 들어 있다.

*Before the design, an uncommitted experimental build (Linux x86) added the requested exports one at a time, rerunning the real 4th at each step to find the next boundary; the results are the design's table, and all experiment code was reverted. The static import table showed that the first `KERNEL32.dll` group of 25 functions belongs to the Hardlock envelope, covering files, libraries, time, and `Sleep`, while every other DLL lists a single function.*

## 변경 / Changes

- **`hle::GuestProcess`**(`include/re2dj/hle/guest_process.h`): process ID `0x0F00`, `ExchangeErrorMode`, heap bookkeeping(first fit, 8 byte 정렬, `BlockContains`). `ImportCallServices`에 선택 서비스 `Process()`와 `IsGuestModule()`을 추가했다.
  ***`hle::GuestProcess`**: process ID `0x0F00`, `ExchangeErrorMode`, and heap bookkeeping (first fit, eight-byte alignment, `BlockContains`); `ImportCallServices` gains the optional `Process()` and `IsGuestModule()`.*
- **없는 이름.** `GuestModuleDescriptor::absent_exports`와 검증(빈 이름·export와 겹침 거절), `GuestModuleRegistry::IsAbsentExport`, `IsAbsentGuestModule`(`wfapi.dll`)을 추가했다. 공용 handler `UnimplementedExport`도 추가했다.
  ***Absent names.** `GuestModuleDescriptor::absent_exports` with validation (rejecting empty or exported names), `GuestModuleRegistry::IsAbsentExport`, `IsAbsentGuestModule` (`wfapi.dll`), and the shared `UnimplementedExport` handler.*
- **`kernel32`**: `GetCurrentProcess`, `GetCurrentProcessId`, `GetEnvironmentVariableA`, `SetErrorMode`, `LoadLibraryA`, `FreeLibrary`, `GetVersionExA`, `GetTickCount`(해석만)을 더해 export가 17개가 됐다. absent 이름은 `IsTNT`, `Borland32`다. Win32 오류 122, 126, 203을 추가했다.
  ***`kernel32`**: adds `GetCurrentProcess`, `GetCurrentProcessId`, `GetEnvironmentVariableA`, `SetErrorMode`, `LoadLibraryA`, `FreeLibrary`, `GetVersionExA`, and `GetTickCount` (resolve-only) for 17 exports, with `IsTNT` and `Borland32` absent; adds Win32 errors 122, 126, and 203.*
- **`user32`**: `CreateCursor`, `DestroyCursor`, `SetCursor`를 해석만 되는 export로 추가했다. **`advapi32`**(새 module): `RegOpenKeyA`, `RegQueryValueExA`, `RegCloseKey`, 모두 해석만 된다. **`wtsapi32`**(새 module): `WTSQuerySessionInformationA`(현재 세션의 `WTSSessionId`만, 세션 0)와 `WTSFreeMemory`.
  ***`user32`** gains resolve-only `CreateCursor`, `DestroyCursor`, and `SetCursor`; the new **`advapi32`** has resolve-only `RegOpenKeyA`, `RegQueryValueExA`, and `RegCloseKey`; the new **`wtsapi32`** has `WTSQuerySessionInformationA` (current session's `WTSSessionId` only, session 0) and `WTSFreeMemory`.*
- **Linux 연결.** `NativeKernel32Diagnostic`이 facade 네 개를 등록한다. 1 MiB 저주소 heap을 잡아 `GuestProcess`로 제공하고, 살아 있는 heap block을 guest byte 허용 범위에 넣는다. absent 이름은 미해석 lookup 기록에서 뺀다. continuation은 absent 이름의 NULL에서 멈추지 않는다. 대신 그 밖의 NULL `LoadLibraryA`에서 `LoadLibraryA(name)` 경계로 멈춘다. `LoadLibraryA`와 `GetEnvironmentVariableA`의 이름 인자도 기록한다.
  ***Linux wiring.** `NativeKernel32Diagnostic` registers four facades, maps a 1 MiB low heap provided through `GuestProcess`, and admits live heap blocks to the guest byte ranges; absent names are left out of the unresolved-lookup record; continuation does not stop on a NULL for an absent name and stops at a `LoadLibraryA(name)` boundary on any other NULL `LoadLibraryA`; the name arguments of `LoadLibraryA` and `GetEnvironmentVariableA` are recorded.*
- **단위 테스트.**
  ***Unit tests.***
  - 메모리 기반 서비스를 `tests/unit/memory_services.h`로 옮겨 공유했다.
    *The memory-backed services moved to `tests/unit/memory_services.h` to be shared.*
  - 새 `guest_process_test.cpp`: heap 할당·해제·재사용·고갈, error mode 교환, WTS 세션 조회와 해제, 거절되는 class·세션, advapi32 해석만.
    *The new `guest_process_test.cpp`: heap allocation, free, reuse, and exhaustion; the error-mode exchange; the WTS session query and free; rejected classes and sessions; advapi32 resolve-only.*
  - `kernel32` 새 export의 동작, `user32` cursor, registry의 absent 조회와 거절도 검사한다.
    *Also checked: the new `kernel32` exports, the `user32` cursors, and the registry's absent lookups and rejections.*

## 검증 / Validation

| 항목 / Item | 결과 / Result |
| --- | --- |
| Linux x64·x86 build, CTest | 경고·오류 없음, 각각 3/3 통과 / no warnings or errors, 3/3 each |
| Linux helper 스크립트, in-process probe, 기존 진단 네 개 / helper script, probes, four diagnostics | 작업 361과 같음(trace 43 frame 포함) / same as Task 361, including the 43-frame trace |
| Windows x86 build, CTest | 오류·경고 없음, 6/6 / no errors or warnings, 6/6 |
| 실제 4th, 두 폭 / real 4th, both widths | 같은 59개 호출(stack·heap 주소만 다름), handshake 2·descriptor 1, `GetProcAddress(kernel32, "OpenProcess")`에서 정지 / the same 59 calls (only stack and heap addresses differ), two handshakes and one descriptor, stop at `GetProcAddress(kernel32, "OpenProcess")` |

실행 기록은 [분석 문서](../analysis/ez2dj4th-linux-inprocess-first-import.md)의 작업 363 절에 두었다. `hardlock.ini` 값은 출력하지 않았다.

*The run record is in the Task 363 section of the [analysis](../analysis/ez2dj4th-linux-inprocess-first-import.md); no `hardlock.ini` values were printed.*

## 다음 / Next

보호 코드는 `OpenProcess`로 자기 process를 연 뒤, `VirtualProtect`, `ReadProcessMemory`, `WriteProcessMemory`, `VirtualAlloc`, `VirtualFree`를 해석한다. 다음 작업은 이 자기 process 메모리 API를 `GuestProcess`에 붙이는 것이다.

*The protection opens its own process with `OpenProcess`, then resolves `VirtualProtect`, `ReadProcessMemory`, `WriteProcessMemory`, `VirtualAlloc`, and `VirtualFree`; the next task attaches these own-process memory APIs to `GuestProcess`.*
