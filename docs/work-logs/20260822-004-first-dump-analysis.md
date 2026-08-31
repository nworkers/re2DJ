# 작업 로그: 첫 원본 덤프 정적 분석

## 결과

완료.

### 먼저 처리한 것: 덤프 유출 차단

`git status`를 확인하다 추적 후보가 **41,735개**로 나오는 것을 발견했다. 원인은 `.gitignore`가 `roms/`를 막지 않았기 때문이다. 처음 `.gitignore`를 쓸 때는 저장소가 비어 있었고, 덤프가 들어올 디렉터리 이름을 `hdd/`, `HDD/`, `MASTER/`로만 예상했다.

`roms/`, `dump/`, `dumps/`, `ez2dj/`를 추가하고, 덤프가 실제로 담고 있는 확장자(`*.wav`, `*.mp3`, `*.dat`, `*.bmp`, `*.avi`, `*.rar`, `*.zip` 등)를 함께 막았다. 추적 후보는 **71개**로 떨어졌다.

디렉터리 이름 목록만으로는 부족하다는 것이 이번에 드러났다. 다음 덤프가 또 다른 이름으로 들어올 수 있으므로 확장자 기반 차단을 함께 두었다.

**그 수정이 곧바로 두 번째 문제를 만들었다.** 추적 후보 목록을 다시 확인하니 `src/hdd/`와 `include/re2dj/hdd/`의 파일 네 개가 사라져 있었다. `.gitignore`에 넣은 `hdd/`가 앵커 없는 패턴이라 **경로 어느 깊이에서든** 일치하기 때문이다. 덤프를 막으려던 규칙이 프로젝트 자신의 소스 디렉터리를 조용히 제외했다.

덤프·빌드·런타임 상태 디렉터리 패턴 전부에 선행 `/`를 붙여 저장소 루트로 고정했다. 고친 뒤 네 파일이 다시 추적 후보에 나타나고 덤프와 빌드 트리는 여전히 제외되는 것을 확인했다.

교훈은 규칙을 넣은 뒤 **차단되어야 할 것과 차단되면 안 되는 것을 둘 다** 확인해야 한다는 것이다. 처음에는 전자만 확인했고, 그래서 두 번째 문제를 한 단계 늦게 발견했다.

### 확인한 것

| 덤프 | 규모 | 실행 파일 |
| --- | --- | --- |
| The 1st Tracks Special Edition | 245 디렉터리 / 16,613 파일 | 4개 |
| 3rd Trax | 199 디렉터리 / 25,054 파일 | 1개 |

가장 중요한 세 가지:

1. **`ez2dj1.exe`만 보호되지 않았다.** `ez2dj.exe`(1st SE)는 진입점이 `.gtide` 섹션에, `EZ2DJ.EXE`(3rd)는 `.protect` 섹션에 있다. `ez2dj1.exe`는 `.text`에 있고 앞 다섯 섹션의 배치가 `ez2dj.exe`와 정확히 같다. 같은 프로그램의 보호되지 않은 빌드다. Stage 2·3의 첫 실행 대상이 정해졌다.

2. **import 표면이 작다.** `ez2dj1.exe`는 7개 DLL에서 144개 함수만 가져온다. 그중 97개가 KERNEL32이고 상당수는 MSVC CRT 시작 코드가 부르는 것이다.

3. **그래픽은 DirectDraw뿐이고 입력은 `GetAsyncKeyState` 하나다.** Direct3D도 DirectInput도 없다. `ARCHITECTURE.md`가 우선순위 2로 잡아 두었던 `d3d`와 우선순위 3의 `dinput`은 1st에 관한 한 필요 없다. 예상하지 못했던 것은 DirectSound가 **ordinal import**(`#1`)로 들어온다는 점이다. HLE 모듈 테이블이 ordinal 대조를 지원해야 한다.

### 발견한 결함

**기본 타깃 선택이 틀렸다.** `re2dj --hdd <1st SE>`가 `Test.exe`를 고른다. 후보 순위가 "게스트 형식 GUI 우선, 같은 등급에서는 크기 내림차순"인데 서비스 도구(1.86 MB)가 게임(561 KB)보다 크기 때문이다.

크기가 "어느 것이 게임인가"의 근거가 못 된다는 것이 실제 덤프로 드러났다. 순위 휴리스틱을 더 정교하게 만드는 것은 또 다른 추측을 쌓는 일이므로 하지 않았다. 경로가 확인되었으니 내장 타깃 프로파일을 추가하는 것이 옳은 해법이고, 코드 변경이므로 별도 설계와 작업 지시가 필요하다. `docs/TODO.md`에 결함으로 기록했다.

**비ASCII 경로 출력이 깨진다.** 덤프 디렉터리 이름에 전각 문자가 있었을 때 `re2dj_hdd_probe`가 경로를 깨진 형태로 출력했다. 경로 해석 자체는 정상이었고 출력만 깨졌다. `std::filesystem::path::string()`이 Windows에서 활성 ANSI 코드 페이지로 변환하기 때문이다. 역시 `docs/TODO.md`에 기록했다.

## 갱신한 문서

`.gitignore`, `docs/analysis/ez2dj-hdd-layout.md`(전면 교체), `docs/analysis/ez2dj-import-surface.md`(신규), `docs/analysis/README.md`, `docs/EXE_DESIGN.ko.md`, `docs/EXE_DESIGN.en.md`, `ARCHITECTURE.md` 8절, `docs/TODO.md`.

## 검증

`git status --porcelain --untracked-files=all`의 항목 수가 41,735에서 71로 떨어진 것을 확인했다.

`git check-ignore -v`로 덤프 실행 파일이 `roms/` 규칙에 걸리는 것과, `README.md`·`src/exe/pe_image.cpp`·`docs/TODO.md`·`scripts/build.sh`가 어떤 규칙에도 걸리지 않는 것을 확인했다.

`re2dj_hdd_probe`와 `re2dj_pe_analyzer` 출력, 그리고 import 테이블 해석 결과를 문서 서술과 대조했다.

## 미검증

실행하지 않았다. 실행 backend가 없다. 실행 중에만 확인 가능한 항목(작업 디렉터리 변경, 열리는 파일 경로, 스레드 개수, DirectDraw 표면 구성, 보호 스텁이 `GetProcAddress`로 가져오는 API)은 전부 미확정으로 남겼다.

`Songs/` 아래 자산 파일 형식은 열어 보지 않았다. 이번 작업 범위가 실행 파일 식별과 API 표면이었다.

## Work Log: First Original Dump Static Analysis

## Result

Complete.

### Handled first: stopping the dump from leaking into the repository

While checking `git status` the tracking candidate count came back as **41,735**, because `.gitignore` did not cover `roms/`. When it was written the repository was empty and the anticipated dump directories were only `hdd/`, `HDD/`, and `MASTER/`.

`roms/`, `dump/`, `dumps/`, and `ez2dj/` were added along with the extensions a dump actually consists of. The candidate count then fell to **71**. A list of directory names alone proved insufficient, since the next dump may arrive under yet another name, so extension-based blocking backs it up.

**That fix immediately created a second problem.** Re-reading the candidate list showed four files missing from `src/hdd/` and `include/re2dj/hdd/`, because the `hdd/` entry is an unanchored pattern and therefore matches **at any depth**. A rule meant to block a dump had silently excluded this project's own source directory.

Every dump, build, and runtime-state directory pattern was anchored to the repository root with a leading `/`. After the fix the four files reappear as candidates while the dump and build tree stay excluded.

The lesson is that a new ignore rule needs both halves checked — **what must be blocked and what must not be**. Only the first was checked initially, which is why the second problem surfaced a step late.

### What was confirmed

1st SE holds 245 directories and 16,613 files with four executables; 3rd holds 199 directories and 25,054 files with one.

Three findings matter most. **Only `ez2dj1.exe` is unprotected** — the protected builds hold their entry point in a `.gtide` or `.protect` section, while `ez2dj1.exe` keeps its in `.text` and shares its first five sections' layout exactly with `ez2dj.exe`, making it the unprotected build of the same program and the bring-up target for Stages 2 and 3. **The import surface is small**: 144 functions from 7 DLLs, 97 of them KERNEL32 and many of those called by CRT startup. **Graphics is DirectDraw only and input is a single `GetAsyncKeyState`** — no Direct3D and no DirectInput, so the `d3d` and `dinput` entries `ARCHITECTURE.md` had anticipated are unnecessary for 1st. What was not anticipated is that DirectSound arrives as an **ordinal import** (`#1`), so the HLE module table must match ordinals.

### Defects found

**Default target selection is wrong**: `re2dj --hdd <1st SE>` picks `Test.exe`, because ties break by descending file size and the service tool is larger than the game. Refining the heuristic would only stack another guess, so it was left alone; with the paths now confirmed, built-in target profiles are the correct fix, and being a code change needing its own design and work order it was recorded as a defect in `docs/TODO.md`.

**Non-ASCII path output is mojibake**: with a full-width character in the dump directory name, `re2dj_hdd_probe` printed the path incorrectly while resolution itself worked, because `std::filesystem::path::string()` converts through the active ANSI code page on Windows. Also recorded in `docs/TODO.md`.

## Verification

The `git status` candidate count fell from 41,735 to 71. `git check-ignore -v` confirmed dump executables match the `roms/` rule while `README.md`, `src/exe/pe_image.cpp`, `docs/TODO.md`, and `scripts/build.sh` match no rule. Tool output and import parsing were compared against every documented statement.

## Not verified

Nothing was executed, as there is no execution backend, so every item only a run can settle remains unresolved. The asset file formats under `Songs/` were not opened; this task's scope was executable identification and the API surface.
