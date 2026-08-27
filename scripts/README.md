# scripts

빌드와 검증 진입점입니다. 모두 `CMakePresets.json`의 preset을 감쌉니다.

*Build and verification entry points. All of them wrap presets from `CMakePresets.json`.*

| 스크립트 | 호스트 | 내용 |
| --- | --- | --- |
| `build.ps1` | 64-bit Windows + WOW64 | Win32 runtime configure + build |
| `test_all.ps1` | 64-bit Windows + WOW64 | Win32 runtime build + ctest, 경고를 오류로 처리 |
| `test_windows_native_helper_probe.ps1` | 64-bit Windows + WOW64 | Win32 x86 native helper probe build + ctest |
| `test_linux_native_helper_probe.sh` | Linux x86-64 + i386 multilib | production i386 helper의 synthetic PE32 IPC integration 검증 |
| `build.sh` | Linux x86-64 | configure + build |
| `test_all.sh` | Linux x86-64 | 경고를 오류로 하여 build + ctest |

`test_all` 계열은 `RE2DJ_WARNINGS_AS_ERRORS=ON`으로 configure합니다. CI에서만 걸리는 경고는 이미 기본 브랜치에 들어간 경고이기 때문입니다.

*The `test_all` scripts configure with `RE2DJ_WARNINGS_AS_ERRORS=ON`, because a warning caught only by CI is a warning that already reached the default branch.*

PowerShell script 실행이 시스템 policy로 제한된 환경에서는 `powershell -ExecutionPolicy Bypass -File scripts/<script>.ps1`로 현재 process에만 예외를 적용하거나, 표에 대응하는 CMake preset 명령을 직접 실행합니다.

*If system policy blocks PowerShell scripts, use `powershell -ExecutionPolicy Bypass -File scripts/<script>.ps1` for a process-local exception, or invoke the corresponding CMake preset commands directly.*
