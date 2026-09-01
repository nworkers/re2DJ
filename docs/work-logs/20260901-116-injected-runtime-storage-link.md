# injected runtime CHD 저장소 링크 수정 작업 로그

## 한국어

### 결과

실제 `ez2dj4th --run` 재현에서 확인된 `cannot find bundled injected runtime`의 원인은 Debug x86 `re2dj_windows_injected_runtime.dll` 링크 실패였습니다. `fat32_chd.cpp`가 사용하는 `EqualsIgnoreAsciiCase` 정의가 `re2dj_core`에만 있어 original-process DLL의 `LNK2019`가 발생했습니다.

`src/storage/guest_path.cpp`를 `re2dj_storage_common`으로 분리하고 `re2dj_chd_storage`가 PUBLIC 링크하도록 CMake를 수정했습니다. `re2dj_core`에서 중복 소스 항목을 제거해 공용 경로 구현이 한 번만 링크되도록 했습니다. 또한 CHD 전용 4th 프로파일을 디렉터리 HDD fingerprint 매칭에서 제외했습니다. 이 guard가 없으면 3rd와 같은 추출 디렉터리 모양을 4th로 중복 매칭하여 기존 target profile 테스트가 실패합니다.

### 검증

다음 검증이 통과했습니다.

```text
cmake --build build\windows-x86 --config Debug --target re2dj_windows_injected_runtime
cmake --build build\windows-x86 --config Debug --target re2dj
cmake --build build\windows-x86 --config Debug --target re2dj_unit_tests
build\windows-x86\bin\Debug\re2dj_unit_tests.exe
```

Debug DLL과 제품 executable이 생성되었고 단위 테스트는 `checks: 1030, failures: 0`이었습니다. 전체 CTest는 첫 번째 `re2dj_windows_vfs_runtime_probe`가 90초 이상 진행되어 Ctrl+C로 중단했으며, 해당 probe 프로세스도 종료되었음을 확인했습니다.

수정 후 사용자의 실제 명령을 다시 실행하자 DLL 탐색 오류는 사라졌고, 다음 경계인 `cannot resolve valid bring-up target`이 노출되었습니다. 이는 launcher가 CHD staging 디렉터리를 다시 스캔하면서 CHD 전용 `ez2dj4th` 프로파일을 찾지 못하는 별도 handoff 문제로 분리했습니다. 다음 작업에서 부모 CLI가 선택한 executable/profile을 launcher에 명시적으로 전달합니다.

## English

### Result

The `cannot find bundled injected runtime` reproduced by the real `ez2dj4th --run` command was caused by a Debug x86 link failure for `re2dj_windows_injected_runtime.dll`. `fat32_chd.cpp` referenced `EqualsIgnoreAsciiCase`, but its definition lived only in `re2dj_core`, so the original-process DLL link reported `LNK2019`.

`src/storage/guest_path.cpp` is now compiled by `re2dj_storage_common`, linked PUBLIC by `re2dj_chd_storage`, and no longer listed directly in `re2dj_core`. The CHD-only 4th profile is also skipped by directory-HDD fingerprint matching; otherwise an extracted directory shaped like 3rd would be claimed twice and fail the existing target-profile test.

### Verification

The Debug injected runtime target, product executable target, unit-test target, and unit-test executable all completed successfully. The unit suite reported `checks: 1030, failures: 0`, and the Debug DLL and executable were present under `build\windows-x86\bin\Debug`.

The full CTest run was stopped with Ctrl+C after the first `re2dj_windows_vfs_runtime_probe` continued beyond 90 seconds; the probe process was also confirmed to have exited.

Rerunning the user's real command after the fix removed the DLL-discovery error and exposed the next boundary: `cannot resolve valid bring-up target`. This is a separate handoff issue where the launcher rescans the CHD staging directory and cannot find a CHD-only `ez2dj4th` profile. The next task will pass the parent CLI's selected executable/profile explicitly to the launcher.
