# 작업 371 작업 로그 — winmm 시간과 게스트 파일 / Task 371 work log — winmm timing and guest files

설계: [20260926-371-winmm-timing-and-guest-files.md](../design/20260926-371-winmm-timing-and-guest-files.md)
작업 지시서: [20260926-371-winmm-timing-and-guest-files.md](../work-orders/20260926-371-winmm-timing-and-guest-files.md)

## 진행 / Progress

`timeBeginPeriod`를 구현하자 게임이 종료 경로로 갔다. 작업 370의 API log에서 `#1703 CreateFileA("D:\ez2dj\EZ2DJ.ini") -> ffffffff, last_error <- 2`를 확인했다. CLI의 `--resolve`로 이 파일이 CHD의 `EZ2DJ/EZ2DJ.ini`에 있음을 확인하고 게스트 파일을 구현했다. 첫 build는 `RunOriginalInProcessContinuation` 안에 남은 `devices` 참조로 실패했다. 실행 script의 build 오류 필터가 `error:`가 아닌 `error`를 찾아 알아보기 어려웠고, 이것도 고쳤다.

*With `timeBeginPeriod` implemented the game went to its exit path; Task 370's API log showed `#1703 CreateFileA("D:\ez2dj\EZ2DJ.ini") -> ffffffff, last_error <- 2`. The CLI's `--resolve` found the file at the CHD's `EZ2DJ/EZ2DJ.ini`, and guest files followed. The first build failed on a leftover `devices` reference in `RunOriginalInProcessContinuation`, hard to spot because the run script's build filter matched `error` instead of `error:`; that filter was fixed too.*

## 변경 / Changes

- **`winmm_module.h/.cpp`**(새 파일): `timeBeginPeriod`, `timeEndPeriod`, `timeGetTime`, mixer 7개(해석 전용).
  ***`winmm_module.h/.cpp`** (new): `timeBeginPeriod`, `timeEndPeriod`, `timeGetTime`, and seven resolve-only mixer exports.*
- **`guest_files.h/.cpp`**(새 파일): `GuestFiles`, `GuestFileSource`, CHD adapter.
  ***`guest_files.h/.cpp`** (new): `GuestFiles`, `GuestFileSource`, and the CHD adapter.*
- **kernel32**: `CreateFileA`가 파일을 연다. `ReadFile`, `WriteFile`, `SetFilePointer`, `GetFileSize`를 구현했다(구현 64개, 해석 전용 33개). `CloseHandle`은 파일도 닫는다.
  ***kernel32:** `CreateFileA` opens files; `ReadFile`, `WriteFile`, `SetFilePointer`, and `GetFileSize` are implemented (64 implemented, 33 resolve-only); `CloseHandle` closes files.*
- **서비스·기록**: `ImportCallServices::Files()`. 기록 장식자도 이것을 넘긴다.
  ***Services and record:** `ImportCallServices::Files()`, forwarded by the recording decorator.*
- **Linux·CLI**: `OriginalRunEnvironment`, `NativeKernel32Diagnostic::ConfigureFiles`. CHD 실행이 `overlays/<profile>`을 쓴다.
  ***Linux and CLI:** `OriginalRunEnvironment` and `NativeKernel32Diagnostic::ConfigureFiles`; a CHD run uses `overlays/<profile>`.*
- **단위 테스트**(`guest_files_test.cpp`): 메모리 source로 다음을 검사한다. / ***Unit tests** (`guest_files_test.cpp`) over a memory source:*
  - 경로 대응(절대·대소문자·상대), 크기·seek·끝 읽기·음수 seek. / *path mapping (absolute, case-folded, relative); size, seek, reading at the end, a negative seek;*
  - 권한, 없는 파일, `CREATE_NEW` 충돌, directory, 루트 밖. / *access rights, a missing file, a `CREATE_NEW` clash, a directory, and outside the root;*
  - copy-on-write와 이후 overlay 읽기, `CREATE_ALWAYS` 절단과 `ERROR_ALREADY_EXISTS`, 새 파일 생성. / *copy on write with later overlay reads, `CREATE_ALWAYS` truncation with `ERROR_ALREADY_EXISTS`, and a new file;*
  - kernel32 파일 export. / *the kernel32 file exports.*

  해석 전용 module 테스트는 winmm가 빠진 구성(6개 DLL, 30개)으로 바꿨다.
  *The resolve-only module test now expects the set without winmm (six DLLs, 30 exports).*

## 검증 / Validation

| 항목 / Item | 결과 / Result |
| --- | --- |
| Linux x64·x86 build, CTest | 경고·오류 없음, 각각 3/3 / no warnings or errors, 3/3 each |
| Linux helper, probe, 기존 진단 네 개 / diagnostics | 이전과 같음 / as before |
| Windows x86 build, CTest | 오류·경고 없음, 6/6 / no errors or warnings, 6/6 |
| 실제 4th, 두 폭 / real 4th, both widths | 호출 1,707번, 주소를 정규화하면 같음. `CreateFileA("D:\ez2dj\EZ2DJ.ini")` → `GetFileSize` 539 byte → `ReadFile` → `CloseHandle` 뒤 `#1707 LoadIconA`에서 정지. 읽기만 했으므로 overlay는 바뀌지 않음 / 1,707 calls, identical after address normalization; `CreateFileA("D:\ez2dj\EZ2DJ.ini")` → `GetFileSize` 539 bytes → `ReadFile` → `CloseHandle`, then a stop at `#1707 LoadIconA`; reads only, so the overlay is unchanged |

## 다음 / Next

window class와 window다. `LoadIconA`, `LoadCursorA`, `RegisterClassA`, `CreateWindowExA`와 message loop 순서로 간다.

*Window classes and windows: `LoadIconA`, `LoadCursorA`, `RegisterClassA`, `CreateWindowExA`, and the message loop.*
