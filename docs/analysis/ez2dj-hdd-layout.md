# EZ2DJ HDD 레이아웃과 실행 파일 식별 / EZ2DJ HDD Layout and Executable Identification

주제: 사용자가 제공한 HDD 덤프의 디렉터리 구조, 어떤 파일이 게임 실행 파일인지, 그 실행 파일의 PE 특성.

*Topic: the directory structure of a user-supplied HDD dump, which file is the game executable, and that executable's PE characteristics.*

측정 대상 덤프 두 개:

| 식별자 | 내용 | 파일 수 |
| --- | --- | --- |
| 1st SE | EZ2DJ 1st Trax Special Edition | 245 디렉터리 / 16,613 파일 |
| 3rd | EZ2DJ 3rd Trax | 199 디렉터리 / 25,054 파일 |

측정 방법: `re2dj_hdd_probe <dir>`, `re2dj_pe_analyzer <file>`, 그리고 import 테이블 확인용 일회성 스크립트.

*Two dumps were measured with `re2dj_hdd_probe`, `re2dj_pe_analyzer`, and a one-off script for the import tables.*

---

## 1. 확인됨: 식별 도구의 정확성 / Confirmed: the tooling is correct

`re2dj_pe_analyzer`를 `C:\Windows\SysWOW64\notepad.exe`에 실행해 Microsoft `dumpbin /headers`와 대조했다. machine, magic, image base, entry point, size of image, section alignment, file alignment, 섹션 수, subsystem이 모두 일치했다.

*Verified `re2dj_pe_analyzer` against Microsoft `dumpbin /headers` on a real 32-bit PE32 GUI executable; every compared field matched.*

두 덤프 모두 `truncated : no`로 완주했고, 발견한 실행 파일 다섯 개 전부 PE 헤더를 읽어냈다.

*Both dumps were walked to completion and all five executables found had readable PE headers.*

---

## 2. 확인됨: 1st SE 덤프 구조 / Confirmed: 1st SE dump layout

```text
<root>/
├── ez2dj.exe          561,152 B  2000-01-01  보호됨 / protected
├── ez2dj1.exe         524,288 B  1999-12-24  보호되지 않음 / not protected
├── Test.exe         1,859,633 B  1999-12-22  서비스·테스트 도구 / service tool
├── PlzPowerOff.exe     98,304 B  1999-03-30  종료 화면 / shutdown screen
├── Tdsd.vxd111         30,278 B  2000-01-19  Windows 9x VxD
├── ez2dj.ini            4,142 B             난이도·모드·곡 목록 / difficulty, modes, song lists
├── System.ini           1,322 B
├── rank_0.dat / rank_1.dat / rank_2.dat  각 400 B  랭킹 저장 / ranking storage
├── Songs/             68개 디렉터리 / 68 directories
└── System/            화면 단위 자산 / per-screen assets
    ├── Title/  MusicSelect/  Result/  Ranking/  NameEntry/
    ├── ClubMix/  RadioMix 계열/  LevelSelect/  ChannelSelect/
    └── Opening/  Demonstration/  EyeCatch/  GameOver/  ending/ ...
```

`Songs/` 아래 디렉터리 이름이 `ez2dj.ini`의 `[BATTLEMODE]`, `[ClubMix]`, `[RADIOMIX]` 항목에 나오는 곡 식별자와 일치한다. 즉 **곡 선택과 모드 구성이 INI로 외부화되어 있다.**

*The directory names under `Songs/` match the song identifiers listed in the `[BATTLEMODE]`, `[ClubMix]`, and `[RADIOMIX]` sections of `ez2dj.ini`, so **song selection and mode composition are externalised into INI files.***

## 확인됨: 3rd 덤프 구조 / Confirmed: 3rd dump layout

```text
<root>/
├── EZ2DJ.EXE        1,216,512 B  2001-09-24  보호됨 / protected
├── EZ2DJ.INI
├── FONTEN.DAT / FONTKR.DAT      영문·한글 폰트 / English and Korean fonts
├── cache.reg / cache.txt
├── BG/            배경 영상·이미지 / background media
├── Sound/
└── system/
```

1st SE와 3rd는 디렉터리 구조가 서로 다르다. **타깃 프로파일을 버전별로 분리한 설계가 실제로 필요하다는 것이 확인되었다.**

*The 1st SE and 3rd layouts differ, which **confirms that per-version target profiles are actually needed** rather than merely anticipated.*

---

## 3. 확인됨: 실행 파일 PE 특성 / Confirmed: executable PE characteristics

모든 실행 파일이 `PE32 / i386 / Windows GUI / ImageBase 0x00400000`이다.

*Every executable is `PE32 / i386 / Windows GUI` based at `0x00400000`.*

| 파일 | 진입점 RVA | 진입점이 놓인 섹션 | SizeOfImage | 보호 |
| --- | --- | --- | --- | --- |
| `ez2dj1.exe` (1st SE) | `0x0003a640` | `.text` | `0x01ad1000` | **없음** |
| `ez2dj.exe` (1st SE) | `0x01ad23cf` | `.gtide` | `0x01ada000` | **있음** |
| `EZ2DJ.EXE` (3rd) | `0x00642240` | `.protect` | `0x0067c000` | **있음** |
| `Test.exe` (1st SE) | `0x0001ada0` | `.text` | — | 없음 |
| `PlzPowerOff.exe` (1st SE) | `0x00001e6e` | `.text` | — | 없음 |

### 확인됨: ez2dj1.exe는 선호 주소에 고정되어 있다

`ez2dj1.exe`에는 이름이 `.reloc`인 섹션이 있지만 optional header의 base relocation data directory는 `{RVA 0, Size 0}`이다. Stage 2 로더로 선호 주소 `0x00400000` 적재는 성공하고 다른 주소 적재는 재배치 정보 부재로 거부된다. 따라서 이 bring-up 빌드는 현재 확인된 형태 그대로라면 선호 주소에 고정해서 적재해야 한다.

*Confirmed: `ez2dj1.exe` has a section named `.reloc`, but its optional-header base-relocation data directory is `{RVA 0, Size 0}`. The Stage 2 loader maps it successfully at preferred base `0x00400000` and rejects a different base because no relocation records are advertised. This bring-up build must therefore be loaded at its preferred base in the form inspected.*

### 확인됨: ez2dj.exe와 EZ2DJ.EXE는 보호되어 있다

`ez2dj.exe`의 섹션은 `.text .rdata .data .idata .reloc` 뒤에 **`.gtide` `.gdata` `.gidata`** 세 개가 더 붙어 있고, 진입점이 마지막 코드 섹션 `.gtide` 안에 있다. import 디렉터리도 `.gidata`(`0x01ad8000`)로 옮겨져 있다.

`EZ2DJ.EXE`(3rd)는 `.protect` 섹션에 진입점이 있고, 이름 자체가 목적을 말한다.

*The protected builds carry extra sections after the normal five, hold their entry point in the last of them, and relocate the import directory into a packer-owned section. The 3rd's section is literally named `.protect`.*

### 확인됨: ez2dj1.exe는 같은 프로그램의 보호되지 않은 빌드다

두 파일의 섹션 배치가 앞부분에서 정확히 일치한다.

| 섹션 | ez2dj1.exe | ez2dj.exe |
| --- | --- | --- |
| `.text` | va `0x00001000` vs `0x00052540` | 동일 |
| `.rdata` | va `0x00054000` vs `0x00007571` | 동일 |
| `.data` | va `0x0005c000` vs `0x01a5d2f8` | 동일 |
| `.idata` | va `0x01aba000` vs `0x00000fa4` | 동일 |
| `.reloc` | va `0x01abb000` vs `0x00015094` | 동일 |

import 목록도 사실상 같다(아래 4절). `ez2dj.exe`는 이 이미지에 보호 계층을 씌운 것이다.

*The two share their first five sections byte-for-byte in layout and share essentially the same import list, so `ez2dj.exe` is this image with a protection layer wrapped around it.*

> [!IMPORTANT]
> **`ez2dj1.exe`가 Stage 2·3의 첫 실행 대상이다.** 보호되지 않았으므로 로더가 언패킹 스텁을 실행하지 않고도 진짜 게임 코드에 도달한다. 보호된 빌드는 자기 수정 코드를 실행할 수 있어야 하므로 인터프리터 backend가 성숙한 뒤로 미룬다.
>
> ***`ez2dj1.exe` is the bring-up target for Stages 2 and 3.** Being unprotected, the loader reaches real game code without executing an unpacking stub. The protected builds need an execution backend that tolerates self-modifying code, so they wait until the interpreter is mature.*

### 추정: `.data`의 거대한 가상 크기

`.data`는 RawSize `0x0000d000`(52 KB)인데 VirtualSize가 `0x01a5d2f8`(약 27 MB)이다. 27 MB는 파일에서 오는 것이 아니라 0으로 채워지는 영역이다. 게임 자산을 담을 정적 버퍼로 추정한다. 근거는 크기와 `.data`라는 위치뿐이며, 실제 용도는 실행해 봐야 확인된다.

*Inferred: `.data` is 52 KB on disk but about 27 MB in memory, so roughly 27 MB is zero-filled. A static buffer for game assets is the likely purpose, but the only evidence is its size and placement, and real use needs a run to confirm.*

### 확인됨: 캐비닛이 실행하는 것은 `ez2dj.exe`다

1st SE 덤프의 `System.ini` `[boot]` 절에 결정적 항목이 있다.

```ini
shell=d:\ez2dj\ez2dj.exe
```

Windows 9x는 `[boot]`의 `shell=` 항목이 가리키는 프로그램을 Explorer 대신 띄운다. 즉 이 한 줄이 아케이드 캐비닛의 부팅 후 진입점을 정의한다. 여기서 세 가지가 동시에 확정된다.

| 항목 | 값 | 근거 |
| --- | --- | --- |
| 정식 실행 파일 | `ez2dj.exe` | `shell=` 값의 파일 이름 |
| 게스트 드라이브 문자 | `D:` | `shell=` 값의 드라이브 |
| 게스트 작업 디렉터리 | `\ez2dj` | `shell=` 값의 디렉터리 |

드라이브 문자와 작업 디렉터리는 이 문서와 `docs/EXE_DESIGN.*`에서 미확정으로 남아 있던 항목이다.

**`ez2dj1.exe`는 캐비닛이 실행한 것이 아니다.** 보호되지 않아 로더 개발에 유용할 뿐이므로, 그것으로 관찰한 동작을 원본 동작으로 인용하면 안 된다. 이 구분은 타깃 프로파일에 `bring_up_target` 플래그로 기록되어 있다.

*Confirmed: the cabinet runs `ez2dj.exe`. The `[boot]` section of `System.ini` reads `shell=d:\ez2dj\ez2dj.exe`, and Windows 9x launches whatever `shell=` names in place of Explorer, so that single line defines the cabinet's post-boot entry point and fixes the canonical executable, the guest drive letter `D:`, and the guest directory `\ez2dj` at once. The latter two had been unresolved. **`ez2dj1.exe` is not what the cabinet ran** — it is merely unprotected and therefore useful for loader development, so behavior observed through it must not be cited as original behavior. That distinction is recorded on the target profile as a `bring_up_target` flag.*

### 확인됨: 3rd는 I/O 카드를 쓴다

3rd 덤프의 `EZ2DJ.INI`에 `"UseIOCard" = 1`이 있다. 해상도는 `"Window Width" = 640`, `"Window Height" = 480`이고 `"FullScreen" = 1`이다.

*Confirmed: the 3rd dump's `EZ2DJ.INI` carries `"UseIOCard" = 1`, a 640x480 window size, and `"FullScreen" = 1`.*

### 미확정: 3rd의 게스트 경로

3rd 덤프에는 `System.ini`가 없다. 따라서 3rd의 게스트 드라이브 문자와 작업 디렉터리는 확인되지 않았다. 1st SE의 값을 복사해 넣지 않는다.

*Unresolved: the 3rd dump has no `System.ini`, so its guest drive letter and working directory are not confirmed. The 1st SE values are not copied across.*

### 미확정: Tdsd.vxd111

`Tdsd.vxd111` 파일이 1st SE 덤프에 있다. VxD는 Windows 9x 전용 커널 드라이버이므로 아케이드 I/O 보드 접근 경로일 가능성이 있다. 다만 `ez2dj1.exe`의 import에는 `DeviceIoControl`이 없고, 파일 이름의 `111` 확장자는 이 덤프에서 드라이버가 **비활성화되어 있음**을 시사한다.

**확인 방법:** 실행 중 `CreateFileA`가 요청하는 경로를 추적한다. `\\.\`로 시작하는 이름이 나오면 드라이버 경로다.

*Unresolved: `Tdsd.vxd111` is a Windows 9x kernel driver and could be the arcade I/O path, but `ez2dj1.exe` imports no `DeviceIoControl` and the `111` suffix suggests the driver is disabled in this dump. Confirm by tracing the paths `CreateFileA` asks for during a run; a name starting with `\\.\` is a driver path.*

---

## 4. import 목록 / Import list

별도 문서로 분리했다: [EZ2DJ import 표면](ez2dj-import-surface.md)

*Split into its own document: [EZ2DJ Import Surface](ez2dj-import-surface.md)*

---

## 5. 도구의 결함 / Defects in the tooling

**해결됨 — 기본 타깃 선택.** `re2dj --hdd <1st SE dump>`가 기본 타깃으로 `Test.exe`를 골랐다. 후보 순위가 파일 크기 내림차순이라 서비스 도구(1.86 MB)가 게임(561 KB)보다 먼저 왔기 때문이다.

크기는 "어느 것이 게임인가"의 근거가 못 된다. 순위 휴리스틱을 손보는 대신 **내장 타깃 프로파일**을 추가해 해결했다. 지금은 두 덤프 모두 정확한 기본 타깃(`ez2dj1stse` → `ez2dj.exe`, `ez2dj3rd` → `EZ2DJ.EXE`)을 고른다. 설계는 [20260822-005](../design/20260822-005-built-in-target-profiles.md)에 있다.

*Resolved: the default target used to be `Test.exe` for the 1st SE dump, because ranking broke ties by descending file size. Size is not evidence of which file is the game, so the fix was built-in target profiles rather than a better heuristic. Both dumps now select correctly.*

**미해결 — 비ASCII 경로 출력.** `re2dj_hdd_probe`가 비ASCII 문자가 든 디렉터리 경로를 콘솔에 깨진 형태로 출력한다. 해석 자체는 정상이고 출력만 깨진다. `std::filesystem::path::string()`이 Windows에서 활성 ANSI 코드 페이지로 변환하기 때문이다.

*Open: `re2dj_hdd_probe` prints a directory path containing non-ASCII characters as mojibake. Resolution itself works and only the output is wrong, because `std::filesystem::path::string()` converts through the active ANSI code page on Windows.*

`re2dj_hdd_probe`가 비ASCII 문자가 든 디렉터리 경로를 콘솔에 깨진 형태로 출력했다. 해석 자체는 정상이었고 출력만 깨졌다. `std::filesystem::path::string()`이 Windows에서 활성 ANSI 코드 페이지로 변환하기 때문이다. 별도로 다룬다.

*`re2dj_hdd_probe` printed a directory path containing non-ASCII characters as mojibake. Resolution itself worked and only the output was wrong, because `std::filesystem::path::string()` converts through the active ANSI code page on Windows. Handled separately.*

---

## 관련 문서 / Related documents

* [EZ2DJ import 표면](ez2dj-import-surface.md)
* [PE32 실행 형식](../kb/pe32-executable-format.md)
* [Win32 HLE 경계](../kb/win32-hle-boundary.md)
* [원본 실행 파일 분석 (누적)](../EXE_DESIGN.ko.md)
