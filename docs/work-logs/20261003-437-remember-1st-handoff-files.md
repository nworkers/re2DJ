# 작업 437 작업 로그 — Remember 1st로 넘기는 파일 / Task 437 work log — the files handed to Remember 1st

설계: [20261003-437-remember-1st-handoff-files.md](../design/20261003-437-remember-1st-handoff-files.md) · 지시서: [20261003-437-remember-1st-handoff-files.md](../work-orders/20261003-437-remember-1st-handoff-files.md)

## 2026-10-03

- **증상**: 사용자가 Linux에서 6th 모드 선택으로 Remember 1st를 고르자 re2dj가 끝났다. 6th 자식은 6,702,225번째 호출 `kernel32!DeleteFileA(".\EZ2DJ1st\bookkeeping.ini")`에서 멈췄고, launcher는 `ExitProcess(0)`으로 끝났다(`20261003-030014-522`, `20261003-030015-364`). overlay에는 `EZ2DJ1st/`와 `EZ2DJ1ST/`가 따로 있었다.
  *Symptom: choosing Remember 1st in 6th's mode select on Linux ended re2dj. The 6th child stopped at call 6,702,225, `kernel32!DeleteFileA(".\EZ2DJ1st\bookkeeping.ini")`, and the launcher ended with `ExitProcess(0)` (`20261003-030014-522`, `20261003-030015-364`); the overlay held both `EZ2DJ1st/` and `EZ2DJ1ST/`.*
- **원본 확인**(`EZ2DJ6th.EXE`, `0x411f6d44`, 작업 436의 파일 이미지): `0x0044b250`이 `[0x0047d0e0] = 0x100` 뒤 `0x0044b0a0`을 부른다. `0x0044b0a0`은 `FreePlay` 쓰기, `DeleteFileA`(IAT `0x0047204c`, import 표에서 `KERNEL32.dll!DeleteFileA` 확인), `Coins`·`PlayCoins`·`ContinueCoins`·`GameLevel` 쓰기 순이다. 쓰기 helper `0x0044b170`은 `wsprintfA("%d")`와 `WritePrivateProfileStringA`다.
  *The original (`EZ2DJ6th.EXE`, `0x411f6d44`, task 436's file image): `0x0044b250` sets `[0x0047d0e0] = 0x100` and calls `0x0044b0a0`, which writes `FreePlay`, calls `DeleteFileA` (IAT `0x0047204c`, `KERNEL32.dll!DeleteFileA` in the import table), then writes `Coins`, `PlayCoins`, `ContinueCoins` and `GameLevel`; the write helper `0x0044b170` is `wsprintfA("%d")` then `WritePrivateProfileStringA`.*
- **구현**
  - `GuestFiles`: `OverlayPath`(대소문자 무시), `.re2dj-deleted` 목록(`LoadDeletedList`·`SaveDeletedList`·`InImage`), `Delete`.
  - `win32_errors.h`: `kWin32ErrorSharingViolation`.
  - kernel32 `DeleteFileA`: resolve-only 목록에서 구현 export로 옮겼다.

  *Implementation: `GuestFiles` gains `OverlayPath` (ignoring case), the `.re2dj-deleted` list (`LoadDeletedList`, `SaveDeletedList`, `InImage`) and `Delete`; `win32_errors.h` gains `kWin32ErrorSharingViolation`; kernel32 `DeleteFileA` moves from the resolve-only list to an implemented export.*
- **검증**
  - `scripts/test_all.sh linux-x64-debug`(경고를 오류로): build 성공, CTest 4개 통과.
  - 새 검사 `CheckDeleteFile`, `CheckDeleteFileExport`(`guest_files_test.cpp`): 대소문자가 다른 overlay 경로, `Delete`의 다섯 결과, whiteout 뒤 `Open`·`Attributes`·`FindFirst`, 다음 `GuestFiles`로의 전달, 다시 만든 파일이 비어 시작하는 것, `DeleteFileA` 뒤 INI가 새로 쓰이는 것.
  - `kernel32_module_test.cpp`의 구현 export 수를 109에서 110으로 고쳤다.
  - 6th 20초 실행이 `Flip` 반복으로 끝났다(회귀 없음).
  - Windows 제품은 바꾸지 않았다.

  *Verification: `scripts/test_all.sh linux-x64-debug` (warnings as errors) builds and passes 4 CTest tests. The new checks `CheckDeleteFile` and `CheckDeleteFileExport` (`guest_files_test.cpp`) cover overlay paths differing in case, `Delete`'s five outcomes, `Open`, `Attributes` and `FindFirst` after a whiteout, carrying it to the next `GuestFiles`, a re-made file starting empty, and an INI written afresh after `DeleteFileA`. `kernel32_module_test.cpp`'s implemented-export count went from 109 to 110. A 20-second 6th run ended still repeating `Flip` (no regression). The Windows product is unchanged.*
- **기존 overlay**: 이미 갈라진 `overlays/ez2dj6th/EZ2DJ1st/`는 정확한 철자가 이겨 계속 따로 쓰인다. 사용자가 지워야 한다.
  *Existing overlay: the already split `overlays/ez2dj6th/EZ2DJ1st/` keeps being used on its own since the exact spelling wins; the user removes it.*
- **사용자 확인**: 사용자가 갈라진 `overlays/ez2dj6th/EZ2DJ1st/`를 지운 뒤 Linux에서 6th를 거쳐 Remember 1st로 넘어갔고, 6th는 `ExitProcess(0x100)`으로 끝났다(`20261003-111041-327`부터). 작업 438~440 뒤 6th 복귀까지 확인했다.
  *User check: after removing the split `overlays/ez2dj6th/EZ2DJ1st/`, the user went through 6th to Remember 1st on Linux with 6th ending in `ExitProcess(0x100)` (from `20261003-111041-327`), and after tasks 438 to 440 back to 6th.*
