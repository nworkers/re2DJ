# 작업 364 설계 — 게스트 자기 process 메모리 / Task 364 design — Guest own-process memory

선행: [작업 363 설계](20260924-363-hardlock-api-startup-environment.md)

## 배경 / Background

작업 363 뒤 실제 4th는 `GetProcAddress(kernel32, "OpenProcess")`에서 멈췄다. 커밋하지 않은 실험 build(Linux x86)에서 필요한 export를 임시로 넣고 끝까지 따라갔다. 실험 코드는 되돌렸다. 관찰한 흐름은 다음과 같다.

*After Task 363 the real 4th stopped at `GetProcAddress(kernel32, "OpenProcess")`. An uncommitted experimental build (Linux x86) supplied the needed exports temporarily and followed the run to its end; the experiment code was reverted. The observed flow:*

1. `OpenProcess(0x38, FALSE, GetCurrentProcessId())`. `0x38`은 `PROCESS_VM_OPERATION | VM_READ | VM_WRITE`다. 그다음 `VirtualProtect`, `ReadProcessMemory`, `WriteProcessMemory`, `VirtualAlloc`, `VirtualFree`를 해석한다.
   *`OpenProcess(0x38, FALSE, GetCurrentProcessId())` (`0x38` = `PROCESS_VM_OPERATION | VM_READ | VM_WRITE`), then resolving `VirtualProtect`, `ReadProcessMemory`, `WriteProcessMemory`, `VirtualAlloc`, and `VirtualFree`.*
2. `VirtualAlloc(NULL, 0x8000, MEM_COMMIT, PAGE_READWRITE)`.
3. image의 각 page(`.text`부터 `.reloc` 끝 `0x00adf05d`까지)마다 다음을 한다.
   *For each image page, from `.text` to the end of `.reloc` at `0x00adf05d`:*
   - `VirtualProtect(page, 0x1000, PAGE_READWRITE, &old)`
   - page를 직접 고친다. / *the page is modified directly;*
   - `VirtualProtect(page, 0x1000, old, &old)`로 되돌린다. / *`VirtualProtect(page, 0x1000, old, &old)` restores it.*

   이 과정을 두 번씩 한다(page 542개, 1098번 호출). 섹션 끝의 짧은 크기(`0x22`, `0x766`, `0x5d`)도 그대로 넘긴다.
   *Each pass happens twice (542 pages, 1098 calls), passing short section tails (`0x22`, `0x766`, `0x5d`) as they are.*
4. 그 사이에 descriptor `0x44c`, `LocalAlloc(0, 0x108)`, Function `0x0e` transform `0x458`(in-place 264 byte), `LocalFree`를 반복한다.
   *In between, descriptor `0x44c`, `LocalAlloc(0, 0x108)`, Function `0x0e` transform `0x458` (in-place, 264 bytes), and `LocalFree` repeat.*
5. `VirtualFree(block, 0x8000, MEM_DECOMMIT)`. 그다음 두 번째 층이 다시 `OpenProcess`를 부르고 같은 API를 해석한 뒤 `GetProcAddress(kernel32, "GetCurrentThreadId")`에 닿는다.
   *`VirtualFree(block, 0x8000, MEM_DECOMMIT)`; a second layer then calls `OpenProcess` again, resolves the same APIs, and reaches `GetProcAddress(kernel32, "GetCurrentThreadId")`.*

- **확인됨(실험).** Hardlock 요청은 initialize 1, handshake 2, descriptor 37, transform 36이다. Windows 실행의 기록(initialize 1, handshake 2, descriptor 37, Function `0x0e` transform 36)과 같다([Hardlock runtime 분석](../analysis/ez2dj4th-hardlock-runtime.md)).
  *Confirmed (experiment): the Hardlock requests are initialize 1, handshake 2, descriptor 37, transform 36, identical to the Windows record.*
- **확인됨(실험).** `ReadProcessMemory`와 `WriteProcessMemory`는 해석만 되고 호출되지 않는다. 열린 process handle을 닫는 호출도 없다.
  *Confirmed (experiment): `ReadProcessMemory` and `WriteProcessMemory` are resolved but never called, and no call closes the process handle.*
- **확인됨(정적).** 섹션 특성은 다음과 같다. `.text` `0x60000020`(실행·읽기), `.rdata` `0x40000040`(읽기), `.data`·`.idata` `0xc0000040`(읽기·쓰기), `.reloc` `0xc2000040`, `.protect` `0xe0000020`.
  *Confirmed (static): the section characteristics are as listed.*

## 결정 / Decisions

### 1. 공용 handle 공간 / Shared handle space

Win32 handle 값은 종류와 관계없이 한 공간에서 나온다. `hle::GuestHandleAllocator`(`0x1004`부터 4씩)를 두고, `GuestProcess`가 이를 소유한다. `GuestDeviceSet::SetHandleAllocator`로 장치 handle도 같은 allocator에서 받는다. 연결하지 않은 장치 set(단위 테스트)은 지금처럼 자기 allocator를 쓴다. 따라서 장치 handle 값은 지금과 같다.

*Win32 handle values come from one space whatever their kind: `hle::GuestHandleAllocator` (from `0x1004` in steps of four) is owned by `GuestProcess`, and `GuestDeviceSet::SetHandleAllocator` makes device handles come from it too. An unconnected device set (unit tests) keeps its own, so device handle values do not change.*

- `OpenProcess(access, inherit, pid)`: `pid == kProcessId`면 새 process handle을 돌려준다. 아니면 NULL과 `ERROR_INVALID_PARAMETER`(87)다. 자기 process이므로 access는 모두 허용한다.
  *For `pid == kProcessId` returns a new process handle, else NULL with `ERROR_INVALID_PARAMETER` (87); every access right is granted for the own process.*
- `CloseHandle`: 장치 handle, 그다음 process handle 순으로 닫는다. 둘 다 아니면 지금처럼 `ERROR_INVALID_HANDLE`이다.
  *Closes a device handle, then a process handle, otherwise `ERROR_INVALID_HANDLE` as now.*

### 2. 게스트 가상 메모리 기록 / Guest virtual memory records

`GuestProcess`가 region과 page별 상태(보호 값, commit 여부)를 기록한다. page 크기는 `0x1000`이다.

*`GuestProcess` records regions and per-page state (protection, committed), with `0x1000` pages.*

- **image region.** 실행 준비 때 PE 정보로 등록한다. header는 `PAGE_READONLY`다. 섹션은 Windows loader처럼 특성에서 보호 값을 정한다. 쓰기가 있는 섹션은 copy-on-write라서, 쓰기가 있으면 `PAGE_WRITECOPY`(`0x08`), 실행까지 있으면 `PAGE_EXECUTE_WRITECOPY`(`0x80`)다. 나머지는 실행·읽기 조합대로 정한다(`0x20`, `0x02`, `0x10`, 없으면 `PAGE_NOACCESS`).
  ***Image region**, registered from the PE information while preparing the run: headers `PAGE_READONLY`; sections take the Windows loader's protection from their characteristics — writable sections are copy-on-write, `PAGE_WRITECOPY` (`0x08`) or `PAGE_EXECUTE_WRITECOPY` (`0x80`) with execute, and the others follow execute/read (`0x20`, `0x02`, `0x10`, otherwise `PAGE_NOACCESS`).*
- **private region.** `VirtualAlloc`이 플랫폼이 준 영역(Linux: 4 GiB 미만 16 MiB)에서 64 KiB 경계로 예약한다. 크기는 page 단위로 올린다.
  ***Private regions**: `VirtualAlloc` reserves at 64 KiB boundaries in the platform-provided arena (16 MiB below 4 GiB on Linux), sizes rounded up to pages.*
- **host 보호.** host의 실제 보호는 바꾸지 않는다. image는 지금처럼 RWX로 map되어 있고, private 영역은 RW다. 보호 값은 기록만 하고 반환에 쓴다. 게스트가 access fault를 기대하는지는 **미확정**이다. 관찰된 흐름은 fault를 쓰지 않는다.
  ***Host protection** is not changed: the image stays mapped RWX as now and the arena RW; protections are recorded and reported only. Whether the guest ever expects an access fault is **unresolved**; the observed flow uses none.*

| export | 동작 / Behavior |
| --- | --- |
| `VirtualAlloc(address, size, type, protect)` | `address == 0`이면 `MEM_RESERVE`/`MEM_COMMIT` 조합으로 새 region을 만든다. `address != 0`에 `MEM_COMMIT`이면 기존 private 예약 안의 page를 commit한다. 새로 commit한 page는 0으로 채운다. 크기 0, 잘못된 보호 값, private 영역의 `WRITECOPY`는 `ERROR_INVALID_PARAMETER`(87)이다. 예약 밖 주소는 `ERROR_INVALID_ADDRESS`(487)이다. 그 밖의 flag(`MEM_TOP_DOWN` 등)와 지정 주소 예약은 handler 실패로 멈춘다. / With `address == 0`, a new region per `MEM_RESERVE`/`MEM_COMMIT`; with `address != 0` and `MEM_COMMIT`, commits pages inside an existing private reservation. Newly committed pages are zeroed. Zero size, an invalid protection, or `WRITECOPY` on private memory: `ERROR_INVALID_PARAMETER` (87); an address outside any reservation: `ERROR_INVALID_ADDRESS` (487). Other flags (`MEM_TOP_DOWN`, …) and reservations at a given address stop through a handler failure. |
| `VirtualFree(address, size, type)` | `MEM_RELEASE`는 `size == 0`이고 주소가 region 시작일 때만 된다. `MEM_DECOMMIT`은 private region 안의 page를 decommit한다(`size == 0`이면 region 전체). image region은 `ERROR_INVALID_PARAMETER`다. / `MEM_RELEASE` only with `size == 0` at a region start; `MEM_DECOMMIT` decommits pages inside a private region (all of it for `size == 0`); image regions give `ERROR_INVALID_PARAMETER`. |
| `VirtualProtect(address, size, protect, &old)` | 범위의 모든 page가 한 region 안에서 commit되어 있어야 한다. 그렇지 않으면 `ERROR_INVALID_ADDRESS`다. `old`에는 첫 page의 보호 값을 쓰고, 범위의 page에 새 값을 기록한다. `size == 0`이나 잘못된 보호 값은 87, `old == NULL`은 `ERROR_NOACCESS`(998)다. `PAGE_GUARD` 같은 수식 bit는 handler 실패로 멈춘다. / Every page in the range must be committed within one region, else `ERROR_INVALID_ADDRESS`; writes the first page's protection to `old` and records the new one for the range. `size == 0` or an invalid protection gives 87 and `old == NULL` gives `ERROR_NOACCESS` (998); modifier bits such as `PAGE_GUARD` stop through a handler failure. |
| `LocalAlloc(flags, size)` | `LMEM_FIXED`(0)와 `LMEM_ZEROINIT`(`0x40`)만 받는다. 작업 363의 heap에서 할당하고, `ZEROINIT`이면 0으로 채운다. `LMEM_MOVEABLE`은 handler 실패로 멈춘다. / `LMEM_FIXED` (0) and `LMEM_ZEROINIT` (`0x40`) only, from Task 363's heap, zeroed for `ZEROINIT`; `LMEM_MOVEABLE` stops through a handler failure. |
| `LocalFree(block)` | 성공하면 NULL이다. `NULL`을 넘기면 NULL이다. 모르는 block이면 그 값을 그대로 돌려주고 `ERROR_INVALID_HANDLE`이다. / NULL on success or for `NULL`; an unknown block comes back unchanged with `ERROR_INVALID_HANDLE`. |
| `ReadProcessMemory`, `WriteProcessMemory` | 해석만 된다(`UnimplementedExport`). / Resolve-only. |

private region의 commit된 page는 `ReadGuestBytes`/`WriteGuestBytes`의 허용 범위에 든다.

*Committed private pages join the `ReadGuestBytes`/`WriteGuestBytes` ranges.*

### 3. 호출 기록의 앞과 끝 / Head and tail of the call log

실행은 이제 1,390번 호출에서 멈춘다. 지금의 "처음 256개" 기록으로는 정지 직전을 볼 수 없다. continuation 기록을 처음 128개와 마지막 128개로 바꾼다. CLI는 그 사이에 생략한 호출 수를 한 줄로 적는다.

*A run now stops after 1,390 calls, and the current "first 256" record cannot show what precedes the stop. The continuation record becomes the first 128 and the last 128 calls, with the CLI printing one line for the calls omitted between them.*

## 기대 결과 / Expected result

두 Linux 폭에서 실제 4th의 Hardlock 요청이 initialize 1, handshake 2, descriptor 37, transform 36이 된다(Windows와 같음). 실행은 `GetProcAddress(kernel32, "GetCurrentThreadId")`에서 멈춘다.

*On both Linux widths the real 4th's Hardlock requests become initialize 1, handshake 2, descriptor 37, transform 36 (as on Windows), and the run stops at `GetProcAddress(kernel32, "GetCurrentThreadId")`.*

## 범위 밖 / Out of scope

- host 보호의 실제 적용, guard page.
  *Enforcing protection on the host, and guard pages.*
- `ReadProcessMemory`/`WriteProcessMemory`의 동작, 다른 process.
  *The behavior of `ReadProcessMemory`/`WriteProcessMemory`, and other processes.*
- 두 번째 층 이후(`GetCurrentThreadId`부터).
  *Anything past the second layer (from `GetCurrentThreadId` on).*
