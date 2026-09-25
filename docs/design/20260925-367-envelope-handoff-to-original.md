# 작업 367 설계 — envelope의 두 번째 층과 원본 진입 / Task 367 design — Envelope second layer and hand-off to the original

선행: [작업 364 설계](20260924-364-guest-process-memory.md), [작업 343 설계](20260922-343-kernel32-linux-i386-facade.md)

## 배경 / Background

작업 364 뒤 실제 4th는 두 번째 층의 `GetProcAddress(kernel32, "GetCurrentThreadId")`에서 멈췄다. 커밋하지 않은 실험 build로 다음 흐름을 확인했다. 이 build는 파일에서 해석 전용 export를 읽었다. 경계에서 복호화된 image를 scratchpad에 dump해 원본 `.idata`를 오프라인으로 읽었다. dump는 원본 자산의 사본이라 저장소에 넣지 않았다.

*After Task 364 the real 4th stopped in the second layer at `GetProcAddress(kernel32, "GetCurrentThreadId")`. An uncommitted experimental build established the following flow. It read resolve-only exports from a file and dumped the decrypted image at the boundary to the scratchpad, so the original `.idata` could be read offline; the dump is a copy of an original asset and stays out of the repository.*

1. **Hardlock API의 남은 해석.** `GetCurrentThreadId`, `Sleep`, `GetTickCount`, `ExitProcess`, `SetTimer`, `KillTimer`, `GetModuleHandleA`, `LoadLibraryA`를 해석한다. 이어 `VirtualAlloc` 두 번(`0x284` RW, `0x508` RWX)을 부른다.
   ***The Hardlock API's remaining lookups:** `GetCurrentThreadId`, `Sleep`, `GetTickCount`, `ExitProcess`, `SetTimer`, `KillTimer`, `GetModuleHandleA`, `LoadLibraryA`, then two `VirtualAlloc` calls (`0x284` RW, `0x508` RWX).*
2. **원본 import 표 재구성.** `.idata`(`0x6d1000`)의 descriptor를 따라 `GetModuleHandleA`와 `GetProcAddress`로 해석한다. 대상은 kernel32 71개, user32 31개, GDI32 12개, ADVAPI32 1개, WINMM 10개, DSOUND ordinal 1, DINPUT 1개, DDRAW 2개, AVIFIL32 5개, WS2_32 ordinal 9개다. 처리한 kernel32·user32 descriptor의 이름 표는 0으로 지운다(dump 방지로 **추정**).
   ***Original import table rebuild:** following the `.idata` (`0x6d1000`) descriptors through `GetModuleHandleA` and `GetProcAddress`: kernel32 71, user32 31, GDI32 12, ADVAPI32 1, WINMM 10, DSOUND ordinal 1, DINPUT 1, DDRAW 2, AVIFIL32 5, WS2_32 9 ordinals. The name tables of processed kernel32 and user32 descriptors are zeroed (anti-dump, **inferred**).*
3. **`ExitProcess` hook.** `VirtualProtect(ExitProcess thunk, 5, PAGE_READWRITE)` 뒤 thunk를 고치고 `PAGE_EXECUTE_READ`로 되돌린다. 이 과정을 두 번 한다. 보호 변경이 실패하면 `ReadProcessMemory`로 5 byte를 읽는 경로로 간다.
   ***`ExitProcess` hook:** `VirtualProtect(ExitProcess thunk, 5, PAGE_READWRITE)`, a patch, and a restore to `PAGE_EXECUTE_READ`, twice; when the protection change fails, it takes a path that reads the five bytes with `ReadProcessMemory`.*
4. **`SetTimer(NULL, 0, 0x8000, 0x00aeaddb)`.** 32.768초 주기의 thread timer다.
   *A thread timer with a 32.768-second period.*
5. **원본 진입.** `.text` 안의 `0x004c4424`에서 `GetVersion`을 부르고, 이어 `HeapCreate(1, 0x1000, 0)`를 부른다. MSVC CRT 시작 순서이므로, envelope가 원본 프로그램에 제어를 넘긴 것이다.
   ***Hand-off to the original:** `GetVersion` from `0x004c4424` inside `.text`, then `HeapCreate(1, 0x1000, 0)` — the MSVC CRT start-up order, so the envelope has passed control to the original program.*

## 결정 / Decisions

1. **해석 전용 export.** 관찰된 이름을 Win32 인자 수와 함께 `UnimplementedExport`로 둔다. 공용 `ResolveOnlyExport` 표와 `AddResolveOnlyExports`가 이 일을 한다. kernel32(72개, `GetTickCount` 포함), user32(34개, cursor 세 개 포함, `wsprintfA`는 가변 cdecl), advapi32(`RegFlushKey` 추가)에 붙인다. 새 facade 일곱 개(`gdi32`, `winmm`, `dsound`, `dinput`, `ddraw`, `avifil32`, `ws2_32`)는 `MakeResolveOnlyModuleDescriptors`가 만든다. `DirectSoundCreate`(1)와 ws2_32는 ordinal을 가진다.
   ***Resolve-only exports:** the observed names become `UnimplementedExport` with their Win32 argument counts, through a shared `ResolveOnlyExport` table and `AddResolveOnlyExports`: kernel32 (72, including `GetTickCount`), user32 (34, including the three cursor exports; `wsprintfA` is variadic cdecl), and advapi32 (adding `RegFlushKey`). `MakeResolveOnlyModuleDescriptors` builds the seven new facades (`gdi32`, `winmm`, `dsound`, `dinput`, `ddraw`, `avifil32`, `ws2_32`), with ordinals for `DirectSoundCreate` (1) and ws2_32.*
2. **facade도 image다.** Linux 진단은 각 facade module을 PE header에서 읽은 섹션과 함께 `GuestProcess`의 image region으로 등록한다. 그래서 `VirtualProtect`가 Windows의 system DLL과 같이 facade page에도 된다.
   ***Facades are images:** the Linux diagnostic registers each facade module as a `GuestProcess` image region with the sections read from its PE header, so `VirtualProtect` works on facade pages as it does on a system DLL.*
3. **facade code는 host에서 쓰기 가능.** 작업 343은 facade를 R/RX로 두고 W+X를 만들지 않았다. 이번에는 code section을 RWX로 바꾼다. header와 export 표는 R로 둔다. 모든 facade thunk가 한 page를 나누어 쓰기 때문이다. 게스트가 잠시 `PAGE_READWRITE`로 둔 page를 host에 적용하면, 그 사이 같은 page의 다른 thunk 호출이 fault난다. Windows에서는 다른 DLL page라 멀쩡한 호출이다. 따라서 보호 값은 작업 364처럼 기록만 한다. probe는 "R header, RWX code"를 확인한다.
   ***Facade code writable on the host:** Task 343 kept facades R/RX without W+X; code sections now become RWX while headers and export tables stay R. Every facade thunk shares one page, and applying the guest's temporary `PAGE_READWRITE` there would fault calls to other thunks on it that Windows serves from other DLL pages, so protections stay recorded only, as in Task 364. The probe checks "R headers, RWX code".*
4. **`ReadProcessMemory`/`WriteProcessMemory`.** 자기 process(pseudo-handle 또는 열린 handle)만 다룬다. 대상은 한 region 안에서 commit되고 `PAGE_NOACCESS`가 아닌 page여야 한다(`GuestProcess::Accessible`). 쓰기는 기록된 보호가 읽기 전용이어도 된다. Windows kernel32의 `WriteProcessMemory`가 필요하면 보호를 잠시 바꿔 쓰기 때문이다(ReactOS `kernel32` 구현 기준, **추정**). 실패하면 아무것도 복사하지 않고 `ERROR_PARTIAL_COPY`(299)다. 모르는 handle은 `ERROR_INVALID_HANDLE`이다. guest byte 허용 범위는 이제 `Accessible` 전체다.
   ***`ReadProcessMemory`/`WriteProcessMemory`:** own process only (pseudo-handle or an open handle), on pages committed within one region and not `PAGE_NOACCESS` (`GuestProcess::Accessible`). A write goes through even when the recorded protection is read-only, since Windows' kernel32 `WriteProcessMemory` changes the protection for it when needed (per ReactOS's `kernel32`, **inferred**). A failure copies nothing and reports `ERROR_PARTIAL_COPY` (299); an unknown handle gives `ERROR_INVALID_HANDLE`. Guest byte access now covers all of `Accessible`.*
5. **`SetTimer`.** window가 없는 thread timer만 받는다. 새 ID(1부터) 또는 같은 ID의 교체로 기록하고, 주기는 `USER_TIMER_MINIMUM`/`MAXIMUM`으로 자른다. Windows에서 이 timer는 thread의 message loop가 `WM_TIMER`를 dispatch할 때만 돈다. message loop가 아직 없으므로 기록만 하고 전달하지 않는다. window timer는 window service가 없어서 handler 실패로 멈춘다. 실제 Windows timer ID 값과의 일치는 **미확정**이다. 게스트는 이 값을 `KillTimer`에 돌려줄 뿐이다.
   ***`SetTimer`:** thread timers without a window only, recorded under a new ID (from 1) or by replacing an existing ID, with the period clamped to `USER_TIMER_MINIMUM`/`MAXIMUM`. On Windows such a timer runs only when the thread's message loop dispatches `WM_TIMER`; with no message loop yet it is recorded and not delivered. Window timers stop through a handler failure for lack of a window service. Whether the IDs match Windows' values is **unresolved**; the guest only passes the ID back to `KillTimer`.*

## 기대 결과 / Expected result

두 Linux 폭에서 실제 4th가 envelope를 끝낸다. 원본 CRT의 `GetVersion`을 지나 `HeapCreate`에서 "unhandled import"로 멈춘다.

*On both Linux widths the real 4th finishes the envelope, passes the original CRT's `GetVersion`, and stops at `HeapCreate` as an unhandled import.*

## 범위 밖 / Out of scope

- 원본 CRT와 게임 초기화(`HeapCreate`부터). / *The original CRT and game initialization (from `HeapCreate`).*
- timer 전달과 message loop. / *Timer delivery and the message loop.*
- `ExitProcess` hook이 실제로 어떤 코드로 이어지는지. 종료 경로에서 확인한다. / *Where the `ExitProcess` hook actually leads, to be seen on the exit path.*
