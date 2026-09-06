# 작업 지시서: ez2dj3rd CHD 프로파일 전환

## 한국어

### 관련 설계

[ez2dj3rd CHD 프로파일 전환 설계](../design/20260906-213-ez2dj3rd-chd-profile.md)

### 작업 목표

사용자가 추가한 `roms/ez2dj3rd/ez2dj3rd.chd`를 `ez2dj3rd` shortcut의 원본 입력으로 연결하고, CHD 내부 `EZ2DJ/EZ2DJ.EXE`를 대표 실행 파일로 사용합니다.

### 작업 항목

1. CHD geometry와 내부 PE 경로를 probe로 확인합니다.
2. `target_profile.cpp`의 `ez2dj3rd`를 MAME CHD profile로 변경합니다.
3. 기존 3rd HLE/Hardlock 실행 정책을 보존합니다.
4. target profile unit test에 CHD 입력 정책을 고정합니다.
5. CHD shortcut 조회와 Windows x86 build/test를 검증합니다.
6. CHD 구조와 profile 변경을 analysis, architecture, work log에 기록합니다.

### 제외 범위

- Hardlock response/seed 변경
- legacy I/O helper 추가 또는 활성화
- graphics/blending 변경
- 원본 CHD·IMG·실행 파일의 저장소 추가

### 완료 조건

- `re2dj ez2dj3rd`가 `roms/ez2dj3rd/ez2dj3rd.chd`를 찾습니다.
- selected executable이 `EZ2DJ/EZ2DJ.EXE`로 표시됩니다.
- `re2dj ez2dj3rd --list-targets`와 target profile tests가 통과합니다.
- Windows x86 build와 focused CTest가 통과합니다.

## English

### Related design

[ez2dj3rd CHD Profile Conversion Design](../design/20260906-213-ez2dj3rd-chd-profile.md)

### Objective

Connect `roms/ez2dj3rd/ez2dj3rd.chd` as the original input for the `ez2dj3rd` shortcut and use `EZ2DJ/EZ2DJ.EXE` inside the CHD as the representative executable.

### Work items

1. Verify the CHD geometry and internal PE path with the probe.
2. Convert `ez2dj3rd` in `target_profile.cpp` to an MAME CHD profile.
3. Preserve the existing 3rd HLE/Hardlock execution policy.
4. Pin the CHD input policy in the target-profile unit test.
5. Verify shortcut lookup and the Windows x86 build/tests.
6. Record the CHD structure and profile change in analysis, architecture, and the work log.

### Out of scope

- Changing Hardlock responses or seeds
- Adding or enabling legacy-I/O helpers
- Changing graphics or blending
- Adding original CHD, IMG, or executable assets to the repository

### Completion criteria

- `re2dj ez2dj3rd` finds `roms/ez2dj3rd/ez2dj3rd.chd`.
- The selected executable is printed as `EZ2DJ/EZ2DJ.EXE`.
- `re2dj ez2dj3rd --list-targets` and target-profile tests pass.
- The Windows x86 build and focused CTest pass.
