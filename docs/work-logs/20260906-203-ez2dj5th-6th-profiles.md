# ez2dj5th·ez2dj6th 프로파일 추가 작업 로그

## 결과

`ez2dj4th`의 CHD/HLE 기본 정책을 기준으로 `ez2dj5th`와 `ez2dj6th` built-in 프로파일을 추가했습니다. 각 프로파일은 별도의 `hle_profile_id`와 `roms/ez2dj5th`, `roms/ez2dj6th` CHD shortcut을 가집니다. 개발용 `ez2dj1stse_unpacked` built-in 프로파일은 제거했습니다.

1st SE 디렉터리에서 `ez2dj1.exe`가 다시 발견되는 경우에는 built-in이 아닌 `ez2dj1` detected 항목으로 유지되도록 실행 관찰 도구의 기본 target도 갱신했습니다. 원본 실행 파일과 CHD는 저장소 변경에 포함하지 않았습니다.

## 확인된 입력

- 6th CHD는 `EZ2DJ/EZ2DJ.EXE`를 포함하는 FAT32 구조로 판독되었고, 해당 파일은 PE32/i386입니다.
- 5th CHD는 현재 `Fat32Volume`에서 `CHD MBR has no in-range FAT32 partition`으로 거부됩니다. 이 작업에서는 FAT16 또는 다른 파일시스템 reader를 구현하지 않았으므로 5th의 내부 실행 파일 경로와 실제 실행 성공은 미확정으로 남겼습니다.

## 검증

- Windows x86 Debug build: 성공 (`scripts/build_win32.bat`)
- `re2dj_unit_tests.exe`: `checks: 1369, failures: 0`
- `re2dj_windows_product_loader_probe.exe`: `profile-defaults=ok second-defaults=ok unsupported-target=ok resolve-iat-slot=ok`
- `re2dj.exe ez2dj6th --list-targets`: `ez2dj6th` built-in target과 FAT32 CHD 경로 출력 확인
- `re2dj.exe ez2dj5th --list-targets`: 위 reader 제약으로 예상된 FAT32 partition 오류 확인
- 전체 CTest: 첫 번째 `re2dj_windows_vfs_runtime_probe`가 약 5분 동안 출력 없이 대기하여 중단했습니다. 이 probe는 이번 프로파일·target profile test 변경과 직접 관련된 실행 경로가 아닙니다.
- `git diff --check`: 성공

---

# ez2dj5th·ez2dj6th Profile Addition Work Log

## Result

Added the `ez2dj5th` and `ez2dj6th` built-in profiles using the `ez2dj4th` CHD/HLE defaults. Each profile has its own `hle_profile_id` and `roms/ez2dj5th` or `roms/ez2dj6th` CHD shortcut. Removed the development-only `ez2dj1stse_unpacked` built-in profile.

When `ez2dj1.exe` is found again in a 1st SE directory, it remains a detected `ez2dj1` entry rather than a built-in profile, and the execution-observation tools were updated to use that detected target by default. No original executable or CHD was added to the repository.

## Observed inputs

- The 6th CHD is recognized as FAT32 and contains `EZ2DJ/EZ2DJ.EXE`, which is PE32/i386.
- The current `Fat32Volume` rejects the 5th CHD with `CHD MBR has no in-range FAT32 partition`. This task did not implement a FAT16 or alternate filesystem reader, so the 5th internal executable path and successful original execution remain unresolved.

## Verification

- Windows x86 Debug build: passed (`scripts/build_win32.bat`)
- `re2dj_unit_tests.exe`: `checks: 1369, failures: 0`
- `re2dj_windows_product_loader_probe.exe`: `profile-defaults=ok second-defaults=ok unsupported-target=ok resolve-iat-slot=ok`
- `re2dj.exe ez2dj6th --list-targets`: confirmed the `ez2dj6th` built-in target and FAT32 CHD path
- `re2dj.exe ez2dj5th --list-targets`: confirmed the expected FAT32-partition error from the current reader
- Full CTest: the first `re2dj_windows_vfs_runtime_probe` waited without output for about five minutes and was interrupted. This probe is not directly affected by the profile and target-profile-test changes.
- `git diff --check`: passed
