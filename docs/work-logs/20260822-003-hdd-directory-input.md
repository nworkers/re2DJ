# 작업 로그: HDD 디렉터리 입력

## 결과

완료.

`re2dj::storage::GuestPath`가 Win32 경로를 다섯 형태(드라이브 절대, 드라이브 상대, 루트 상대, 순수 상대, UNC)로 파싱하고 정규화한다. UNC는 거부하고, 루트를 벗어나는 `..`도 거부한다.

`re2dj::hdd::HddRoot`가 사용자 디렉터리를 검증하고 게스트 상대 경로를 대소문자 무시로 해석한다. 나열 결과는 디렉터리 단위로 캐시한다.

`re2dj::exe::PeImageInfo`가 DOS 헤더, COFF 파일 헤더, optional header(PE32/PE32+), 섹션 테이블, 데이터 디렉터리를 읽는다. 파일에서 읽을 때는 앞 64 KiB만 읽는다. 디렉터리 하나에 실행 파일이 여럿 있어도 전체 이미지를 메모리에 올리지 않기 위해서다.

`re2dj::hdd::ScanHdd`가 디렉터리를 순회해 `.exe`를 모으고, 게스트 형식 GUI → 게스트 형식 기타 → 나머지 순으로, 같은 등급 안에서는 크기 내림차순으로 정렬한다.

`re2dj::target::TargetProfile`과 `DetectTargetProfiles()`가 스캔 결과에서 프로파일을 만든다.

명령행 호스트 `re2dj`와 도구 `re2dj_hdd_probe`, `re2dj_pe_analyzer`를 추가했다.

## 설계에서 바뀐 부분

**대소문자 해석 순서.** 처음 구현은 요청한 철자를 그대로 먼저 시도하고 실패할 때만 디렉터리를 나열했다. 실제로 돌려 보니 Windows에서는 파일 시스템이 대소문자를 구분하지 않아 첫 시도가 성공했고, 결과 경로가 **요청한 철자 그대로** 나왔다. 같은 덤프에 대해 Linux는 디스크의 철자를 내놓으므로 두 호스트의 출력이 달라진다.

`--resolve "C:\ez2dj\data\SONG01.ez"`가 Windows에서 `...\ez2dj\data\SONG01.ez`를, Linux에서 `.../EZ2DJ/DATA/Song01.EZ`를 내놓는 상태였다. 호스트에 따라 로그와 비교 결과가 달라지는 것은 공용 코어에 플랫폼 가정을 넣지 않는다는 규칙에 어긋나므로, 항상 나열 결과와 대조하도록 바꿨다. 정확히 일치하는 항목이 있으면 그것을, 없으면 대소문자 무시로 처음 일치한 항목을 쓴다. 이 동작을 고정하는 테스트도 추가했다.

디렉터리를 나열할 수 없는 경우에만 요청한 철자를 그대로 시도하는 경로가 남아 있다.

## 검증

**단위 테스트 119개 검사, 실패 0.** `ctest`로 실행한다.

**PE 판독기 정확성.** `re2dj_pe_analyzer`를 `C:\Windows\SysWOW64\notepad.exe`(실제 32비트 x86 PE32 GUI 실행 파일)에 돌려 Microsoft `dumpbin /headers` 출력과 대조했다. machine, magic, image base, entry point, size of image, section alignment, file alignment, 섹션 수, subsystem이 모두 일치했다. 대조 표는 [분석 문서](../analysis/ez2dj-hdd-layout.md)에 남겼다.

**스캔과 타깃 선택.** 32비트 실행 파일 하나(`EZ2DJ/Ez2dj.exe`)와 64비트 실행 파일 하나(`tools/Helper.exe`)를 담은 임시 디렉터리를 만들어 확인했다. 32비트 항목만 `guest fmt : yes`로 분류되고 후보 맨 앞에 왔으며, 타깃 프로파일이 정확히 하나 생성되었다.

**대소문자 해석.** `--resolve "C:\ez2dj\data\SONG01.ez"`가 `...\EZ2DJ\DATA\Song01.EZ`를 반환했다. 요청과 다른 철자로 물어도 해석되고, 결과는 디스크의 철자로 나온다.

**경로 탈출 차단.** `--resolve "..\..\secrets.txt"`가 거부되고 종료 코드 1로 끝났다.

**미구현 통보.** `--run`이 실행 backend 미구현임을 알리고 종료 코드 3으로 끝났다.

## 미검증

Linux와 Web 호스트에서는 실행하지 않았다. 해당 툴체인이 이 환경에 없다. 대소문자 무시 해석의 **실제 fallback 경로**는 대소문자를 구분하는 파일 시스템에서만 동작하므로, Linux 검증 전까지는 단위 테스트가 확인한 범위까지만 신뢰한다.

원본 EZ2DJ 덤프로는 확인하지 않았다. 덤프가 없다.

## Work Log: HDD Directory Input

## Result

Complete. `GuestPath` parses and normalises the five Win32 path forms, rejecting UNC and any `..` that escapes the root. `HddRoot` validates the user directory and resolves guest-relative paths case-insensitively, caching listings per directory. `PeImageInfo` reads the DOS header, COFF file header, PE32 and PE32+ optional headers, section table, and data directories, reading only the leading 64 KiB of a file so scanning a directory of executables does not pull whole images into memory. `ScanHdd` walks the tree and ranks candidates. `TargetProfile` and `DetectTargetProfiles()` build profiles from a scan. The `re2dj` host and the `re2dj_hdd_probe` and `re2dj_pe_analyzer` tools were added.

## Changed from the design

The first implementation probed the requested spelling before listing the directory. Running it showed that on Windows the case-insensitive file system made the first probe succeed and return the **requested** spelling, while Linux would return the on-disk one — so `--resolve "C:\ez2dj\data\SONG01.ez"` produced different output per host for one dump. Host-dependent output contradicts the rule against platform assumptions in the shared core, so resolution now always goes through the listing: exact match first, then the first case-insensitive match. A test pins that behavior. The verbatim probe survives only for a directory that cannot be listed.

## Verification

119 checks pass with no failures under `ctest`.

`re2dj_pe_analyzer` was compared against Microsoft `dumpbin /headers` on `C:\Windows\SysWOW64\notepad.exe`, a real 32-bit x86 PE32 GUI executable. Machine, magic, image base, entry point, size of image, section alignment, file alignment, section count, and subsystem all matched; the comparison table is in the [analysis document](../analysis/ez2dj-hdd-layout.md).

A temporary directory holding one 32-bit and one 64-bit executable confirmed that only the 32-bit entry is classified as guest format, that it sorts first, and that exactly one target profile results.

`--resolve "C:\ez2dj\data\SONG01.ez"` returned `...\EZ2DJ\DATA\Song01.EZ`, `--resolve "..\..\secrets.txt"` was refused with exit code 1, and `--run` reported the missing execution backend and exited with code 3.

## Not verified

Nothing was run on a Linux or Web host, as neither toolchain is present here. The **real fallback path** of case-insensitive resolution only engages on a case-sensitive file system, so until Linux verification it is trusted only as far as the unit tests reach.

Nothing was verified against an original EZ2DJ dump, because none is available.
