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

`package_release.ps1` collects the Release `re2dj.exe`, injected runtime DLL, example `config/`, and user-facing repository documents into a Windows x86 zip and writes a SHA256 sidecar file. It does not include original HDD or CHD assets. `package_release.bat` is the command-prompt wrapper.

```powershell
powershell -ExecutionPolicy Bypass -File scripts/package_release.ps1 -Configuration Release -Version 0.0.40
```

*`package_release.ps1` collects the Release `re2dj.exe`, injected runtime DLL, example `config/`, and user-facing repository documents into a Windows x86 zip and writes a SHA256 sidecar file. It never includes original HDD or CHD assets. `package_release.bat` is the command-prompt wrapper. The GitHub Actions workflow uses this package and reads optional notes from `docs/release-notes/v<version>.md`.*
