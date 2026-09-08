# 작업 지시서: ez2dj1stse CHD 프로파일 전환

## 한국어

### 관련 설계

[ez2dj1stse CHD 프로파일 전환 설계](../design/20260908-222-ez2dj1stse-chd-profile.md)

### 작업 목표

사용자가 추가한 `roms/ez2dj1stse/ez2dj1stse.chd`를 `ez2dj1stse` shortcut의 원본 입력으로 연결하고, CHD 내부 `ez2dj/Ez2DJ.exe`를 대표 실행 파일로 사용합니다.

### 작업 항목

1. CHD geometry, FAT32 구조, 내부 PE 경로, 게스트 부팅 경로를 probe로 확인합니다.
2. CHD 실행 파일과 기존 추출 실행 파일의 보호 계열 차이를 analysis 문서에 기록합니다.
3. `target_profile.cpp`의 `ez2dj1stse`를 MAME CHD profile로 변경하고 `guest_drive_letter`를 `C`로 정정합니다.
4. 기존 1st SE HLE 실행 정책을 호환성 기준선으로 보존합니다.
5. target profile unit test에 CHD 입력 정책과 추출 디렉터리 fallback을 고정합니다.
6. CHD shortcut 조회와 Windows x86 build/test를 검증합니다.
7. profile 변경과 CHD 구조를 analysis, ARCHITECTURE, README, work log에 기록합니다.

### 제외 범위

- Hardlock response/seed 변경
- legacy I/O helper RVA 변경 또는 추가
- graphics/blending 변경
- 추출 디렉터리 전용 profile 추가
- 원본 CHD·실행 파일의 저장소 추가

### 완료 조건

- `re2dj ez2dj1stse`가 `roms/ez2dj1stse/ez2dj1stse.chd`를 찾습니다.
- selected executable이 `ez2dj/Ez2DJ.exe`로 표시됩니다.
- `re2dj ez2dj1stse --list-targets`와 target profile unit test가 통과합니다.
- Windows x86 build와 focused CTest가 통과합니다.

## English

### Related design

[ez2dj1stse CHD Profile Conversion Design](../design/20260908-222-ez2dj1stse-chd-profile.md)

### Objective

Connect `roms/ez2dj1stse/ez2dj1stse.chd` as the original input for the `ez2dj1stse` shortcut and use `ez2dj/Ez2DJ.exe` inside the CHD as the representative executable.

### Work items

1. Verify the CHD geometry, FAT32 structure, internal PE path, and guest boot path with the probe.
2. Record the protection-family difference between the CHD executable and the existing extracted executable in the analysis document.
3. Convert `ez2dj1stse` in `target_profile.cpp` to an MAME CHD profile and correct `guest_drive_letter` to `C`.
4. Preserve the existing 1st SE HLE execution policy as a compatibility baseline.
5. Pin the CHD input policy and the extracted-directory fallback in the target-profile unit test.
6. Verify shortcut lookup and the Windows x86 build/tests.
7. Record the profile change and CHD structure in the analysis document, `ARCHITECTURE.md`, `README.md`, and the work log.

### Out of scope

- Changing Hardlock responses or seeds
- Changing or adding legacy-I/O helper RVAs
- Changing graphics or blending
- Adding a directory-only profile for the extracted dump
- Adding the original CHD or executables to the repository

### Completion criteria

- `re2dj ez2dj1stse` finds `roms/ez2dj1stse/ez2dj1stse.chd`.
- The selected executable is printed as `ez2dj/Ez2DJ.exe`.
- `re2dj ez2dj1stse --list-targets` and the target-profile unit tests pass.
- The Windows x86 build and focused CTest pass.
