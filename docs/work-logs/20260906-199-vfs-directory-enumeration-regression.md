# 작업 로그 199: 디렉터리 기반 VFS 열거 회귀 수정
# Work Log 199: Directory-Backed VFS Enumeration Regression Fix

설계: [Task 199 설계](../design/20260906-199-vfs-directory-enumeration-regression.md), 작업 지시: [Task 199](../work-orders/20260906-199-vfs-directory-enumeration-regression.md)

*Design: [Task 199 design](../design/20260906-199-vfs-directory-enumeration-regression.md); work order: [Task 199](../work-orders/20260906-199-vfs-directory-enumeration-regression.md).* 

## 원인 확인 / Cause confirmation

사용자 실행 `20260906-000419-856`의 VFS 로그는 `System\Title` 현재 디렉터리 설정 성공 뒤 `FindFirstFileA("*.*")`가 `find-first-fallback`으로 처리되고, 이어서 `SetCurrentDirectoryA("logs")`가 `System/Title/logs`에서 실패하는 순서를 기록했습니다. 호스트 작업 디렉터리는 `ez2dj`로 유지되므로 기존 fallback은 게스트 CWD가 아니라 호스트 root를 검색했습니다. 같은 실행의 DDraw trace에는 이 종료 직전의 draw failure 또는 unsupported blend가 없었습니다.

*The user's `20260906-000419-856` VFS trace records successful entry into guest directory `System\Title`, `FindFirstFileA("*.*")` handled by `find-first-fallback`, and then `SetCurrentDirectoryA("logs")` failing at `System/Title/logs`. Because the host working directory remains `ez2dj`, the old fallback searched the host root instead of the guest CWD. The same run has no draw failure or unsupported blend record immediately before the stop.*

이것은 코드에서 직접 확인되는 회귀입니다. `FindFirstFileA` HLE가 CHD가 아닐 때 원래 검색 문자열을 native API에 전달하고 있었고, Task 177의 논리 게스트 CWD는 파일 열기에는 사용되지만 directory-backed 열거에는 연결되어 있지 않았습니다.

*This is a code-level regression confirmed directly in the implementation: when CHD was not active, the `FindFirstFileA` HLE passed the original search string to the native API, while Task 177's logical guest CWD was used for file opens but not connected to directory-backed enumeration.*

## 구현 / Implementation

- `MapVfsSearchPath`를 추가해 검색 패턴의 마지막 구성요소(`*.*`, `*.abm` 등)는 보존하고, 앞의 디렉터리 부분만 기존 `MapVfsPath`로 해석하도록 했습니다.
- directory-backed `FindFirstFileA`는 `HDD root\guest CWD\pattern`을 native API에 전달하고 mapped path, 성공 여부, 오류를 VFS trace에 기록합니다.
- CHD 합성 enumeration 및 native handle의 `FindNextFileA`/`FindClose` 전달은 변경하지 않았습니다.
- VFS runtime probe에 `System\Title`와 root `logs`를 함께 만든 뒤 guest-relative `*.*` 열거가 title 항목만 반환하는 회귀 검사를 추가했습니다. `--vfs-enumeration-only` 옵션으로 DirectSound 종료 검사와 분리해 실행할 수 있습니다.
- 프로브의 stale root-relative 경로 기대값을 현재 게스트 경로 설계와 일치하는 성공 검사로 정정하고, 동적 resolver 및 CHD 초기 상태를 명시적으로 설정했습니다.

*Added `MapVfsSearchPath`, preserving the final search-pattern component such as `*.*` or `*.abm` while resolving only the directory portion through the existing `MapVfsPath`. Directory-backed `FindFirstFileA` now passes `HDD root\guest CWD\pattern` to the native API and records the mapped path, result, and error. CHD synthetic enumeration and native-handle `FindNextFileA`/`FindClose` forwarding are unchanged. The VFS runtime probe now creates both `System\Title` and root `logs`, verifies guest-relative enumeration, and supports `--vfs-enumeration-only` so this test can run independently of the DirectSound shutdown check. The probe's stale root-relative-path expectation was also aligned with the current guest-path design, and its dynamic-resolver/CHD initial state is now explicit.*

## 검증 / Verification

- `cmd /c scripts\build_win32.bat`: 성공.
- `re2dj_windows_vfs_runtime_probe.exe --vfs-enumeration-only`: 성공. `System\Title\*.*` native mapping과 title entry 검사가 통과했습니다.
- `re2dj_unit_tests.exe`: `checks: 1265, failures: 0`.
- `ctest --test-dir build\windows-x86 -C Debug -R re2dj_unit_tests --output-on-failure`: 1/1 통과.
- `git diff --check`: 통과.

*`cmd /c scripts\build_win32.bat` passed. `re2dj_windows_vfs_runtime_probe.exe --vfs-enumeration-only` passed, including the `System\Title\*.*` native mapping and title-entry checks. `re2dj_unit_tests.exe` passed with 1265 checks and zero failures. The unit CTest passed 1/1, and `git diff --check` passed.*

## 제한 및 사용자 재검증 / Limits and user revalidation

전체 VFS runtime probe는 기존 DirectSound 수명주기 검사 종료부에서 3분 이상 반환하지 않아 종료했습니다. 새 열거 검사는 그 전에 통과했으므로 `--vfs-enumeration-only`로 분리 검증했습니다. 직접 실행한 product-loader probe는 이 작업에 필요한 원본 프로세스 옵션 없이 실행해 `invalid Windows original-process options`로 끝났으므로 제품 검증으로 사용하지 않았습니다.

*The full VFS runtime probe was stopped after it failed to return for more than three minutes in its existing DirectSound lifecycle teardown. The new enumeration check had already passed, so it was verified separately with `--vfs-enumeration-only`. A standalone product-loader probe invocation was not used as product verification because it was run without the original-process options required by that probe and returned `invalid Windows original-process options`.*

아직 사용자가 새 빌드로 `ez2dj1stse`에 코인을 입력해 종료 여부를 재검증하지 않았습니다. 다음 실행의 `.vfs.log`에서 기존 `find-first-fallback:name=*.*` 대신 `find-first:native:...:mapped=...\System\Title\*.*`가 보이고, `SetCurrentDirectoryA("logs")` 실패가 사라지는지 확인해야 합니다. 그래도 종료되면 그 시점의 새 종료 경계와 입력/장치 기록을 이어서 분석합니다.

*The user has not yet revalidated coin insertion with the new build. In the next run's `.vfs.log`, look for `find-first:native:...:mapped=...\System\Title\*.*` instead of `find-first-fallback:name=*.*`, and confirm that the `SetCurrentDirectoryA("logs")` failure is gone. If the game still exits, continue with a new termination-boundary and input/device trace at that point.*
