# scripts

빌드와 검증 진입점입니다. 모두 `CMakePresets.json`의 preset을 감쌉니다.

*Build and verification entry points. All of them wrap presets from `CMakePresets.json`.*

| 스크립트 | 호스트 | 내용 |
| --- | --- | --- |
| `build_win32_debug.bat` | Windows command prompt | Win32 Debug configure + build (`build.ps1` 실행) |
| `build_win32_release.bat` | Windows command prompt | Win32 Release configure + build + ctest (`build_release.ps1` 실행) |
| `build.ps1` | 64-bit Windows + WOW64 | Win32 runtime configure + build |
| `build_release.ps1` | 64-bit Windows + WOW64 | Win32 Release configure + build + ctest, 경고를 오류로 처리 |
| `test_all.ps1` | 64-bit Windows + WOW64 | Win32 runtime build + ctest, 경고를 오류로 처리 |
| `test_windows_native_helper_probe.ps1` | 64-bit Windows + WOW64 | Win32 x86 native helper probe build + ctest |
| `test_linux_native_helper_probe.sh` | Linux x86/x86-64 + i386 multilib | 두 product host가 production i386 helper를 실행하는 synthetic PE32 IPC integration 검증 |
| `build.sh` | Linux x86/x86-64 | configure + build (preset 선택) |
| `test_all.sh` | Linux x86/x86-64 | 경고를 오류로 하여 build + ctest |
| `build_release_linux.sh <linux-x64\|linux-x86> [version]` | Linux x86/x86-64 | Release preset build + ctest + `package_release.sh`, 경고를 오류로 처리 |
| `package_release.sh <linux-x64\|linux-x86> [version]` | Linux x86/x86-64 | Release `re2dj`를 tar.gz와 SHA256 파일로 묶음, 이식성 검사 포함 |
| `release/release_refs.py <tag>` | Python 3 (+ 선택 `gh`) | GitHub Release body 끝에 붙는 해결된 이슈·커밋 표를 Markdown으로 출력 (`release.yml`이 사용) |

`test_all` 계열은 `RE2DJ_WARNINGS_AS_ERRORS=ON`으로 configure합니다. CI에서만 걸리는 경고는 이미 기본 브랜치에 들어간 경고이기 때문입니다.

*The `test_all` scripts configure with `RE2DJ_WARNINGS_AS_ERRORS=ON`, because a warning caught only by CI is a warning that already reached the default branch.*

Windows command prompt에서는 Win32 Debug를 `scripts\build_win32_debug.bat`, Release를 `scripts\build_win32_release.bat`로 빌드합니다. 둘 다 어느 작업 디렉터리에서나 실행할 수 있습니다. 결과물은 각각 `build\windows-x86\bin\Debug`와 `build\windows-x86\bin\Release`에 생깁니다. 추가 인자는 PowerShell script로 넘어갑니다. 예를 들어 `scripts\build_win32_release.bat -SkipTests`는 테스트 없이 빌드합니다.

*From a Windows command prompt, build Win32 Debug with `scripts\build_win32_debug.bat` and Release with `scripts\build_win32_release.bat`. Both run from any working directory. Outputs go to `build\windows-x86\bin\Debug` and `build\windows-x86\bin\Release`. Extra arguments pass through to the PowerShell script; for example, `scripts\build_win32_release.bat -SkipTests` builds without tests.*

PowerShell script 실행이 시스템 policy로 제한된 환경에서는 `powershell -ExecutionPolicy Bypass -File scripts/<script>.ps1`로 현재 process에만 예외를 적용하거나, 표에 대응하는 CMake preset 명령을 직접 실행합니다.

*If system policy blocks PowerShell scripts, use `powershell -ExecutionPolicy Bypass -File scripts/<script>.ps1` for a process-local exception, or invoke the corresponding CMake preset commands directly.*

## Windows x86 Release

`scripts\build_release.ps1`는 `windows-x86-debug` preset을 configure에 재사용하면서 `Release` configuration으로 빌드하고, 기본적으로 Release CTest를 실행합니다. 테스트 없이 빌드하려면 `-SkipTests`를 지정합니다. 결과물은 `build\windows-x86\bin\Release`에 생성됩니다. Windows command prompt에서는 `scripts\build_win32_release.bat`를 사용합니다.

*`scripts\build_release.ps1` reuses the `windows-x86-debug` preset for configuration, builds the `Release` configuration, and runs Release CTest by default. Pass `-SkipTests` to build without tests. Outputs are written to `build\windows-x86\bin\Release`. Use `scripts\build_win32_release.bat` from Windows command prompt.*

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build_release.ps1
powershell -ExecutionPolicy Bypass -File scripts/build_release.ps1 -SkipTests
```

## GitHub Release package

GitHub Actions의 `release.yml`은 Windows x86, Linux x86-64, Linux x86 패키지를 각각 만들고, 태그 실행에서 세 플랫폼이 모두 성공하면 한 release에 올립니다. 패키지에는 원본 HDD·CHD 자산과 사용자의 `cfg/` Hardlock 자료가 들어가지 않습니다. release notes는 `docs/release-notes/v<version>.md`가 있으면 그것을 씁니다. 설계는 [작업 442](../docs/design/20261003-442-linux-release-artifacts.md)에 있습니다.

*`release.yml` in GitHub Actions builds the Windows x86, Linux x86-64 and Linux x86 packages, and on a tag run puts them in one release once all three succeed. No package carries original HDD or CHD assets or the user's `cfg/` Hardlock material. Release notes come from `docs/release-notes/v<version>.md` when present. See the [task 442 design](../docs/design/20261003-442-linux-release-artifacts.md).*

### Windows x86

`package_release.ps1`은 Release `re2dj.exe`(작업 450부터 DLL 없이 실행 파일 하나), 예제 `config/`, 사용자용 저장소 문서(`README.md`, `LICENSE`, `VERSION`, `RELEASE_NOTES.md`, `THIRD_PARTY_NOTICES.md`, `CREDITS.md`)를 Windows x86 zip으로 묶고 SHA256 파일을 씁니다. `package_release.bat`는 command prompt용 wrapper입니다.

*`package_release.ps1` collects the Release `re2dj.exe` (one executable with no DLL from task 450), the example `config/` and the user-facing repository documents (`README.md`, `LICENSE`, `VERSION`, `RELEASE_NOTES.md`, `THIRD_PARTY_NOTICES.md`, `CREDITS.md`) into a Windows x86 zip and writes a SHA256 file. `package_release.bat` is the command-prompt wrapper.*

```powershell
powershell -ExecutionPolicy Bypass -File scripts/package_release.ps1 -Configuration Release -Version 0.0.40
```

### Linux x86-64 / x86

`build_release_linux.sh`가 `linux-x64-release` 또는 `linux-x86-release` preset을 경고를 오류로 하여 빌드하고, CTest를 돌린 뒤 `package_release.sh`를 부릅니다. release preset은 libstdc++와 libgcc를 정적으로 링크합니다. `package_release.sh`는 strip한 `re2dj`와 Windows와 같은 문서·`config/`를 `build/package/re2dj-v<version>-linux-<arch>.tar.gz`로 묶고 `.sha256`을 씁니다. tar.gz는 실행 권한을 보존하고, 같은 이름의 최상위 디렉터리를 담습니다.

*`build_release_linux.sh` builds the `linux-x64-release` or `linux-x86-release` preset with warnings as errors, runs CTest, then calls `package_release.sh`; the release presets link libstdc++ and libgcc statically. `package_release.sh` bundles the stripped `re2dj` with the Windows package's documents and `config/` into `build/package/re2dj-v<version>-linux-<arch>.tar.gz` and writes `.sha256`; the tar.gz keeps the executable bit and holds a top-level directory of the same name.*

패키징 전에 이식성을 검사하고, 어긋나면 실패합니다.

- `file`: ELF 폭(x86-64 / Intel 80386)
- `readelf -d`: NEEDED가 `libc`, `libm`, `libdl`, `libpthread`, `librt`, `ld-linux*`뿐인지. SDL은 X11·Wayland·GL·오디오 라이브러리를 실행 시점에 `dlopen`합니다.
- `objdump -T`: 가장 높은 `GLIBC_` 심볼 버전이 `RE2DJ_MAX_GLIBC`(기본 2.36, Debian 12) 이하인지

*Before packaging it checks portability and fails on a mismatch: the ELF width with `file` (x86-64 / Intel 80386); that `readelf -d`'s NEEDED holds only `libc`, `libm`, `libdl`, `libpthread`, `librt` and `ld-linux*`, since SDL `dlopen`s the X11, Wayland, GL and audio libraries at run time; and that `objdump -T`'s highest `GLIBC_` symbol version is at most `RE2DJ_MAX_GLIBC` (2.36 by default, Debian 12).*

workflow는 이것을 Debian 12 컨테이너(`debian:bookworm`, `i386/debian:bookworm`)에서 실행합니다. 개발 머신의 glibc가 더 새로우면 GLIBC 검사에 걸리므로, 로컬 시험에서는 한도를 올립니다.

*The workflow runs this in Debian 12 containers (`debian:bookworm`, `i386/debian:bookworm`). A development machine with a newer glibc fails the GLIBC check, so a local trial raises the limit:*

```bash
RE2DJ_MAX_GLIBC=2.99 bash scripts/build_release_linux.sh linux-x64
```
