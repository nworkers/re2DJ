# 작업 371 설계 — winmm 시간과 게스트 파일 / Task 371 design — winmm timing and guest files

선행: [작업 369 설계](20260925-369-static-initializers-to-winmain.md), [작업 370 설계](20260925-370-guest-api-call-log.md)

## 배경 / Background

작업 369 뒤 실제 4th는 WinMain의 `timeBeginPeriod(1)`에서 멈췄다. 그것을 구현하자 게임이 곧바로 종료 경로(`HeapFree`, `VirtualFree`, `CloseHandle`, `SetUnhandledExceptionFilter(0)`, envelope의 `KillTimer`)로 갔다. 작업 370의 API log에서 원인이 보였다. 게임이 WinMain 초기에 `CreateFileA("D:\ez2dj\EZ2DJ.ini", GENERIC_READ, …, OPEN_EXISTING)`로 설정 파일을 여는데, Linux의 `CreateFileA`는 장치만 열어 `ERROR_FILE_NOT_FOUND`를 돌려주고 있었다. `--resolve`로 확인하니 이 파일은 CHD의 `EZ2DJ/EZ2DJ.ini`에 있다. Windows 경로는 injected runtime의 VFS가 CHD에서 이 파일을 준다.

*After Task 369 the real 4th stopped at WinMain's `timeBeginPeriod(1)`. Once that was implemented, the game went straight to its exit path (`HeapFree`, `VirtualFree`, `CloseHandle`, `SetUnhandledExceptionFilter(0)`, the envelope's `KillTimer`). Task 370's API log showed why: early in WinMain the game opens its settings with `CreateFileA("D:\ez2dj\EZ2DJ.ini", GENERIC_READ, …, OPEN_EXISTING)`, and Linux's `CreateFileA` opened only devices, answering `ERROR_FILE_NOT_FOUND`. `--resolve` shows the file at the CHD's `EZ2DJ/EZ2DJ.ini`; on Windows the injected runtime's VFS serves it from the CHD.*

## 결정 / Decisions

1. **winmm module.** `winmm.dll`을 해석 전용 목록에서 분리해 `winmm_module`로 둔다.
   ***winmm module:** `winmm.dll` leaves the resolve-only list for its own `winmm_module`.*
   - `timeBeginPeriod`/`timeEndPeriod`: 1 ms 이상이면 `TIMERR_NOERROR`, 0이면 `TIMERR_NOCANDO`(97)를 돌려준다. host 시계가 이미 1 ms보다 촘촘하기 때문이다.
     *`TIMERR_NOERROR` from 1 ms and `TIMERR_NOCANDO` (97) for 0, as the host clock is already finer than a millisecond.*
   - `timeGetTime`: 작업 369 시계의 ms(`GetTickCount`와 같은 값)를 준다.
     *the Task 369 clock's milliseconds (as `GetTickCount`).*
   - mixer export는 해석 전용으로 남긴다.
     *The mixer exports stay resolve-only.*
2. **`GuestFiles`.** 공용 게스트 파일 집합이다. Windows의 규칙을 따른다.
   ***`GuestFiles`:** the shared guest file set, following the Windows rules.*
   - **경로**: 게스트 루트(`GuestRootPath`, 기본 `D:\ez2dj`)가 현재 디렉터리다. 절대·상대·대소문자 섞인 이름을 공용 `CombineGuestPath`로 합쳐 루트 아래 상대 경로로 바꾼다. 루트 밖 경로(예: `C:\WINDOWS\…`)는 결과를 추측하지 않고 handler 실패로 멈춘다.
     ***Paths:** the guest root (`GuestRootPath`, `D:\ez2dj` by default) is the current directory; absolute, relative, and mixed-case names are combined through the shared `CombineGuestPath` into a path below the root, and a path outside it (such as `C:\WINDOWS\…`) stops the handler rather than guess.*
   - **읽기**: overlay(`overlays/<profile>/…`)에 파일이 있으면 그것을, 없으면 CHD(`<chd_root>/…`, 읽기 전용)를 읽는다.
     ***Reads:** the overlay (`overlays/<profile>/…`) when it holds the file, otherwise the CHD (`<chd_root>/…`, read-only).*
   - **쓰기**: 먼저 CHD 파일을 overlay로 복사한 뒤(copy-on-write) host 파일로 연다. CHD는 쓰지 않는다.
     ***Writes:** the CHD file is first copied into the overlay (copy on write) and the host file opened; the CHD is never written.*
   - **disposition**: `CREATE_NEW`는 이미 있으면 `ERROR_FILE_EXISTS`(80), `OPEN_EXISTING`·`TRUNCATE_EXISTING`은 없으면 `ERROR_FILE_NOT_FOUND`다. `CREATE_ALWAYS`·`OPEN_ALWAYS`는 이미 있으면 성공과 함께 `ERROR_ALREADY_EXISTS`(183)다. directory는 `ERROR_ACCESS_DENIED`다.
     ***Dispositions:** `CREATE_NEW` on an existing file gives `ERROR_FILE_EXISTS` (80); `OPEN_EXISTING`/`TRUNCATE_EXISTING` on a missing one `ERROR_FILE_NOT_FOUND`; `CREATE_ALWAYS`/`OPEN_ALWAYS` on an existing one succeed with `ERROR_ALREADY_EXISTS` (183); a directory gives `ERROR_ACCESS_DENIED`.*
   - handle은 공용 handle 공간에서 받는다. 읽기 source는 `GuestFileSource` interface로 분리한다. 운영에서는 CHD의 `Fat32Volume`, 테스트에서는 메모리 source를 쓴다.
     *Handles come from the shared handle space; the read source sits behind the `GuestFileSource` interface — the CHD's `Fat32Volume` in production, a memory source in tests.*
3. **kernel32.**
   - `CreateFileA`: 장치 prefix가 먼저다. 그다음 `GuestFiles`로 연다(`GENERIC_READ`, 그리고 `GENERIC_WRITE`/`FILE_APPEND_DATA`/`DELETE`를 쓰기로 본다).
     *`CreateFileA`: device prefixes first, then `GuestFiles` (`GENERIC_READ`, with `GENERIC_WRITE`/`FILE_APPEND_DATA`/`DELETE` as writing).*
   - `ReadFile`/`WriteFile`: 동기 방식만 다룬다. OVERLAPPED면 멈춘다. 권한이 없으면 `ERROR_ACCESS_DENIED`다.
     *`ReadFile`/`WriteFile`: synchronous only (OVERLAPPED stops), `ERROR_ACCESS_DENIED` without the right.*
   - `SetFilePointer`: 64비트 high DWORD를 지원한다. 음수 위치는 `ERROR_NEGATIVE_SEEK`(131)와 `INVALID_SET_FILE_POINTER`다.
     *`SetFilePointer`: 64-bit high DWORD support, `ERROR_NEGATIVE_SEEK` (131) with `INVALID_SET_FILE_POINTER` for a negative position.*
   - `GetFileSize`, 그리고 `CloseHandle`도 파일을 닫는다.
     *`GetFileSize`, and `CloseHandle` closing files.*
4. **실행 환경 묶음.** `RunOriginalInProcessContinuation`의 인자가 늘어서 `OriginalRunEnvironment`(장치, module 경로, 파일)로 묶는다. CLI는 CHD 실행에서만 파일 설정을 채운다. 디렉터리 dump 실행은 아직 파일을 주지 않는다.
   ***Run environment:** `RunOriginalInProcessContinuation`'s growing arguments are bundled as `OriginalRunEnvironment` (devices, module path, files); the CLI fills the file settings only for a CHD run, and a directory-dump run provides no files yet.*

## 범위 밖 / Out of scope

- 게스트 루트 밖의 경로(Windows 경로의 `windows` 지원 디렉터리 등), 디렉터리 dump 실행의 파일. / *Paths outside the guest root (such as the Windows path's `windows` support directory) and files for directory-dump runs.*
- `FindFirstFileA`, 파일 속성·시각 API. 호출이 관찰되면 한다. / *`FindFirstFileA` and file attribute/time APIs, when observed.*
- window class와 window(`LoadIconA`부터). / *Window classes and windows (from `LoadIconA`).*
