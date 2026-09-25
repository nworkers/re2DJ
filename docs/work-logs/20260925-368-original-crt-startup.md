# 작업 368 작업 로그 — 원본 MSVC CRT 시작 / Task 368 work log — Original MSVC CRT start-up

설계: [20260925-368-original-crt-startup.md](../design/20260925-368-original-crt-startup.md)
작업 지시서: [20260925-368-original-crt-startup.md](../work-orders/20260925-368-original-crt-startup.md)

## 진행 / Progress

정지할 때마다 해당 API를 구현하고 다시 실행했다. 중간에 세 가지를 바로잡았다.

*Each stop was implemented and the run repeated; three things were corrected along the way:*

1. **stop stub의 `INT3`가 CRT SEH로 감.** `GetStartupInfoA`에서 멈춰야 할 때 stack 주소 `0xf7836ff8`에서 fault가 났다. CRT의 `_except_handler3`(`0x004c8a5c`)가 SEH frame에 있었다. 진단 stop stub이 image base보다 높은 주소에 있어 게스트 `INT3`로 분류된 것이다. host trap 범위를 두어 해결했다.
   ***The stop stub's `INT3` reached the CRT's SEH.** What should have stopped at `GetStartupInfoA` faulted at stack address `0xf7836ff8`, with the CRT's `_except_handler3` (`0x004c8a5c`) in the SEH frame; the stub lies above the image base and was classified as a guest `INT3`. A host trap range fixed it.*
2. **CP949 값을 추측하지 않음.** CRT는 lead byte를 공백으로 바꾼 256 byte 표를 `MultiByteToWideChar(949)`로 바꾼다. 남는 비ASCII 단일 byte는 `0x80`과 `0xFF`다. 이 host에서 Win32를 직접 불러 `U+0080`, `U+F8F7`을 얻었다. 같은 측정에서 `GetStringTypeW`가 `C1_DEFINED`를 붙인다는 것도 알게 돼 처음 표를 고쳤다. 첫 측정은 PowerShell P/Invoke에서 `char`가 1 byte로 marshal되어 틀렸다. `ushort`로 다시 쟀다.
   ***CP949 values not guessed.** The CRT converts a 256-byte table, lead bytes replaced by spaces, through `MultiByteToWideChar(949)`, leaving `0x80` and `0xFF` as non-ASCII single bytes. Calling Win32 on this host gave `U+0080` and `U+F8F7`, and the same measurement showed `GetStringTypeW` adds `C1_DEFINED`, correcting the first table. The first measurement was wrong because PowerShell P/Invoke marshalled `char` as one byte; it was repeated with `ushort`.*
3. **wide 출력의 상위 byte.** `MultiByteToWideChar`가 UTF-16 단위의 하위 byte만 써서, `U+F8F7`이 `0x00F7`이 되었다. 두 byte를 모두 쓰도록 고쳤다.
   ***High byte of wide output.** `MultiByteToWideChar` wrote only each UTF-16 unit's low byte, turning `U+F8F7` into `0x00F7`; both bytes are written now.*

작업 365에서 빠진 launcher 오류 문구의 글자 `\n` 18곳도 이번에 없앴다. CLI의 Hardlock 통계 줄에 남은 `\n`도 지웠다.

*The 18 literal `\n` endings Task 365 missed in launcher error strings are gone now, as is the one left on the CLI's Hardlock counts line.*

## 변경 / Changes

- **`guest_heap.h/.cpp`**(새 파일), **`GuestProcess`**(`CreateHeap/FindHeap/DestroyHeap`, `SetMainImage`, 명령줄 주소, 필터, `kThreadId`).
  ***`guest_heap.h/.cpp`** (new) and **`GuestProcess`** (`CreateHeap/FindHeap/DestroyHeap`, `SetMainImage`, the command line address, the filter, `kThreadId`).*
- **kernel32**: 구현 export가 24개에서 51개로, 해석 전용이 72개에서 46개로 바뀌었다. `GetModuleHandleA(NULL)`은 image base를 준다. Win32 오류 8을 추가했다.
  ***kernel32:** implemented exports grow from 24 to 51 and resolve-only ones shrink from 72 to 46; `GetModuleHandleA(NULL)` returns the image base; Win32 error 8 added.*
- **`target::GuestRootPath`/`GuestExecutablePath`**, `ImportCallServices::GuestModuleName`.
- **Linux**: arena 512 MiB, `MAP_NORESERVE`(두 폭의 저주소 할당), `SetNativeHostTrapRange`, `DescribeImage`에 게스트 경로.
  ***Linux:** a 512 MiB arena, `MAP_NORESERVE` (both widths' low allocation), `SetNativeHostTrapRange`, and the guest path in `DescribeImage`.*
- **단위 테스트**(`kernel32_crt_test.cpp` 새 파일):
  ***Unit tests** (new `kernel32_crt_test.cpp`):*
  - `GuestHeap` 제자리 확장·요청 크기. / *`GuestHeap` in-place growth and requested sizes;*
  - heap export(128 MiB 예약 실패, zero, 이동하며 복사, in-place 전용 실패, process heap, NULL 해제, 예외 flag 정지). / *the heap exports (the 128 MiB reservation failing, zeroing, moving with a copy, in-place-only failure, the process heap, freeing NULL, the exception flag stopping);*
  - 시작 정보·std handle·명령줄·환경·module 경로(잘림 포함), CPU 기능, 필터. / *start-up information, std handles, the command line, the environment, module paths including truncation, CPU features, and the filter;*
  - CP949 측정값(`GetCPInfo`, 변환, lead byte 정지, `CT_CTYPE1` 8자, case mapping). / *the CP949 measurements (`GetCPInfo`, conversions, the lead-byte stop, `CT_CTYPE1` for eight characters, case mapping);*
  - 게스트 경로 규칙. / *the guest path rule.*

## 검증 / Validation

| 항목 / Item | 결과 / Result |
| --- | --- |
| Linux x64·x86 build, CTest | 경고·오류 없음, 각각 3/3 / no warnings or errors, 3/3 each |
| Linux helper, probe, 기존 진단 네 개 / diagnostics | 이전과 같음(trace 43 frame) / as before (43-frame trace) |
| Windows x86 build, CTest | 오류·경고 없음, 6/6 / no errors or warnings, 6/6 |
| 실제 4th, 두 폭 / real 4th, both widths | 호출 1,623번, 주소를 정규화하면 같음. Hardlock은 이전과 같음. CRT 시작을 마치고 `#1623 CreateEventA`(`0x00406fe3`)에서 정지 / 1,623 calls, identical after address normalization; Hardlock unchanged; CRT start-up completes and the run stops at `#1623 CreateEventA` (`0x00406fe3`) |

## 다음 / Next

게임 초기화에 필요한 kernel object부터 제공한다. event로 시작해, 이어서 thread와 대기 함수로 간다.

*Game initialization's kernel objects come next: events first, then threads and waits.*
