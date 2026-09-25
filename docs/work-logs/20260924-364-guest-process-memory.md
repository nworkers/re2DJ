# 작업 364 작업 로그 — 게스트 자기 process 메모리 / Task 364 work log — Guest own-process memory

설계: [20260924-364-guest-process-memory.md](../design/20260924-364-guest-process-memory.md)
작업 지시서: [20260924-364-guest-process-memory.md](../work-orders/20260924-364-guest-process-memory.md)

## 조사 / Investigation

설계 전에 커밋하지 않은 실험 build에서 여섯 API와 `LocalAlloc`/`LocalFree`를 임시로 넣었다. 실행을 끝까지 따라가 설계의 흐름을 확인했고, 실험 코드는 되돌렸다. 이때 호출 기록이 처음 256개에서 잘려 정지 직전이 보이지 않았다. 그래서 기록 방식도 바꿨다.

*Before the design, an uncommitted experimental build supplied the six APIs plus `LocalAlloc`/`LocalFree`, following the run to its end to establish the design's flow; the experiment code was reverted. The call record, cut at the first 256 calls, hid what preceded the stop, so the record changed as well.*

## 변경 / Changes

- **`hle::GuestHandleAllocator`**(`guest_handles.h`): `0x1004`부터 4씩 나가는 공용 handle 공간이다. `GuestProcess`가 소유한다. `GuestDeviceSet::SetHandleAllocator`로 장치 handle도 이 공간을 쓴다.
  ***`hle::GuestHandleAllocator`**: the shared handle space from `0x1004` in steps of four, owned by `GuestProcess`, which device handles also use through `GuestDeviceSet::SetHandleAllocator`.*
- **`GuestProcess`**
  - process handle(`OpenProcess`, pseudo-handle 판정, 닫기). / *process handles (`OpenProcess`, pseudo-handle checks, closing);*
  - image region: loader 보호 값, 쓰기 섹션은 `WRITECOPY`. / *image regions with loader protections, `WRITECOPY` for writable sections;*
  - private arena와 region: 64 KiB 경계, page별 보호와 commit 기록. / *a private arena and regions on 64 KiB boundaries with per-page protection and commit records;*
  - `VirtualAlloc`, `VirtualFree`, `VirtualProtect`의 판단(`GuestMemoryResult`). / *the `VirtualAlloc`, `VirtualFree`, and `VirtualProtect` decisions (`GuestMemoryResult`).*
- **`kernel32`**: `OpenProcess`, `VirtualAlloc`(새 commit page를 0으로 채움), `VirtualFree`, `VirtualProtect`, `LocalAlloc`(`LMEM_FIXED`/`ZEROINIT`), `LocalFree`를 추가했다. `ReadProcessMemory`와 `WriteProcessMemory`는 해석만 된다. export는 25개다. `CloseHandle`은 process handle도 닫는다. Win32 오류 487을 추가했다.
  ***`kernel32`** adds `OpenProcess`, `VirtualAlloc` (zeroing newly committed pages), `VirtualFree`, `VirtualProtect`, `LocalAlloc` (`LMEM_FIXED`/`ZEROINIT`), and `LocalFree`, with resolve-only `ReadProcessMemory` and `WriteProcessMemory`, for 25 exports; `CloseHandle` also closes process handles; adds Win32 error 487.*
- **Linux 연결.** `NativeKernel32Diagnostic`이 16 MiB(+64 KiB) private arena를 잡는다. `DescribeImage`로 image region을 등록하고, 장치 set을 공용 handle에 연결한다. commit된 private page는 guest byte 범위에 넣는다.
  ***Linux wiring.** `NativeKernel32Diagnostic` maps a 16 MiB (+64 KiB) private arena, registers the image region through `DescribeImage`, connects the device set to the shared handles, and admits committed private pages to the guest byte ranges.*
- **호출 기록.** continuation은 처음 128개와 마지막 128개 호출을 남긴다. CLI는 그 사이를 `... N calls omitted`로 적는다.
  ***Call record.** Continuation keeps the first 128 and the last 128 calls, and the CLI prints `... N calls omitted` between them.*
- **단위 테스트.**
  ***Unit tests.***
  - 섹션 보호 값, 4th의 보호·복원 흐름. / *section protections, and 4th's open-then-restore flow;*
  - 예약·commit·범위 밖·지원 밖 모양, arena 소진. / *reserve, commit, out-of-range, and unmodelled shapes, and arena exhaustion;*
  - handle 공유, `OpenProcess`·`CloseHandle`. / *shared handles, `OpenProcess`, and `CloseHandle`;*
  - `VirtualAlloc/Protect/Free`의 오류 코드, `PAGE_GUARD` 정지. / *the `VirtualAlloc/Protect/Free` error codes, and the `PAGE_GUARD` stop;*
  - `LocalAlloc/Free`. / *`LocalAlloc/Free`.*

## 검증 / Validation

| 항목 / Item | 결과 / Result |
| --- | --- |
| Linux x64·x86 build, CTest | 경고·오류 없음, 각각 3/3 / no warnings or errors, 3/3 each |
| Linux helper, probe, 기존 진단 네 개 / diagnostics | 작업 363과 같음(trace 43 frame) / same as Task 363 (43-frame trace) |
| Windows x86 build, CTest | 오류·경고 없음, 6/6 / no errors or warnings, 6/6 |
| 실제 4th, 두 폭 / real 4th, both widths | 1,390번 호출, 주소를 정규화하면 x64와 x86이 같음. Hardlock initialize 1·handshake 2·descriptor 37·transform 36. `GetProcAddress(kernel32, "GetCurrentThreadId")`에서 정지 / 1,390 calls, identical across x64 and x86 after address normalization; Hardlock initialize 1, handshake 2, descriptor 37, transform 36; stop at `GetProcAddress(kernel32, "GetCurrentThreadId")` |

`.text` page의 첫 `VirtualProtect`는 `PAGE_EXECUTE_READ`(`0x20`)를 돌려받는다. 게스트는 그 값으로 page를 되돌린다. `.reloc`의 마지막 page는 `PAGE_WRITECOPY`(`0x08`)로 되돌아간다.

*The first `VirtualProtect` on a `.text` page returns `PAGE_EXECUTE_READ` (`0x20`), which the guest uses to restore the page; the last `.reloc` page goes back to `PAGE_WRITECOPY` (`0x08`).*

## 다음 / Next

두 번째 층은 `OpenProcess`로 자기 process를 다시 열고 같은 메모리 API를 해석한다. 그 뒤 `GetCurrentThreadId`를 찾는다.

*The second layer reopens its own process with `OpenProcess`, resolves the same memory APIs, and then looks up `GetCurrentThreadId`.*
