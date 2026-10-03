# 작업 442 작업 로그 — Linux 릴리스 산출물 / Task 442 work log — Linux release artifacts

설계: [20261003-442-linux-release-artifacts.md](../design/20261003-442-linux-release-artifacts.md) · 지시서: [20261003-442-linux-release-artifacts.md](../work-orders/20261003-442-linux-release-artifacts.md)

## 2026-10-03

- **구현**
  - `CMakePresets.json`: `linux-x64-release`와 `linux-x86-release`의 링크에 `-static-libstdc++ -static-libgcc`를 더했다. x86은 `-m32`를 함께 둔다.
  - `scripts/package_release.sh`(새로 만듦): ELF 폭, NEEDED 허용 목록, 최대 GLIBC 버전을 검사하고 tar.gz와 `.sha256`을 만든다.
  - `scripts/build_release_linux.sh`(새로 만듦): Release preset build, CTest, 패키징.
  - `scripts/package_release.ps1`: Windows zip에 `THIRD_PARTY_NOTICES.md`와 `CREDITS.md`를 더했다.
  - `.github/workflows/release.yml`: `version`, `windows-x86`, `linux`(x64·x86 matrix), `publish` job으로 나눴다.
    - Linux 빌드는 `docker run`으로 Debian 12 컨테이너에서 한다. x86은 `setarch i686`으로 감싼다.
    - 컨테이너는 `seccomp=unconfined`로 돈다(게스트 런타임의 `modify_ldt` 때문).
  - `.github/workflows/ci.yml`: linux-x64에 오디오 헤더를, i386 컨테이너의 `linux-x86` job을 더했다.
  - 사이트: `releases.py`의 플랫폼 분류(`asset_platform`, `Release.packages`, `checksum_for`), `download.html`의 플랫폼별 버튼과 표, ko·en 문구(Linux 시작하기·요구사항, 쓰지 않게 된 `primary` 제거).
  - 문서: `scripts/README.md`, `docs/sites/README.md`, `ARCHITECTURE.md`.

  *Implementation:*
  - *`CMakePresets.json`: `linux-x64-release` and `linux-x86-release` link with `-static-libstdc++ -static-libgcc`, x86 keeping `-m32`.*
  - *`scripts/package_release.sh` (new) checks the ELF width, the NEEDED allow-list and the highest GLIBC version, then makes the tar.gz and `.sha256`.*
  - *`scripts/build_release_linux.sh` (new): the Release preset build, CTest and packaging.*
  - *`scripts/package_release.ps1`: the Windows zip gains `THIRD_PARTY_NOTICES.md` and `CREDITS.md`.*
  - *`.github/workflows/release.yml`: split into `version`, `windows-x86`, `linux` (an x64 and x86 matrix) and `publish` jobs. The Linux build runs in a Debian 12 container through `docker run`, x86 under `setarch i686`, with `seccomp=unconfined` for the guest runtime's `modify_ldt`.*
  - *`.github/workflows/ci.yml`: the audio headers for linux-x64 and a `linux-x86` job in the i386 container.*
  - *The site: platform classification in `releases.py` (`asset_platform`, `Release.packages`, `checksum_for`), per-platform buttons and table links in `download.html`, and the ko and en text (Linux getting started and requirements, the now unused `primary` removed).*
  - *Documents: `scripts/README.md`, `docs/sites/README.md`, `ARCHITECTURE.md`.*
- **설계 중 확인한 제약**
  - GitHub JavaScript action은 64비트 node를 컨테이너 안에서 실행하므로 32비트 컨테이너 job에서 돌지 않는다. 그래서 `container:` 대신 `docker run`을 썼다.
  - Docker 기본 seccomp 프로필은 `modify_ldt`를 막는다. 게스트 실행과 probe 테스트가 LDT를 쓴다.
  - Ubuntu 22.04의 glibc는 2.35라 Debian 12(2.36) 빌드가 돌지 않는다. 사이트 요구사항은 "Ubuntu 23.04 이후"로 적었다.

  *Constraints found while designing: GitHub's JavaScript actions run a 64-bit node inside the container, so they do not work in a 32-bit container job, hence `docker run` instead of `container:`; Docker's default seccomp profile refuses `modify_ldt`, which the guest runs and probe tests use; Ubuntu 22.04's glibc is 2.35, so a Debian 12 (2.36) build does not run there, and the site's requirement reads "Ubuntu 23.04 onward".*
- **검증**(Ubuntu 26.04, x64)
  - `linux-x64-release`(경고를 오류로) build 성공, CTest 4개 통과.
  - `package_release.sh linux-x64`: 기본 한도에서는 `GLIBC_2.43`이 2.36보다 새로워 의도대로 실패했다. `RE2DJ_MAX_GLIBC=2.99`에서는 패키지를 만들었고, NEEDED는 `libm.so.6 libc.so.6 ld-linux-x86-64.so.2`뿐이었다(libstdc++ 정적 링크 확인).
  - tar 내용: 최상위 `re2dj-v0.0.59-linux-x64/`, `re2dj`(755, strip), 문서 6개, `config/`. `.sha256`을 `sha256sum -c`로 확인했다.
  - 푼 패키지의 `re2dj`: `--version` 동작. 저장소 디렉터리에서 6th 실행 시 launcher → 6th 자식, 오디오, Hardlock 재료 적용, 25초 `Flip` 반복. `./re2dj ez2dj6th` 형태도 동작한다.
  - 푼 디렉터리에서 직접 실행하면 "Hardware lock is not found!"로 끝났다. Hardlock 재료는 현재 디렉터리의 `cfg/`에서 읽고, Windows 패키지와 같이 패키지에 담지 않는다. 의도된 동작이다.
  - 사이트: `build_site.py --offline` 통과(내부 링크 검사 포함). 가짜 API 응답(세 플랫폼 + 체크섬, Windows만 있는 옛 릴리스)으로 빌드했다. 버튼 3개가 Windows → Linux x86-64 → Linux x86 순으로 나오고, 산출물이 플랫폼별로 묶이며, 옛 릴리스 표가 그대로인 것을 확인했다. 이 머신에 `mdit-py-plugins`가 없어 requirements의 고정 버전 wheel을 scratchpad에 풀어 썼다.
  - workflow: PyYAML로 세 파일을 파싱했다. linux matrix 두 항목의 `docker run` 명령을 펼쳐 `bash -n`으로 문법을 확인했다.
  - **하지 못한 것**: 이 머신에는 Docker가 없고 GitHub Actions를 실행할 수 없다. 그래서 Debian 12 컨테이너 빌드, i386 빌드와 테스트, workflow 실행(artifact, publish)은 확인하지 못했다. 사용자가 `workflow_dispatch`로 확인한다. Windows 패키지 스크립트 변경도 MSVC·PowerShell이 없어 실행하지 못했다.

  *Verification (Ubuntu 26.04, x64): the `linux-x64-release` build (warnings as errors) and 4 CTest tests pass. `package_release.sh linux-x64` failed as intended at the default limit, `GLIBC_2.43` being newer than 2.36; with `RE2DJ_MAX_GLIBC=2.99` it packaged, NEEDED being only `libm.so.6 libc.so.6 ld-linux-x86-64.so.2` (libstdc++ static). The tar holds the top-level `re2dj-v0.0.59-linux-x64/`, `re2dj` (755, stripped), six documents and `config/`, and `sha256sum -c` accepts the `.sha256`. The extracted `re2dj` answers `--version`; run from the repository directory on 6th it went launcher → 6th child with audio and the Hardlock material applied, repeating `Flip` for 25 seconds, and `./re2dj ez2dj6th` works too. Run from the extracted directory it ended with "Hardware lock is not found!": the Hardlock material is read from `cfg/` in the current directory and, as with the Windows package, is not packaged, which is intended. The site: `build_site.py --offline` passes with its link check, and a build on made-up API data (three platforms with checksums, plus an older Windows-only release) shows three buttons in the order Windows, Linux x86-64, Linux x86, assets grouped by platform, and the older table unchanged; `mdit-py-plugins` is not on this machine, so the requirements' pinned wheels were unpacked into the scratchpad. The workflows: all three files parse with PyYAML, and both linux matrix entries' `docker run` commands, expanded, pass `bash -n`. **Not done**: this machine has no Docker and cannot run GitHub Actions, so the Debian 12 container build, the i386 build and tests, and the workflow run (artifacts, publish) are unverified and left to the user through `workflow_dispatch`; the Windows packaging change could not run either, without MSVC or PowerShell.*

## 2026-10-03 — 첫 workflow 실행 / First workflow run

- **실행 `37102420867`**(사용자가 `workflow_dispatch`로 실행): 빌드 job 세 개가 모두 실패했다.
  - Linux x64·x86: Debian 12의 GCC 12가 `-O3`에서 `import_dispatcher.cpp:117`의 `"#" + std::to_string(...)`에 `-Wrestrict`를 냈고, `-Werror`로 멈췄다. GCC 12의 알려진 오탐이다([GCC bug 105651](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=105651)). 패키지 설치와 configure는 두 컨테이너 모두 통과했다.
  - Windows x86: `CMake Error: Could not create named generator Visual Studio 18 2026`. `build_release.ps1`이 쓰는 `windows-x86-debug` preset의 생성기가 `windows-2022` runner에 없다. 이번 변경 전부터 그랬다(`v0.0.58` Release도 7초 만에 실패했다).
- **수정**
  - `re2dj_warnings`: GCC 12.x에서만 `-Wno-restrict`.
  - Windows job: `ci.yml`처럼 preset 없이 `cmake -S . -B build/windows-x86 -A Win32`로 configure하고 Release로 빌드한다.
  - main의 `ci` 실행(`36853864649`)에서 clang이 `fat32_chd.cpp:754`의 항상 거짓인 비교(`uint32_t` > `size_t` 최대값)로 실패한 것도 같이 고쳤다. 실행 중 검사를 `static_assert`로 바꿨다. 같은 실행의 Windows 테스트 실패는 로그 저장소 연결 오류로 내용을 받지 못했다.

  *Run `37102420867` (started by the user through `workflow_dispatch`) failed in all three build jobs. On Linux x64 and x86, Debian 12's GCC 12 at `-O3` reported `-Wrestrict` on `"#" + std::to_string(...)` at `import_dispatcher.cpp:117`, stopped by `-Werror`, a known GCC 12 false positive ([GCC bug 105651](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=105651)); package installation and configuration passed in both containers. On Windows x86, `CMake Error: Could not create named generator Visual Studio 18 2026`: the generator of the `windows-x86-debug` preset that `build_release.ps1` uses is not on the `windows-2022` runner, which predates this change (the `v0.0.58` Release also failed in seven seconds). Fixes: `-Wno-restrict` on GCC 12.x only in `re2dj_warnings`; the Windows job configures without a preset, as `ci.yml` does, with `cmake -S . -B build/windows-x86 -A Win32`, and builds Release. Also fixed: main's `ci` run (`36853864649`) failed on clang at `fat32_chd.cpp:754`, an always-false comparison of a `uint32_t` with the `size_t` maximum, now a `static_assert`; that run's Windows test failure could not be read, the log store refusing the connection.*
- **실행 `37103222535`**: 세 job 모두 더 나아갔지만 다시 실패했다.
  - Windows는 configure·빌드를 통과했다. 단위 테스트에서 `guest_files_test.cpp:288` 한 곳이 실패했다. 작업 437에서 넣은 `!exists(overlay / "SAVE")` 검사가, 대소문자를 구분하지 않는 Windows 파일 시스템에서는 성립하지 않는다. 바로 위의 디렉터리 항목 개수 검사가 같은 것을 두 OS에서 확인하므로 그 줄을 뺐다.
  - Linux 두 폭은 GCC 12가 `bitmap_file_test.cpp`의 `std::vector::insert`에 `-Warray-bounds`를 냈다([GCC bug 107852](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=107852)). GCC 12의 같은 계열 오탐(`restrict`, `array-bounds`, `stringop-overflow`, `stringop-overread`)을 GCC 12.x에서만 오류로 승격하지 않게 했다(`-Wno-error=`). 경고는 로그에 남는다.

  *Run `37103222535` got further in all three jobs but failed again. Windows configured and built, and one unit check failed, `guest_files_test.cpp:288`: task 437's `!exists(overlay / "SAVE")` does not hold on Windows' case-insensitive file system; the directory-entry count just above checks the same on both systems, so that line went. On both Linux widths GCC 12 reported `-Warray-bounds` on `std::vector::insert` in `bitmap_file_test.cpp` ([GCC bug 107852](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=107852)); GCC 12's false positives of that family (`restrict`, `array-bounds`, `stringop-overflow`, `stringop-overread`) are no longer promoted to errors, on GCC 12.x only (`-Wno-error=`), and stay in the log as warnings.*
- **실행 `37103694482`**: Linux x64·x86 통과. Windows는 빌드와 단위 테스트를 통과한 뒤 `Test VFS enumeration probe`가 출력 없이 1로 끝났다.
  - 예전 Release 실행(v0.0.46~v0.0.58)은 대부분 결제 문제로 job이 시작되지 않았거나 생성기 문제로 실패했다. 그래서 이 단계는 CI에서 실제로 돈 적이 없었다.
- **실행 `37104275003`**: 단계를 `cmd` 셸로 바꾸고, probe `Check`의 `"\\n"`(글자 그대로)을 줄바꿈으로 고쳤다. 그래도 출력 없이 `-1073740791`(`0xC0000409`)로 끝났다.
- **실행 `37104803972`**: 실패하면 Windows SDK의 `cdb`로 probe를 다시 돌리는 진단 단계를 더했다.
  - 결과: second chance C++ 예외(`e06d7363`)였다. 스택은 `_CxxThrowException` ← probe의 `main`이었다.
  - `Check` 실패 메시지가 없었으므로 모든 검사는 통과한 것이다. 그 뒤 enumeration 모드 끝의 `std::filesystem::remove_all(root)`가 런타임이 아직 열어 둔 `vfs.log` 때문에 예외를 던진 것으로 판단했다.
  - 전체 모드의 끝에는 같은 경합을 피하는 정리(trace 경로 비우기, `Re2djCloseRuntimeLogs()`, `error_code`를 받는 `remove_all`)가 이미 있었다. enumeration 모드에도 그것을 적용했다. 설정 실패 경로의 `remove_all`도 `error_code`를 받게 했다.
- **실행 `37105285093`: 전체 성공.** Version, Windows x86, Linux x64, Linux x86이 통과했다. publish는 태그가 없는 수동 실행이라 설계대로 건너뛰었다.
  - artifact: `re2dj-v0.0.59-windows-x86.zip`(1.46 MB), `-linux-x64.tar.gz`(3.12 MB), `-linux-x86.tar.gz`(3.17 MB)와 각 `.sha256`. Windows zip에 `THIRD_PARTY_NOTICES.md`와 `CREDITS.md`가 들어 있다.
- **CI 산출물의 로컬 확인**(앞 실행 `37103694482`의 artifact):
  - 두 패키지 모두 체크섬이 맞다. NEEDED는 `libm`, `libc`, `ld-linux*`뿐이고, 최대 요구 glibc는 `GLIBC_2.36`이다.
  - x64 실행 파일: 저장소 디렉터리에서 `re2dj ez2dj6th`로 launcher → 6th 자식, 오디오, Hardlock 재료 적용, 25초 `Flip` 반복.
  - x86 실행 파일: 이 64비트 머신의 32비트 라이브러리로 같은 실행이 20초 동안 됐다.
  - 이 머신의 새 `file`은 32비트를 "Intel i386"으로 표시하므로, `package_release.sh`의 폭 검사가 "Intel 80386"과 둘 다 받게 했다.
- **남은 것**
  - 태그 실행의 publish 단계는 태그를 올릴 때 처음 돈다.
  - main의 `ci` Windows job은 전체 probe(창·오디오 lifecycle 포함)를 돌린다. 이번 수정 뒤 결과는 main에 머지한 뒤 확인된다.
  - Windows zip 항목 이름이 `config\...`처럼 백슬래시로 저장된다(PowerShell 5 `Compress-Archive`). Windows에서는 문제가 없고, 이번 작업 전부터 그랬다.

  *Run `37103694482`: Linux x64 and x86 passed. Windows built and passed its unit tests, then `Test VFS enumeration probe` ended with 1 and no output; earlier Release runs (v0.0.46 to v0.0.58) mostly never started, for billing, or failed on the generator, so this step had never really run in CI. Run `37104275003`: with the step under `cmd` and the probe's `Check` printing a newline instead of a literal `"\\n"`, it still ended with no output, now `-1073740791` (`0xC0000409`). Run `37104803972`: a step reran the probe under the Windows SDK's `cdb` on failure, showing a second-chance C++ exception (`e06d7363`) from `_CxxThrowException` in the probe's `main`. With no `Check` message every check had passed, and the enumeration-only path's final `std::filesystem::remove_all(root)` is judged to have thrown on the `vfs.log` the runtime still held open; the full run's end already avoided that race (clearing the trace paths, `Re2djCloseRuntimeLogs()`, `remove_all` with an `error_code`), which the enumeration-only path now does too, and the configuration-failure path's `remove_all` takes an `error_code` as well. **Run `37105285093` passed in full**: version, Windows x86, Linux x64 and Linux x86, with publish skipped on a tag-less manual run as designed; the artifacts are `re2dj-v0.0.59-windows-x86.zip` (1.46 MB), `-linux-x64.tar.gz` (3.12 MB), `-linux-x86.tar.gz` (3.17 MB) and their `.sha256`, and the Windows zip carries `THIRD_PARTY_NOTICES.md` and `CREDITS.md`. Checked locally on run `37103694482`'s artifacts: both checksums match, NEEDED holds only `libm`, `libc` and `ld-linux*`, and the highest glibc needed is `GLIBC_2.36`; the x64 executable ran `re2dj ez2dj6th` from the repository directory through launcher → 6th child with audio and the Hardlock material for 25 seconds of `Flip`, and the x86 executable did the same for 20 seconds on this 64-bit machine's 32-bit libraries. This machine's newer `file` prints "Intel i386" for 32-bit, so `package_release.sh` accepts it alongside "Intel 80386". Left: a tag run's publish step first runs when a tag is pushed; main's `ci` Windows job runs the full probe, window and audio lifecycle included, and shows its result once merged; the Windows zip stores entry names with backslashes such as `config\...` (PowerShell 5's `Compress-Archive`), harmless on Windows and older than this task.*
