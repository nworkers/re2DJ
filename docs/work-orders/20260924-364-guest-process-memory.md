# 작업 364 작업 지시서 — 게스트 자기 process 메모리 / Task 364 work order — Guest own-process memory

설계: [20260924-364-guest-process-memory.md](../design/20260924-364-guest-process-memory.md)

## 절차 / Steps

1. `hle::GuestHandleAllocator`를 추가한다. `GuestProcess`가 소유하고, `GuestDeviceSet::SetHandleAllocator`로 공유한다.
   *Add `hle::GuestHandleAllocator`, owned by `GuestProcess` and shared through `GuestDeviceSet::SetHandleAllocator`.*
2. `GuestProcess`에 process handle, image·private region, page별 보호·commit 기록, `VirtualAlloc/Free/Protect` 판단을 추가한다.
   *Add process handles, image and private regions, per-page protection and commit records, and the `VirtualAlloc/Free/Protect` decisions to `GuestProcess`.*
3. `kernel32`에 `OpenProcess`, `VirtualAlloc`, `VirtualFree`, `VirtualProtect`, `LocalAlloc`, `LocalFree`, `ReadProcessMemory`·`WriteProcessMemory`(해석만)를 추가한다. `CloseHandle`은 process handle도 닫는다.
   *Add `OpenProcess`, `VirtualAlloc`, `VirtualFree`, `VirtualProtect`, `LocalAlloc`, `LocalFree`, and resolve-only `ReadProcessMemory`/`WriteProcessMemory` to `kernel32`; `CloseHandle` also closes process handles.*
4. Linux 진단은 다음 일을 한다. / *The Linux diagnostic:*
   - 16 MiB private 영역을 잡는다. / *maps a 16 MiB private arena;*
   - PE 정보로 image region을 등록한다. / *registers the image region from the PE information;*
   - 장치 set을 공용 handle에 연결한다. / *connects the device set to the shared handles;*
   - commit된 private page를 guest byte 범위에 넣는다. / *admits committed private pages to the guest byte ranges.*
5. continuation 기록을 처음 128개와 마지막 128개로 바꾸고, CLI에 생략 줄을 추가한다.
   *Change the continuation record to the first 128 and last 128 calls, with an omission line in the CLI.*
6. 단위 테스트, 분석·TODO·ARCHITECTURE 갱신.
   *Unit tests, and analysis/TODO/ARCHITECTURE updates.*

## 완료 조건 / Done when

- Linux x64·x86 build와 CTest, 기존 진단·probe, Windows x86 build와 CTest 6/6이 모두 통과한다.
  *Linux x64/x86 build and CTest, existing diagnostics and probes, and Windows x86 build and CTest 6/6 all pass.*
- 실제 4th가 두 폭에서 initialize 1, handshake 2, descriptor 37, transform 36을 마치고 `GetProcAddress(kernel32, "GetCurrentThreadId")`에서 멈춘다.
  *On both widths the real 4th completes initialize 1, handshake 2, descriptor 37, and transform 36 and stops at `GetProcAddress(kernel32, "GetCurrentThreadId")`.*
