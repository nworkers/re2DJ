# 작업 로그: ez2dj6th 자식 프로세스 Hardlock 동적 HLE 및 I/O HLE 활성화

## 한국어

### 관련 문서

- 설계: [ez2dj6th 자식 프로세스 Hardlock 동적 HLE 및 I/O HLE 활성화](../design/20260912-254-ez2dj6th-child-process-hardlock-and-io-hle.md)
- 작업 지시: [ez2dj6th 자식 프로세스 Hardlock 동적 HLE 및 I/O HLE 활성화](../work-orders/20260912-254-ez2dj6th-child-process-hardlock-and-io-hle.md)
- 선행: [ez2dj6th Hardlock 시드 검증 및 런타임 연동](20260912-253-ez2dj6th-seed-verification.md)

### 작업 요약

1. **동적 API_CODE 계산 블록 3 2개 DWORD 가산(`+=`) 구현**:
   - `src/hle/hardlock/device.cpp`의 `Complete` 메서드 내 동적 `CodePayload` 분기에서 `code_resp`의 블록 3(offsets 24..31) 두 32비트 little-endian DWORD를 게스트 `payload`의 블록 3에 가산(`+=`)하도록 구현했습니다.
   - `tests/unit/hardlock_engine_test.cpp`의 `TestDynamicDevicePayload`에 블록 3 가산 검증을 추가했습니다.

2. **Launcher Probe 자식 프로세스 Hardlock 플래그 조기 평가 버그 수정**:
   - `src/tools/windows_x86_launcher_probe/main.cpp`에서 `child_hardlock_device`가 `hardlock.ini` 파싱 전에 조기 평가되어 자식 프로세스에 `g_re2dj_hardlock_device_enabled`가 전달되지 않던 버그를 수정했습니다.
   - 자식 옵션 구성 지점에서 `child_follow_options.hardlock_device = follow_child_process && hardlock_device;`로 최신 상태를 반영하도록 변경했습니다.

3. **자식 프로세스 핸드오프(Child Process Handoff) I/O HLE 주입 구현**:
   - `child_process_handoff.h`의 `BootstrapChildHandoffOptions`에 `hle_io_ports`, `io_config_path`, `io_in_byte_rva`, `io_out_byte_rva`, `io_word_width` 필드를 추가했습니다.
   - `child_process_handoff.cpp`의 `PrepareBootstrapChildProcess`에서 `options.hle_io_ports` 활성화 시 자식 프로세스 메모리에 `g_re2dj_hle_io_ports = 1`, `g_re2dj_io_image_base = result->image_base`, `g_re2dj_io_config_path`, `g_re2dj_io_in_byte_rva`, `g_re2dj_io_out_byte_rva`, `g_re2dj_io_word_width`를 원격 주입하도록 구현했습니다.
   - `main.cpp`에서 부모가 `follow_child_process`로 인해 `hle_io_ports = false`로 리셋하기 전 `child_hle_io_ports`를 저장하고, `follow_child_process` 환경에서 `--io-config`가 허용되도록 커맨드라인 검증 조건을 조정했습니다 (`!io_config_path.empty() && !run_detached && !follow_child_process`).

4. **`ez2dj6th` 프로필 레거시 I/O 활성화 및 테스트 갱신**:
   - `src/target/target_profile.cpp`에서 `ez2dj6th` 프로필의 `legacy_io_ports = true`, `legacy_io_ports_default = true`, `legacy_io_port_range_fallback = true`를 활성화했습니다.
   - `tests/unit/target_profile_test.cpp`의 `ez2dj6th` 기대값을 `legacy_io = true`로 갱신했습니다.
   - `src/tools/windows_product_loader_probe/main.cpp`의 6th 검사 항목을 `sixth_io_config_supported`로 갱신했습니다.

### 검증 결과

| 빌드/테스트 | 대상 | 결과 |
| --- | --- | --- |
| Windows x86 Debug | `re2dj_unit_tests.exe` | 1679개 검사 모두 통과 (신규 블록 3 가산 검증 통과) |
| Windows x86 Debug | `re2dj_windows_product_loader_probe.exe` | 통과 (`profile-defaults=ok second-defaults=ok unsupported-target=ok resolve-iat-slot=ok`) |
| Windows x86 Debug | `re2dj_ez2dj_keyboard_input_test.exe` | 7개 검사 모두 통과 |
| Windows x86 Debug | CTest (3개 타깃) | 3/3 통과 (100%) |
| 실기 실행 검증 | `re2dj ez2dj6th --io-config .\config\ez2dj-io.example.ini` | 통과: Hardlock 0x458 transform 성공 (`payload=1`), DirectDraw 창 생성 및 표시, DirectSound 오디오 스트리밍 개시, 포트 0x100..0x106 입출력 정상 트랩 (`handled=1`), 게임 루프 지속 실행 확인 |

---

## English

### Related Documents

- Design: [ez2dj6th Child Process Hardlock Dynamic HLE and I/O HLE Activation](../design/20260912-254-ez2dj6th-child-process-hardlock-and-io-hle.md)
- Work Order: [ez2dj6th Child Process Hardlock Dynamic HLE and I/O HLE Activation](../work-orders/20260912-254-ez2dj6th-child-process-hardlock-and-io-hle.md)
- Preceding: [ez2dj6th Hardlock Seed Verification and Runtime Integration](20260912-253-ez2dj6th-seed-verification.md)

### Summary of Work

1. **Implemented Block 3 Two-DWORD Addition (`+=`) for Dynamic API_CODE**:
   - In `src/hle/hardlock/device.cpp`, added 32-bit little-endian addition (`+=`) of the two DWORDs in `code_resp` (offsets 24..31) into guest `payload` (offsets 24..31) for dynamic `CodePayload` execution.
   - Added block 3 addition verification to `TestDynamicDevicePayload` in `tests/unit/hardlock_engine_test.cpp`.

2. **Fixed Premature Evaluation Bug for Child Hardlock Device Flag in Launcher Probe**:
   - In `src/tools/windows_x86_launcher_probe/main.cpp`, fixed premature evaluation of `child_hardlock_device` before `hardlock.ini` was parsed, which prevented `g_re2dj_hardlock_device_enabled` from being written into the child process.
   - Updated `child_follow_options.hardlock_device = follow_child_process && hardlock_device;` at child option configuration time.

3. **Implemented Child Process Handoff I/O HLE Injection**:
   - Added `hle_io_ports`, `io_config_path`, `io_in_byte_rva`, `io_out_byte_rva`, and `io_word_width` to `BootstrapChildHandoffOptions` in `child_process_handoff.h`.
   - In `child_process_handoff.cpp`, injected `g_re2dj_hle_io_ports = 1`, `g_re2dj_io_image_base = result->image_base`, `g_re2dj_io_config_path`, `g_re2dj_io_in_byte_rva`, `g_re2dj_io_out_byte_rva`, and `g_re2dj_io_word_width` into child process memory.
   - In `main.cpp`, saved `child_hle_io_ports` before `hle_io_ports` was reset for bootstrap, and adjusted command line validation to allow `--io-config` under `follow_child_process` (`!io_config_path.empty() && !run_detached && !follow_child_process`).

4. **Enabled Legacy I/O for `ez2dj6th` Profile and Updated Tests**:
   - In `src/target/target_profile.cpp`, set `legacy_io_ports = true`, `legacy_io_ports_default = true`, and `legacy_io_port_range_fallback = true` for `ez2dj6th`.
   - Updated `target_profile_test.cpp` to expect `legacy_io = true` for `ez2dj6th`.
   - Updated `src/tools/windows_product_loader_probe/main.cpp` check to `sixth_io_config_supported`.

### Verification Results

| Build / Test | Target | Result |
| --- | --- | --- |
| Windows x86 Debug | `re2dj_unit_tests.exe` | 1679 checks passed (new block 3 addition test passed) |
| Windows x86 Debug | `re2dj_windows_product_loader_probe.exe` | Passed (`profile-defaults=ok second-defaults=ok unsupported-target=ok resolve-iat-slot=ok`) |
| Windows x86 Debug | `re2dj_ez2dj_keyboard_input_test.exe` | 7 checks passed |
| Windows x86 Debug | CTest (3 targets) | 3/3 passed (100%) |
| Runtime Verification | `re2dj ez2dj6th --io-config .\config\ez2dj-io.example.ini` | Passed: Hardlock 0x458 transform completed (`payload=1`), DirectDraw window created and shown, DirectSound audio streaming started, ports 0x100..0x106 trapped and handled (`handled=1`), game loop running continuously |
