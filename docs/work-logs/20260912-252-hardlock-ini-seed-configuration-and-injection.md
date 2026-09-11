# hardlock.ini 시드 설정 지원 및 런타임 프로세스 주입 연동 작업 로그

## 작업 정보

* 날짜: 2026-09-12
* 작업 지시: [docs/work-orders/20260912-252-hardlock-ini-seed-configuration-and-injection.md](file:///e:/MYWORK/Projects/re2DJ/docs/work-orders/20260912-252-hardlock-ini-seed-configuration-and-injection.md)
* 상태: 완료

---

## 작업 내용

### 한국어

1. **`hardlock.ini` 시드 설정 지원**:
   - `cfg/hardlock.ini`에 `module_address`, `seed1`, `seed2`, `seed3` 설정 시 `LoadHardlockProfileMaterial`에서 "unknown Hardlock configuration key" 오류가 발생하던 문제를 해결했습니다.
   - `HardlockSecretMaterial`에 시드 16진수 문자열 필드들을 추가하고 파서에서 정상 수용하도록 구현했습니다.
   - `hardlock_material_config_test.cpp`에 시드 설정 로딩 단위 테스트를 추가하여 회귀 검증했습니다.

2. **런타임 시드 주입 및 동적 HLE 활성화**:
   - 기존에는 정적 `.map` 파일이 감지되어야만 `hardlock_device` 에뮬레이션이 활성화되었으나, 프로필에 시드(`has_seeds`)가 설정되어 있는 경우에도 `hardlock_device = true`로 기동하도록 런처를 수정했습니다.
   - `injected_runtime.cpp`에 `g_re2dj_hardlock_seeds_enabled` 및 시드 변수들을 export로 추가하고, `child_process_handoff.cpp`에서 자식 프로세스의 원격 메모리에 시드 값을 직접 주입하도록 연동했습니다.
   - `BuildHardlockDeviceOptions()`에서 주입된 시드를 바탕으로 `options.seeds`를 생성함으로써, 정적 테이블 없이 `HardlockEngine`의 실시간 연산으로 IOCTL 응답을 생성하도록 구성했습니다.

3. **검증**:
   - `re2dj_unit_tests.exe`: 1677 checks 전부 통과.
   - `ez2dj1st` 실제 실행:
     - `hardlock.ini`의 `[ez2dj1st]` 섹션 시드 설정을 읽어 launcher가 `{"event":"hardlock_seeds","module_address":5605,"enabled":true}`, `{"event":"hardlock_device","enabled":true}`를 기록.
     - VFS 및 IOCTL 로그에서 0x9c402468 (initialize), 0x9c402450 (handshake), 0x9c40244c (descriptor)가 정상 통과하며 보호 계층을 뚫고 게임 창이 성공적으로 표시됨을 확인.

### English

1. **`hardlock.ini` Seed Configuration Support**:
   - Resolved the issue where `module_address`, `seed1`, `seed2`, `seed3` under `cfg/hardlock.ini` triggered "unknown Hardlock configuration key" in `LoadHardlockProfileMaterial`.
   - Added hex string fields for seeds to `HardlockSecretMaterial` and updated the parser to accept them cleanly.
   - Added unit test cases to `hardlock_material_config_test.cpp` to verify correct seed loading.

2. **Runtime Seed Injection and Dynamic HLE Activation**:
   - Updated the launcher probe to enable `hardlock_device = true` when seeds are configured (`has_seeds`), even without static `.map` files.
   - Exported global variables (`g_re2dj_hardlock_seeds_enabled` and seed parameters) in `injected_runtime.cpp` and implemented remote memory writing in `child_process_handoff.cpp`.
   - Populated `options.seeds` in `BuildHardlockDeviceOptions()` so that `HardlockEngine` dynamically calculates IOCTL responses in real time without static tables.

3. **Verification**:
   - `re2dj_unit_tests.exe`: All 1677 checks passed.
   - Executed `ez2dj1st`:
     - Launcher successfully loaded seeds and logged `{"event":"hardlock_seeds","module_address":5605,"enabled":true}` and `{"event":"hardlock_device","enabled":true}`.
     - VFS/IOCTL logs confirmed successful completion of 0x9c402468 (initialize), 0x9c402450 (handshake), and 0x9c40244c (descriptor), successfully launching the game window.
