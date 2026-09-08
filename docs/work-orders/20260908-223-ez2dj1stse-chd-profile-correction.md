# 작업 지시서: ez2dj1stse CHD 프로파일 실행 정책 정정

## 한국어

### 관련 설계

[ez2dj1stse CHD 프로파일 실행 정책 정정 설계](../design/20260908-223-ez2dj1stse-chd-profile-correction.md)

### 작업 목표

CHD `.protect` 빌드에서 확인된 packed import directory와 실행 관측에 맞게 `ez2dj1stse` built-in profile의 실행 기본값을 정정합니다.

### 작업 항목

1. `target_profile.cpp`의 `ez2dj1stse` 실행 기본값을 설계 표대로 정정합니다.
2. profile note를 관측된 경계(FEnteDev, packed import 제약)로 갱신합니다.
3. target profile unit test에 정정된 값을 고정합니다.
4. product loader probe의 1st SE 인자 계약을 정정된 값에 맞춥니다.
5. Windows x86 build, unit test, product loader probe를 검증합니다.
6. `re2dj ez2dj1stse --run`으로 preparation 전 항목 통과와 FEnteDev 개방을 확인합니다.
7. analysis, ARCHITECTURE, README, work log를 갱신합니다.

### 제외 범위

- Hardlock transform 응답값 확정 또는 저장소 추가
- packer-owned import용 graphics HLE 예외 신설
- legacy I/O helper RVA 변경
- launcher·injected runtime 코드 변경
- `hle_wts_active_console` 활성화 (근거 없음)

### 완료 조건

- `re2dj ez2dj1stse --run`이 preparation 실패 없이 게스트를 실행합니다.
- 게스트가 `\\.\FEnteDev`를 열고 Hardlock initialize에 도달합니다.
- target profile unit test와 product loader probe가 통과합니다.
- Windows x86 build가 통과합니다.

## English

### Related design

[ez2dj1stse CHD Profile Execution Policy Correction Design](../design/20260908-223-ez2dj1stse-chd-profile-correction.md)

### Objective

Correct the `ez2dj1stse` built-in profile's execution defaults to match the packed import directory and runtime observations confirmed for the CHD `.protect` build.

### Work items

1. Correct the `ez2dj1stse` execution defaults in `target_profile.cpp` per the design table.
2. Update the profile note to describe the observed boundary — FEnteDev and the packed-import constraints.
3. Pin the corrected values in the target-profile unit test.
4. Align the product-loader probe's 1st SE argument contract with the corrected values.
5. Verify the Windows x86 build, unit tests, and product-loader probe.
6. Confirm with `re2dj ez2dj1stse --run` that every preparation step passes and the guest opens FEnteDev.
7. Update the analysis document, `ARCHITECTURE.md`, `README.md`, and the work log.

### Out of scope

- Establishing or committing the Hardlock transform response
- Adding a graphics HLE exception for packer-owned imports
- Changing the legacy-I/O helper RVAs
- Changing launcher or injected-runtime code
- Enabling `hle_wts_active_console`, which has no supporting evidence

### Completion criteria

- `re2dj ez2dj1stse --run` executes the guest with no preparation failure.
- The guest opens `\\.\FEnteDev` and reaches the Hardlock initialize request.
- The target-profile unit test and the product-loader probe pass.
- The Windows x86 build passes.
