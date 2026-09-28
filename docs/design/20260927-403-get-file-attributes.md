# 작업 403 설계 — GetFileAttributesA / Task 403 design — GetFileAttributesA

선행: [작업 399 설계](20260927-399-find-files.md), [작업 393 설계](20260927-393-current-directory.md)

## 배경 / Background

작업 402 뒤 Linux의 4th는 `kernel32!GetFileAttributesA("..\..\ranking\ranking_StreetMix.bin")`에서 멈췄다. 게임의 현재 디렉터리 기준으로 이 경로는 CHD의 `EZ2DJ/SYSTEM/Ranking/ranking_StreetMix.bin`(79,992바이트)이다. 게임은 이 파일에서 랭킹(HIGH SCORE 표)을 읽는다.

Windows 제품은 이 함수를 VFS로 가로채지 않았다. 게임은 이 함수를 `GetProcAddress`로 얻는데, 동적 resolver가 `win32` 경로로 진짜 함수를 넘겼다. 그래서 guest의 상대 경로가 guest 현재 디렉터리가 아니라 host 프로세스의 현재 디렉터리 기준으로 조회되었다. CHD에 있는 파일도 없다고 답할 수 있었다.

*After Task 402 the 4th on Linux stopped at `kernel32!GetFileAttributesA("..\..\ranking\ranking_StreetMix.bin")`. Against the game's current directory that path is the CHD's `EZ2DJ/SYSTEM/Ranking/ranking_StreetMix.bin` (79,992 bytes), which holds the ranking (HIGH SCORE) table. The Windows product did not route this function through its VFS: the game obtains it through `GetProcAddress`, and the dynamic resolver handed out the real function (`win32` route). A guest-relative path was then looked up against the host process's current directory, not the guest's, so a file present on the CHD could be reported missing.*

### 측정 / Measurements

Windows 11의 32비트 프로그램으로 NTFS에서 측정했다.

*Measured by a 32-bit program on Windows 11 on NTFS.*

| 요청 / Request | 결과 / Result |
| --- | --- |
| 파일(구분자 `\`·`/`, `..` 포함) / a file (either separator, `..` included) | `0x80`(`FILE_ATTRIBUTE_NORMAL`로 만든 파일), last error 그대로 / *last error unchanged* |
| 디렉터리, 끝 구분자, `.` / a directory, with a trailing separator, `.` | `0x10`, last error 그대로 / *last error unchanged* |
| 없는 파일 / a missing file | `INVALID_FILE_ATTRIBUTES`, 2 |
| 없는 중간 디렉터리, 파일 아래의 이름 / a missing directory on the way, a name below a file | 3 |
| 끝 구분자가 붙은 파일 / a file with a trailing separator | 267 (`ERROR_DIRECTORY`) |
| 빈 문자열, NULL / an empty string, null | 3 |
| 와일드카드, 금지 문자 / a wildcard, a forbidden character | 123 |

## 결정 / Decisions

1. **공용 규칙.** `storage/guest_find`에 `DescribeGuestFileAttributes`를 둔다. root 아래의 경로 구성 요소를 받아 앞에서부터 한 칸씩 host의 조회 함수에 묻고, 측정한 오류를 정한다. 속성 값은 `FindFirstFileA`와 같은 `EntryAttributes`(파일 `NORMAL`, 디렉터리 `DIRECTORY`)다.
   ***The shared rule.** `DescribeGuestFileAttributes` in `storage/guest_find` takes the components below the root, asks the host's lookup about each prefix in turn, and decides the measured errors. The attribute values are `EntryAttributes`, the same as `FindFirstFileA` gives (files `NORMAL`, directories `DIRECTORY`).*
2. **Linux.** `GuestFiles::Attributes`가 경로를 현재 디렉터리 기준으로 풀고 오버레이와 CHD에서 찾는다. kernel32 `GetFileAttributesA`가 이를 쓰고, 성공하면 last error를 그대로 둔다. guest root 밖의 경로는 모델 밖이라 멈춘다.
   ***Linux.** `GuestFiles::Attributes` resolves the path against the current directory and looks it up in the overlay and the CHD; kernel32 `GetFileAttributesA` uses it, leaving the last error alone on success. A path outside the guest root is unmodelled and stops.*
3. **Windows.** VFS에 `Re2djVfsGetFileAttributesA`를 더하고 동적 resolver와 IAT 목록에 등록한다.
   - HDD 경로는 추적 중인 guest 현재 디렉터리로 푼다. 그리고 같은 공용 규칙을 `GuestEntryAt`(오버레이·HDD 디렉터리·CHD 순서)으로 적용한다. `GuestEntryAt`는 기존 디렉터리 확인 함수를 파일까지 구분하도록 일반화한 것이다.
   - 지원 디렉터리(`C:\windows`)와 guest 문법이 거부하는 이름은 전처럼 OS에 묻는다.
   - 성공하면 호출 전 last error를 되돌린다. 내부의 host 조회가 last error를 바꾸기 때문이다.

   ***Windows.** The VFS gains `Re2djVfsGetFileAttributesA`, registered in the dynamic resolver and the IAT lists.*
   - *An HDD path resolves against the tracked guest current directory and goes through the same shared rule with `GuestEntryAt` (overlay, HDD directory, then CHD), which generalises the existing directory check to tell files apart.*
   - *Support-directory names (`C:\windows`) and names the guest syntax rejects still ask the OS.*
   - *On success the caller's last error is restored, since the host lookups inside change it.*

## 범위 밖 / Out of scope

- 랭킹 파일 쓰기(게임이 점수를 저장할 때의 오버레이 쓰기)는 게임이 도달할 때 본다. / *Writing the ranking file (the overlay write when the game saves a score) is taken up when the game reaches it.*
- FAT 속성 비트(읽기 전용, 보관 등)는 반영하지 않는다. Windows 제품 VFS의 목록 규칙과 같다. / *FAT attribute bits (read-only, archive, and so on) are not reflected, as in the Windows product VFS's listing rule.*
