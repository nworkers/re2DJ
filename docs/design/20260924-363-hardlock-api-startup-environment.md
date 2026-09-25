# 작업 363 설계 — Hardlock API 시작 환경 / Task 363 design — Hardlock API startup environment

선행: [작업 361 설계](20260924-361-linux-guest-device-handles.md), [작업 360 설계](20260924-360-hardlock-hle-shared-boundary.md), [작업 340 설계](20260921-340-guest-pe-compatibility-modules.md) 5단계

## 배경 / Background

작업 361 뒤 Linux in-process 실행은 Hardlock initialize를 마친 다음 `GetProcAddress(user32, "CreateCursor")`에서 멈췄다. 이 뒤에 무엇이 오는지 보려고, 커밋하지 않은 실험 build에서 요구되는 export를 임시로 하나씩 넣어 실제 4th CHD를 Linux x86으로 실행했다. 실험 코드는 모두 되돌렸다. 관찰한 순서는 아래와 같다.

*After Task 361 the Linux in-process run finished the Hardlock initialize and stopped at `GetProcAddress(user32, "CreateCursor")`. To see what follows, an uncommitted experimental build added the requested exports one at a time, running the real 4th CHD on Linux x86; all experiment code was reverted. The observed order:*

| 단계 / Step | 게스트 동작 / Guest action | 호출 여부 / Called |
| --- | --- | --- |
| 1 | `user32`: `CreateCursor`, `DestroyCursor`, `SetCursor` 해석 | 해석만 / resolve only |
| 2 | `kernel32`: `GetCurrentProcess`, `GetTickCount` 해석 | 해석만 / resolve only |
| 3 | `GetCurrentProcessId()` (정적 import / static import) | 호출 / called |
| 4 | `GetEnvironmentVariableA("HL_SEARCH", buf, 88)` | 호출 / called |
| 5 | `SetErrorMode(0x8000)` 두 번, `GetModuleHandleA("kernel32.dll")` | 호출 / called |
| 6 | `LoadLibraryA("advapi32.dll")`, `RegOpenKeyA`·`RegQueryValueExA`·`RegCloseKey` 해석 | 해석만 / resolve only |
| 7 | `GetVersion`, `GetProcAddress(kernel32, "GetVersionExA")`, `GetVersionExA(156 byte)` | 호출 / called |
| 8 | `LoadLibraryA("wtsapi32.dll")`, `WTSQuerySessionInformationA`·`WTSFreeMemory` 해석 | 호출 / called |
| 9 | `WTSQuerySessionInformationA(0, WTS_CURRENT_SESSION, 4, &buf, &bytes)` | 호출 / called |
| 10 | `LoadLibraryA("wfapi.dll")` | 호출 / called |
| 11 | `SetErrorMode(0)`, `LoadLibraryA("kernel32.dll")`, `GetProcAddress("IsTNT")`, `GetProcAddress("Borland32")`, `FreeLibrary(kernel32)` | 호출 / called |
| 12 | `CreateFileA("\\.\FEnteDev")`, handshake `0x450` 두 번, descriptor `0x44c` 한 번 | 호출 / called |
| 13 | `GetProcAddress`: `GetCurrentProcessId`, `OpenProcess`, 다음으로 `VirtualProtect`, `ReadProcessMemory`, `WriteProcessMemory`, `VirtualAlloc`, `VirtualFree` | — |

- **확인됨(실험).** 12단계의 handshake와 descriptor는 작업 360의 공용 Hardlock HLE가 처리한다(`hardlock requests: total=4 initialize=1 handshake=2 descriptor=1`).
  *Confirmed (experiment): Task 360's shared Hardlock HLE answers step 12's handshakes and descriptor.*
- **확인됨(실험).** 13단계 뒤 자기 process 메모리를 다루는 API가 없으면, 보호 코드는 `"Error 1003 : Internal Error."`를 띄우고 `ExitProcess(3)`으로 끝난다.
  *Confirmed (experiment): without the process-memory APIs after step 13, the protection shows `"Error 1003 : Internal Error."` and ends with `ExitProcess(3)`.*
- **확인됨.** `IsTNT`와 `Borland32`는 실제 `kernel32.dll`의 export가 아니다. 보호 코드가 DOS extender(Phar Lap TNT, Borland 32비트 DPMI) 위인지 가리는 조회로 보인다(**추정**). `wfapi.dll`은 Citrix ICA client의 DLL이고, 일반 Windows에는 없다.
  *Confirmed: `IsTNT` and `Borland32` are not exports of the real `kernel32.dll`; they appear to test for a DOS extender (Phar Lap TNT, Borland 32-bit DPMI) (**inferred**). `wfapi.dll` is the Citrix ICA client DLL and is absent from a normal Windows.*
- **확인됨.** WTS class 4는 `WTSSessionId`다(작업 360). Windows 실행에서는 세션 번호 0이 돌아와야 handshake로 진행했다([Hardlock runtime 분석](../analysis/ez2dj4th-hardlock-runtime.md)).
  *Confirmed: WTS class 4 is `WTSSessionId` (Task 360); on Windows the run advanced to the handshake only with session ID 0.*

이 작업은 1–12단계를 정확한 Win32 의미로 제공한다. 13단계(자기 process 메모리)는 다음 작업이다.

*This task provides steps 1–12 with accurate Win32 semantics; step 13 (own-process memory) is the next task.*

## 결정 / Decisions

### 1. 해석만 되는 export / Exports that are only resolved

`CreateCursor`, `DestroyCursor`, `SetCursor`, `GetTickCount`, `RegOpenKeyA`, `RegQueryValueExA`, `RegCloseKey`는 해석만 되고 호출되지 않았다. 추측한 동작을 넣지 않는다. 공용 `UnimplementedExport` handler를 붙여 주소는 돌려주되, 호출되면 handler가 실패해 "unhandled import"로 멈춘다. 호출이 관찰되면 그때 구현한다.

*These were resolved but never called, so no guessed behavior is added: a shared `UnimplementedExport` handler gives them an address, and a call fails the handler and stops as an unhandled import. They are implemented once a call is observed.*

`GetCurrentProcess`는 예외로 구현한다. Win32 계약상 항상 pseudo-handle `(HANDLE)-1`을 돌려주기 때문이다.

*`GetCurrentProcess` is implemented anyway, since Win32 defines it to always return the pseudo-handle `(HANDLE)-1`.*

### 2. 없는 이름 / Absent names

지금 continuation은 `GetProcAddress`가 NULL을 돌려주면 항상 멈춘다. 이 규칙은 facade가 아직 없는 export를 찾는 데 쓸모가 있다. 그러나 실제 Windows에도 없는 이름까지 막아 버린다. 그래서 "실제 Windows에도 없음"을 데이터로 선언한다.

*Continuation now always stops on a NULL `GetProcAddress`. That finds exports the facade still lacks, but it also blocks names real Windows lacks too, so "absent on real Windows" becomes declared data.*

- `GuestModuleDescriptor::absent_exports`: 실제 DLL이 export하지 않는 이름이다. `kernel32`에는 `IsTNT`, `Borland32`를 둔다.
  *`GuestModuleDescriptor::absent_exports`: names the real DLL does not export; `kernel32` lists `IsTNT` and `Borland32`.*
- `GuestModuleRegistry`의 absent module 목록: 일반 Windows에 없는 DLL이다. `wfapi.dll`을 둔다.
  *An absent-module list in `GuestModuleRegistry`: DLLs a normal Windows lacks, holding `wfapi.dll`.*
- continuation은 `GetProcAddress`나 `LoadLibraryA`가 NULL을 돌려줄 때, 요청이 absent로 선언된 이름이면 멈추지 않는다. 그 밖의 NULL은 지금처럼 경계로 멈춘다. `LoadLibraryA`의 NULL도 새로 이 규칙을 따른다.
  *Continuation does not stop on a NULL `GetProcAddress` or `LoadLibraryA` when the request is declared absent; any other NULL stops as a boundary as now, `LoadLibraryA` included.*

### 3. 게스트 process 상태 / Guest process state

`include/re2dj/hle/guest_process.h`에 플랫폼 중립 `GuestProcess`를 둔다. `ImportCallServices::Process()`(기본 null)로 handler에 제공한다. 다음 작업의 process 메모리 API도 여기에 붙인다.

*A platform-neutral `GuestProcess` in `include/re2dj/hle/guest_process.h`, provided to handlers through `ImportCallServices::Process()` (null by default); the next task's process-memory APIs attach here too.*

- **process ID.** `kGuestProcessId = 0x00000F00`로 고정한다. Windows process ID처럼 4의 배수이고, 실행마다 같다. 작업 361의 장치 handle(`0x1004`부터)과 겹치지 않는다.
  *Fixed at `kGuestProcessId = 0x00000F00`: a multiple of four like Windows process IDs, identical across runs, and distinct from Task 361's device handles (from `0x1004`).*
- **error mode.** `SetErrorMode`는 이전 값을 돌려주고 새 값을 저장한다. 시작값은 0이다.
  *`SetErrorMode` returns the previous mode and stores the new one, starting from 0.*
- **heap.** 게스트가 읽고 쓸 수 있는 4 GiB 미만 영역에 block을 할당하고 해제한다. bookkeeping(first-fit, 8 byte 정렬, 크기 기록)은 공용 코드가 맡는다. 영역을 확보하는 일은 플랫폼이 한다. Linux는 `MapNativeLowMemory`로 1 MiB를 잡는다. 할당된 block은 `ReadGuestBytes`/`WriteGuestBytes`가 허용하는 범위에 들어간다.
  *Allocates and frees blocks in a guest-addressable region below 4 GiB; the shared code does the bookkeeping (first fit, eight-byte alignment, recorded sizes) and the platform provides the region, 1 MiB through `MapNativeLowMemory` on Linux. Live blocks join the ranges `ReadGuestBytes`/`WriteGuestBytes` accept.*

### 4. `kernel32` 추가 export / New `kernel32` exports

| export | 동작 / Behavior |
| --- | --- |
| `GetCurrentProcess()` | `0xFFFFFFFF` |
| `GetCurrentProcessId()` | `kGuestProcessId` |
| `GetEnvironmentVariableA(name, buffer, size)` | 게스트 환경은 비어 있다. 0을 돌려주고 last error `ERROR_ENVVAR_NOT_FOUND`(203)를 둔다. `HL_SEARCH`가 없으면 Hardlock API는 기본 검색 순서를 쓴다. / The guest environment is empty: 0 with `ERROR_ENVVAR_NOT_FOUND` (203); without `HL_SEARCH` the Hardlock API uses its default search order. |
| `SetErrorMode(mode)` | 이전 값을 돌려주고 새 값을 저장한다. / Previous mode, new mode stored. |
| `LoadLibraryA(name)` | facade module이면 그 base(`GetModuleHandleA`와 같은 이름 규칙), 아니면 NULL과 `ERROR_MOD_NOT_FOUND`(126). / A facade module's base (same name rule as `GetModuleHandleA`), else NULL with `ERROR_MOD_NOT_FOUND` (126). |
| `FreeLibrary(module)` | facade module은 unload하지 않으므로, facade base면 TRUE다. 아니면 FALSE와 `ERROR_INVALID_HANDLE`(6)이다. / Facade modules never unload: TRUE for a facade base, else FALSE with `ERROR_INVALID_HANDLE` (6). |
| `GetVersionExA(info)` | `dwOSVersionInfoSize`가 148(`OSVERSIONINFOA`) 또는 156(`OSVERSIONINFOEXA`)이면 `GetVersion`과 같은 6.2.9200, `VER_PLATFORM_WIN32_NT`(2), 빈 `szCSDVersion`을 채운다. EX는 service pack 0, suite mask 0, `VER_NT_WORKSTATION`(1)이다. 그 밖의 크기는 FALSE와 `ERROR_INSUFFICIENT_BUFFER`(122)다. / For `dwOSVersionInfoSize` 148 (`OSVERSIONINFOA`) or 156 (`OSVERSIONINFOEXA`), fills 6.2.9200 as `GetVersion`, `VER_PLATFORM_WIN32_NT` (2), and an empty `szCSDVersion`; EX adds service pack 0, suite mask 0, and `VER_NT_WORKSTATION` (1). Other sizes: FALSE with `ERROR_INSUFFICIENT_BUFFER` (122). |
| `GetTickCount` | 해석만 / resolve only (`UnimplementedExport`) |

### 5. `advapi32`, `wtsapi32` facade

새 facade module 두 개를 추가한다. Linux 연결은 기존 `kernel32`·`user32`와 같은 방식이다.

*Two new facade modules, wired on Linux like `kernel32` and `user32`.*

- `advapi32.dll`: `RegOpenKeyA`, `RegQueryValueExA`, `RegCloseKey`. 모두 해석만 된다(`UnimplementedExport`).
  *`advapi32.dll`: `RegOpenKeyA`, `RegQueryValueExA`, `RegCloseKey`, all resolve-only (`UnimplementedExport`).*
- `wtsapi32.dll`:
  *`wtsapi32.dll`:*
  - `WTSQuerySessionInformationA(server, session, class, &buffer, &bytes)`: 서버는 `WTS_CURRENT_SERVER_HANDLE`(0), 세션은 `WTS_CURRENT_SESSION`(`0xFFFFFFFF`) 또는 0, class는 `WTSSessionId`(4)만 받는다. heap에 4 byte를 할당해 세션 번호 0을 쓰고, `*buffer`와 `*bytes = 4`를 채운 뒤 TRUE를 돌려준다. 세션 0은 Windows에서 진행을 확인한 값이다(작업 360). 그 밖의 인자는 handler 실패로 멈춘다. 이 경우 추측한 값을 돌려주지 않는다.
    *Accepts only `WTS_CURRENT_SERVER_HANDLE` (0), `WTS_CURRENT_SESSION` (`0xFFFFFFFF`) or session 0, and class `WTSSessionId` (4). It allocates four heap bytes holding session ID 0, fills `*buffer` and `*bytes = 4`, and returns TRUE. Session 0 is the value confirmed to advance on Windows (Task 360). Other arguments stop through a handler failure instead of a guessed value.*
  - `WTSFreeMemory(buffer)`: heap block을 해제한다. 반환값이 없는 함수다.
    *Frees the heap block; the function returns nothing.*

### 6. user32

`CreateCursor`, `DestroyCursor`, `SetCursor`를 `UnimplementedExport`로 추가한다.

*Adds `CreateCursor`, `DestroyCursor`, and `SetCursor` as `UnimplementedExport`.*

## 기대 결과 / Expected result

두 Linux 폭 모두 실제 4th가 1–12단계를 지나 handshake 두 번과 descriptor 한 번을 마친다. 그 뒤 `GetProcAddress(kernel32, "OpenProcess")`에서 미해석 lookup으로 멈춘다. `IsTNT`, `Borland32`, `wfapi.dll`에서는 멈추지 않는다.

*On both Linux widths the real 4th passes steps 1–12, completing two handshakes and one descriptor, and stops at the unresolved lookup `GetProcAddress(kernel32, "OpenProcess")`, without stopping at `IsTNT`, `Borland32`, or `wfapi.dll`.*

## 범위 밖 / Out of scope

- `OpenProcess`, `VirtualProtect`, `Read/WriteProcessMemory`, `VirtualAlloc/Free`: 다음 작업의 process 메모리.
  *Next task's process memory.*
- 해석만 된 export의 실제 동작. Windows 쪽 HLE는 바꾸지 않는다.
  *Behavior of resolve-only exports; no change to the Windows HLE.*
