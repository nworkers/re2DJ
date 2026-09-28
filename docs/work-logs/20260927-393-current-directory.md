# 작업 393 작업 로그 — 현재 디렉터리 / Task 393 work log — the current directory

설계: [20260927-393-current-directory.md](../design/20260927-393-current-directory.md)
작업 지시서: [20260927-393-current-directory.md](../work-orders/20260927-393-current-directory.md)

## 진행 / Progress

측정 스크립트에서 처음 넣었던 `GetCurrentDirectoryA(300, NULL)`은 32비트 PowerShell 프로세스를 access violation으로 끝냈다. 그 결과를 측정값으로 기록하고, 그 줄을 빼고 다시 측정했다.

*The first measuring script's `GetCurrentDirectoryA(300, NULL)` ended the 32-bit PowerShell process with an access violation. That outcome is recorded as a measurement, and the rest was measured again without that line.*

두 export를 더하자 Linux의 4th는 Windows VFS 기록과 같은 순서로 현재 디렉터리를 옮겼다.

| 호출 / Call | 결과 / Result |
| --- | --- |
| `#22723 GetCurrentDirectoryA` | 8 (`D:\ez2dj`) |
| `#22724 SetCurrentDirectoryA("System\Common")` | TRUE |
| `#23005 SetCurrentDirectoryA("D:\ez2dj")` | TRUE |
| `#23009 SetCurrentDirectoryA("System\AmuseLogo")` | TRUE |

그 사이에 다음 장면의 텍스처(`www.abm` 등)를 이름만으로 열어 표면에 올렸다. 그 뒤 CRT가 `aw-logo.ezw`를 여는 과정에서 `GetFileType`이 파일 handle에 0(`FILE_TYPE_UNKNOWN`)과 last error 6을 돌려주었다. CRT는 handle을 닫고 실패를 돌려주었고, 게임은 결과를 확인하지 않고 쓰다가 `EIP 0x004724d3`에서 SIGSEGV로 멈췄다.

*With the two exports the 4th on Linux moved its current directory in the same order as the Windows VFS trace (above), opening the next scene's textures (`www.abm` and others) by bare name in between. Then, as the CRT opened `aw-logo.ezw`, `GetFileType` answered 0 (`FILE_TYPE_UNKNOWN`) with last error 6 for the file handle; the CRT closed the handle and failed, and the game used the result unchecked and stopped with SIGSEGV at `EIP 0x004724d3`.*

kernel32 descriptor 테스트는 export 순서를 고정해 확인한다. 그래서 새 export는 구현 목록 끝에 붙였다. 구현 67개, 해석 전용 30개이며 합계 97개는 그대로다.

*The kernel32 descriptor test checks the export order, so the new exports were appended to the end of the implemented list: 67 implemented and 30 resolve-only, still 97 in all.*

## 변경 / Changes

- **HLE**:
  - `GuestFiles::CurrentDirectory`, `SetCurrentDirectory`.
  - 상대 경로가 현재 디렉터리를 기준으로 풀린다.
  - `kWin32ErrorPathNotFound`(3), `kWin32ErrorDirectory`(267).

  ***HLE:***
  - *`GuestFiles::CurrentDirectory` and `SetCurrentDirectory`.*
  - *Relative paths resolve against the current directory.*
  - *`kWin32ErrorPathNotFound` (3) and `kWin32ErrorDirectory` (267).*
- **kernel32**: `GetCurrentDirectoryA`, `SetCurrentDirectoryA`. 해석 전용 목록에서 옮겼다. / ***kernel32:** `GetCurrentDirectoryA` and `SetCurrentDirectoryA`, moved out of the resolve-only list.*
- **단위 테스트**: 이동(대소문자 보존, `..`, `/`, 끝 `\`, `.`), 이동 뒤 상대 열기, 오류 2·3·267·123, 루트 밖, 두 export의 버퍼·last error 규칙. / ***Unit tests:** moving (case kept, `..`, `/`, a trailing `\`, `.`), relative opens after a move, errors 2, 3, 267, and 123, outside the root, and both exports' buffer and last-error rules.*

## 검증 / Validation

| 항목 / Item | 결과 / Result |
| --- | --- |
| Windows x86 build, CTest | exit 0, 6/6 |
| Windows 실제 4th | 생략했다. `src/platform/windows`에 바뀐 것이 없다. / *Skipped; nothing under `src/platform/windows` changed.* |
| Linux x64·x86 build, CTest | 경고·오류 없음, 각각 3/3(unit checks 3,917) / *no warnings or errors, 3/3 each (3,917 unit checks)* |
| 실제 4th, Linux 두 폭 / real 4th, both Linux widths | 호출 23,059번, hardlock 83. 주소와 시계 값을 정규화하면 두 폭이 같다. `GetFileType` 뒤 `EIP 0x004724d3`에서 SIGSEGV. / *23,059 calls, hardlock 83; identical on both widths after address and clock normalization. SIGSEGV at `EIP 0x004724d3` after `GetFileType`.* |

## 다음 / Next

파일 handle의 `GetFileType`이다(`FILE_TYPE_DISK`).

*Next is `GetFileType` for file handles (`FILE_TYPE_DISK`).*
