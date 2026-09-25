# 작업 363 작업 지시서 — Hardlock API 시작 환경 / Task 363 work order — Hardlock API startup environment

설계: [20260924-363-hardlock-api-startup-environment.md](../design/20260924-363-hardlock-api-startup-environment.md)

## 절차 / Steps

1. `hle/guest_process.h/.cpp`에 `GuestProcess`(process ID, error mode, heap)를 추가하고 `ImportCallServices::Process()`를 둔다.
   *Add `GuestProcess` (process ID, error mode, heap) in `hle/guest_process.h/.cpp` and `ImportCallServices::Process()`.*
2. `GuestModuleDescriptor::absent_exports`, registry의 absent module 목록과 조회를 추가한다. 공용 `UnimplementedExport` handler를 둔다.
   *Add `GuestModuleDescriptor::absent_exports`, the registry's absent-module list and queries, and a shared `UnimplementedExport` handler.*
3. `kernel32`에 설계 4절의 export와 absent 이름을, `user32`에 cursor 세 개를 추가한다.
   *Add design §4's exports and absent names to `kernel32`, and the three cursor exports to `user32`.*
4. `advapi32`, `wtsapi32` module descriptor를 추가하고 Linux 진단에 등록한다.
   *Add the `advapi32` and `wtsapi32` descriptors and register them in the Linux diagnostic.*
5. Linux 진단이 heap 영역(1 MiB)을 잡아 `GuestProcess`로 제공하고, 할당된 block을 guest byte 허용 범위에 넣는다.
   *The Linux diagnostic maps a 1 MiB heap region, provides it through `GuestProcess`, and admits live blocks to the guest byte ranges.*
6. continuation은 absent로 선언된 이름에 대한 NULL `GetProcAddress`·`LoadLibraryA`에서 멈추지 않는다. 그 밖의 NULL `LoadLibraryA`는 미해석 lookup으로 멈춘다.
   *Continuation does not stop on a NULL `GetProcAddress`/`LoadLibraryA` for a declared-absent name; any other NULL `LoadLibraryA` stops as an unresolved lookup.*
7. 단위 테스트: `GuestProcess` heap, 새 `kernel32` export, `wtsapi32`, absent 조회.
   *Unit tests: the `GuestProcess` heap, the new `kernel32` exports, `wtsapi32`, and absent lookups.*
8. 분석 문서, TODO, ARCHITECTURE를 갱신한다.
   *Update the analysis, TODO, and ARCHITECTURE.*

## 완료 조건 / Done when

- Linux x64·x86 build에 경고가 없고 CTest가 통과한다. Windows x86 build와 CTest가 6/6이다.
  *Linux x64/x86 build without warnings and pass CTest; the Windows x86 build and CTest pass 6/6.*
- 기존 Linux 진단과 probe의 결과가 그대로다.
  *Existing Linux diagnostics and probes are unchanged.*
- 실제 4th가 두 폭 모두 handshake 두 번과 descriptor 한 번을 마치고 `GetProcAddress(kernel32, "OpenProcess")`에서 멈춘다. `hardlock.ini` 값은 출력하지 않는다.
  *On both widths the real 4th completes two handshakes and one descriptor and stops at `GetProcAddress(kernel32, "OpenProcess")`, with no `hardlock.ini` values printed.*
