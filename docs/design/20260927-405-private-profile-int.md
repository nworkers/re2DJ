# 작업 405 설계 — GetPrivateProfileIntA와 디렉터리 덤프 파일 / Task 405 design — GetPrivateProfileIntA and directory-dump files

## 배경 / Background

Linux의 EZ2DJ 1st(`ez2dj1st`)는 보호 stub이 원래 프로그램의 import를 되살리는 중에, `GetProcAddress(kernel32, "GetPrivateProfileIntA")`에서 멈췄다. 1st는 `bookkeeping.ini`에서 코인·통계 값을 이 함수로 읽는다. Windows 제품은 이 호출을 VFS로 받아 진짜 Win32 profile API에 넘기고, `[GAMEASSIGNMENTS] DemoVolume`만 제품 설정으로 답한다(작업 086).

1st는 CHD가 아닌 디렉터리 덤프(`roms/ez2dj1st`)다. 그런데 Linux `GuestFiles`는 CHD만 원본으로 받아서, 1st에는 파일이 하나도 없었다.

*On Linux, EZ2DJ 1st stopped at `GetProcAddress(kernel32, "GetPrivateProfileIntA")` while its protection rebuilt the original program's imports. 1st reads its coin and statistics settings from `bookkeeping.ini` with this function. The Windows product routes the call through its VFS to the real Win32 profile API, answering only `[GAMEASSIGNMENTS] DemoVolume` from the product's own setting (Task 086). 1st is a directory dump (`roms/ez2dj1st`), not a CHD, and Linux `GuestFiles` took only a CHD as its source, so 1st had no files at all.*

### 측정 / Measurements

Windows 11에서 32비트 프로그램으로 시험 INI를 읽어 측정했다.

*Measured on Windows 11 by a 32-bit program reading a test INI.*

| 항목 / Item | 결과 / Result |
| --- | --- |
| 이름 비교 / names | 섹션·키는 대소문자와 앞뒤 공백을 무시 / *sections and keys compare without case or surrounding spaces* |
| 중복 / duplicates | 같은 섹션은 첫 번째만, 같은 키는 첫 줄만 / *only a section's first occurrence, a key's first line* |
| 주석, `=` 없는 줄 / comments | `;`로 시작하는 줄과 `=` 없는 줄은 키가 아님 / *lines starting with `;` and lines without `=` hold no key* |
| 값 / values | 따옴표 하나 건너뜀, 부호와 10진수(소문자 `0x` 뒤 16진수)를 다른 문자 전까지 읽음, 2^32로 감김. `12abc` 12, `abc` 0, `0X1b` 0, `010` 10, 빈 값은 기본값 / *one quote skipped, sign and decimal digits (hex after lowercase `0x`) up to another character, wrapping at 2^32* |
| last error | 찾으면 0, 키·섹션 없음 2(기본값), 디렉터리 없음 3, NULL 섹션·키는 0을 돌려주고 0 / *0 when found; 2 with the default when the key or section is missing; 3 for a missing directory; a null section or key returns 0 with 0* |
| 파일 이름 / file name | 디렉터리 없는 이름은 현재 디렉터리가 아니라 Windows 디렉터리에서 찾음. `.\x.ini`는 현재 디렉터리 / *a bare name is looked up in the Windows directory; `.\x.ini` in the current one* |

## 결정 / Decisions

1. **공용 INI core.** `hle/private_profile.h`의 `FindPrivateProfileValue`와 `ParsePrivateProfileInt`가 측정한 규칙을 담는다. `DemoVolume` 정책은 `PrivateProfileIntOverride`로 옮겨 Windows 제품(`ini_profile_hle.cpp`)과 Linux가 함께 쓴다. 기본값도 `kDefaultDemoVolume`(3) 하나다.
   ***A shared INI core.** `FindPrivateProfileValue` and `ParsePrivateProfileInt` in `hle/private_profile.h` hold the measured rules. The DemoVolume policy moves to `PrivateProfileIntOverride`, which the Windows product (`ini_profile_hle.cpp`) and Linux share, with one default, `kDefaultDemoVolume` (3).*
2. **Linux kernel32 `GetPrivateProfileIntA`.** 파일을 `GuestFiles`로 읽고 core로 답한다. 디렉터리 없는 파일 이름(Windows 디렉터리)과 guest root 밖의 파일은 모델 밖이라 멈춘다.
   ***Linux kernel32 `GetPrivateProfileIntA`.** It reads the file through `GuestFiles` and answers with the core. A bare file name (the Windows directory) and a file outside the guest root are unmodelled and stop.*
3. **디렉터리 덤프 원본.** `GuestFileConfig::hdd_directory`가 CHD 대신 host 디렉터리를 원본으로 삼는다.
   - host 파일 시스템은 대소문자를 구분할 수 있어서, 정확한 철자가 없으면 구성 요소마다 대소문자를 무시하고 찾는다.
   - 목록은 NTFS처럼 이름을 대소문자 무시로 정렬한다. 덤프의 host 시각은 복사한 시각일 뿐이라 날짜는 비운다.
   - CLI는 디렉터리 덤프의 HDD root를 넘긴다.

   ***Directory-dump source.** `GuestFileConfig::hdd_directory` makes a host directory the source in place of a CHD.*
   - *A host file system may tell case apart, so each component is matched without case when its exact spelling is missing.*
   - *Listings sort names without case, as NTFS does; dates stay empty, since a dump's host times are only when it was copied.*
   - *The CLI passes a directory dump's HDD root.*
4. **`CreateFileA`의 없는 디렉터리.** `GuestFiles::Open`이 기존 파일 열기에서 중간 디렉터리가 없으면 `ERROR_PATH_NOT_FOUND`(3)를 준다. Windows와 같다.
   ***A missing directory in `CreateFileA`.** When opening an existing file, `GuestFiles::Open` now answers `ERROR_PATH_NOT_FOUND` (3) for a missing directory on the way, as Windows does.*
5. **이름 조회.** 원래 프로그램의 `.idata`에 남은 import 이름 가운데 facade에 없는 것을 resolve-only로 등록한다. 대상은 kernel32 24개, user32 3개, gdi32 4개, ddraw 2개다. 호출되면 멈추고, 그때 각자의 경계로 다룬다.
   ***Name lookups.** The original program's import names left in its `.idata` that the facades lacked are registered resolve-only: 24 in kernel32, 3 in user32, 4 in gdi32, and 2 in ddraw. A call stops, to be taken up as its own boundary.*

## 범위 밖 / Out of scope

- `GetPrivateProfileStringA`, `GetPrivateProfileSectionNamesA`, `WritePrivateProfileStringA`(resolve-only). / *`GetPrivateProfileStringA`, `GetPrivateProfileSectionNamesA`, and `WritePrivateProfileStringA` (resolve-only).*
- 1st의 CRT 시작 함수(`InitializeCriticalSection` 등). / *1st's CRT startup functions (`InitializeCriticalSection` and the rest).*
