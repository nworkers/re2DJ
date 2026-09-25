# 작업 368 설계 — 원본 MSVC CRT 시작 / Task 368 design — Original MSVC CRT start-up

선행: [작업 367 설계](20260925-367-envelope-handoff-to-original.md)

## 배경 / Background

작업 367 뒤 실제 4th는 원본 CRT의 `HeapCreate`에서 멈췄다. 여기부터는 실험 build 없이 진행했다. 호출이 멈추면 그 API를 Win32 의미대로 구현하고 다시 실행하는 방식이다. 값이 문서만으로 분명하지 않은 곳은 이 Windows 11(한국어) host에서 Win32 API를 직접 불러 측정했다.

*After Task 367 the real 4th stopped at the original CRT's `HeapCreate`. From here the work went without an experimental build: each stop was implemented with its Win32 meaning and the run repeated. Where the documentation alone left a value open, the Win32 API was called on this Korean Windows 11 host and measured.*

관찰한 CRT 순서는 다음과 같다. 모두 `.text`(`0x004c…`)에서 부른다.

*The observed CRT order, all called from `.text` (`0x004c…`):*

`HeapCreate(1, 0x1000, 0)` → `GetVersionExA` → `HeapAlloc` → `GetStartupInfoA` → `GetStdHandle`/`GetFileType` × 3 → `SetHandleCount(32)` → `GetCommandLineA` → `GetEnvironmentStringsW` → `WideCharToMultiByte` × 2 → `FreeEnvironmentStringsW` → `GetACP` → `GetCPInfo(949)` → `GetStringTypeW`(probe) → `MultiByteToWideChar(949, 256 byte)` × 2 → `GetStringTypeW(256)` → `LCMapStringW`(LOWER/UPPER, locale 0x412) → `WideCharToMultiByte` → `GetModuleFileNameA(NULL)` → `GetProcAddress("IsProcessorFeaturePresent")` → `IsProcessorFeaturePresent(0)` → `SetUnhandledExceptionFilter` → (게임 코드 / game code) `CreateEventA`

## 결정 / Decisions

1. **heap.** `GuestHeap`이 한 region의 block 기록을 맡는다. first fit, 8 byte 정렬이고, 요청 크기를 기록해 `HeapSize`가 돌려준다. 제자리 확장도 한다. process heap(작업 363)도 `GuestHeap`이 된다. `HeapCreate`는 private arena에서 region을 잡는다. 최대 크기가 0이면 128 MiB를 예약하고, heap handle은 region base다(Win32와 같다). `HeapAlloc/Free/ReAlloc/Size/Destroy`를 구현한다. `HEAP_ZERO_MEMORY`와 `HEAP_REALLOC_IN_PLACE_ONLY`를 지원하고, `HEAP_NO_SERIALIZE`는 무시한다(guest thread가 하나). `HEAP_GENERATE_EXCEPTIONS`는 handler 실패로 멈춘다. Linux arena는 512 MiB(+64 KiB)이고, `MAP_NORESERVE`로 잡아 건드린 page만 메모리를 쓴다.
   ***Heaps:** `GuestHeap` keeps one region's blocks — first fit, eight-byte alignment, and the requested size that `HeapSize` reports — and resizes in place. The process heap (Task 363) becomes a `GuestHeap`. `HeapCreate` reserves a region from the private arena (128 MiB for maximum 0), with the region base as the heap handle, as on Win32. `HeapAlloc/Free/ReAlloc/Size/Destroy` support `HEAP_ZERO_MEMORY` and `HEAP_REALLOC_IN_PLACE_ONLY`, ignore `HEAP_NO_SERIALIZE` (one guest thread), and stop on `HEAP_GENERATE_EXCEPTIONS`. The Linux arena is 512 MiB (+64 KiB), mapped `MAP_NORESERVE` so only touched pages take memory.*
2. **process 정체.** `GuestProcess::SetMainImage`가 image base와 게스트 경로를 기록한다. 경로는 새 공용 함수 `target::GuestExecutablePath`가 만든다(`GuestRootPath`, 기본 `D:\ez2dj`). Windows launcher의 기존 규칙을 옮긴 것이고, launcher도 이 함수를 쓴다.
   ***Process identity:** `GuestProcess::SetMainImage` records the image base and guest path, built by the new shared `target::GuestExecutablePath` (`GuestRootPath`, `D:\ez2dj` by default) — the Windows launcher's existing rule, which the launcher now calls too.*
   - `GetModuleHandleA(NULL)` → image base. `GetModuleFileNameA`: NULL이나 image base면 게스트 경로, facade면 `C:\WINDOWS\system32\<dll>`을 준다. 짧은 buffer에는 잘린 문자열과 `ERROR_INSUFFICIENT_BUFFER`를 준다(XP 이후 동작).
     *`GetModuleHandleA(NULL)` → image base; `GetModuleFileNameA` gives the guest path for NULL or the image base and `C:\WINDOWS\system32\<dll>` for a facade, truncating with `ERROR_INSUFFICIENT_BUFFER` (XP and later).*
   - `GetCommandLineA`는 `"<경로>"`를 process heap에 한 번 둔다. `GetCurrentThreadId`는 `0x0F04`다.
     *`GetCommandLineA` places `"<path>"` once on the process heap; `GetCurrentThreadId` is `0x0F04`.*
3. **시작 정보.** `GetStartupInfoA`는 cb 68을 뺀 나머지를 0으로 채운다. `GetStdHandle`은 GUI process라 NULL을 준다. `GetFileType`은 `FILE_TYPE_UNKNOWN`과 `ERROR_INVALID_HANDLE`이다. `SetHandleCount`는 인자를 그대로 돌려준다.
   ***Start-up information:** `GetStartupInfoA` zeroes all but cb 68; `GetStdHandle` gives NULL for a GUI process; `GetFileType` gives `FILE_TYPE_UNKNOWN` with `ERROR_INVALID_HANDLE`; `SetHandleCount` returns its argument.*
4. **환경.** 작업 363과 같이 환경은 비어 있다. `GetEnvironmentStrings(A/W)`는 종결자만 담은 block을 부를 때마다 새로 주고, `FreeEnvironmentStrings`가 해제한다.
   ***Environment:** empty, as in Task 363. `GetEnvironmentStrings(A/W)` return a fresh terminator-only block per call, freed by `FreeEnvironmentStrings`.*
5. **code page 949.** 원본이 돌던 한국어 Windows의 값이다. 이 host에서 측정했다.
   ***Code page 949:** the Korean Windows the original ran on, measured on this host.*
   - `GetACP`와 `GetOEMCP`는 949다. `GetCPInfo`는 MaxCharSize 2, 기본 문자 `?`, lead byte `0x81–0xFE`다.
     *`GetACP`/`GetOEMCP` 949; `GetCPInfo` MaxCharSize 2, default `?`, lead bytes `0x81–0xFE`.*
   - 단일 byte 변환: ASCII는 그대로, `0x80 ↔ U+0080`, `0xFF ↔ U+F8F7`. 두 byte 문자는 handler 실패로 멈춘다(표를 추측하지 않음).
     *Single bytes: ASCII unchanged, `0x80 ↔ U+0080`, `0xFF ↔ U+F8F7`; double-byte text stops (no guessed table).*
   - `GetStringTypeW(CT_CTYPE1)`: ASCII는 C-locale 분류에 `C1_DEFINED`(0x200)를 더한 값이다. 측정한 128자 모두와 같다. U+0080은 0x220, U+F8F7은 0x200이다.
     *`GetStringTypeW(CT_CTYPE1)`: ASCII is the C-locale class plus `C1_DEFINED` (0x200), matching all 128 measured characters; U+0080 0x220, U+F8F7 0x200.*
   - `LCMapStringW/A`의 `LCMAP_LOWERCASE/UPPERCASE`는 ASCII 글자만 바꾼다. 길이를 물으면(`cchDest` 0) 길이를 준다.
     *`LCMapStringW/A` `LCMAP_LOWERCASE/UPPERCASE` change only ASCII letters, and `cchDest` 0 returns the length.*
6. **CPU 기능과 필터.** `IsProcessorFeaturePresent`는 모든 x86 host에서 답이 같은 것만 답한다. 오류·FPU 에뮬레이션은 없고, CMPXCHG8B·MMX·RDTSC·SSE·SSE2는 있다. 나머지는 멈춘다. `SetUnhandledExceptionFilter`는 필터를 기록하고 이전 값을 돌려준다. 아직 참조하는 곳은 없다.
   ***CPU features and filter:** `IsProcessorFeaturePresent` answers only features identical on every x86 host (no FDIV erratum or FPU emulation; CMPXCHG8B, MMX, RDTSC, SSE, SSE2 present) and stops on others. `SetUnhandledExceptionFilter` records the filter and returns the previous one, consulted nowhere yet.*
7. **stop stub과 게스트 SEH.** CRT가 SEH frame(`_except_handler3`)을 등록한 뒤에는, 진단 stop stub의 `INT3`가 게스트 SEH로 넘어가 stack에서 fault가 났다. `SetNativeHostTrapRange`로 host의 trap page를 표시하고, `PrepareNativeGuestBreakpointDispatch`가 그 범위를 빼도록 한다. 두 폭 공용 코드다.
   ***Stop stub and guest SEH:** once the CRT registered its SEH frame (`_except_handler3`), the diagnostic stop stub's `INT3` reached guest SEH and faulted on the stack. `SetNativeHostTrapRange` marks the host trap page and `PrepareNativeGuestBreakpointDispatch` excludes it, in code shared by both widths.*

## 기대 결과 / Expected result

두 Linux 폭에서 실제 4th가 CRT 시작을 마친다. 게임 코드의 첫 호출 `CreateEventA`(`0x00406fe3`)에서 멈춘다.

*On both Linux widths the real 4th finishes CRT start-up and stops at the game code's first call, `CreateEventA` (`0x00406fe3`).*

## 범위 밖 / Out of scope

- 두 byte CP949 표, 다른 locale 동작. / *The double-byte CP949 table and other locale behavior.*
- event, thread 등 kernel object(게임 초기화). / *Kernel objects such as events and threads (game initialization).*
- unhandled exception filter의 호출. / *Invoking the unhandled exception filter.*
