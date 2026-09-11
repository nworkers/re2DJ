# 작업 지시서: ez2dj6th 자식 프로세스 Hardlock 동적 HLE 및 I/O HLE 활성화

## 한국어

### 작업 목표

1. `HardlockDevice::Complete`의 동적 API_CODE 계산에서 블록 3 2개 DWORD 가산(`+=`) 로직을 추가한다.
2. Launcher Probe(`main.cpp`)에서 자식 프로세스 Hardlock 활성화 플래그가 최신 상태로 전달되도록 수정한다.
3. `BootstrapChildHandoffOptions` 및 `PrepareBootstrapChildProcess`에 I/O HLE 주입 로직을 추가하여 자식 프로세스에 레거시 I/O 포트 트랩이 동작하도록 한다.
4. `ez2dj6th` 대상 프로필에서 레거시 I/O 포트를 활성화하고 관련 단위 테스트 및 프로브 테스트를 갱신한다.
5. `re2dj ez2dj6th --io-config .\config\ez2dj-io.example.ini` 실행을 통해 게임 루프 진입 및 I/O HLE 동작을 검증한다.

### 작업 범위

- `src/hle/hardlock/device.cpp`
- `src/tools/windows_x86_launcher_probe/child_process_handoff.h`
- `src/tools/windows_x86_launcher_probe/child_process_handoff.cpp`
- `src/tools/windows_x86_launcher_probe/main.cpp`
- `src/target/target_profile.cpp`
- `tests/unit/target_profile_test.cpp`
- `tests/unit/hardlock_engine_test.cpp`
- `src/tools/windows_product_loader_probe/main.cpp`
- `docs/work-logs/20260912-254-ez2dj6th-child-process-hardlock-and-io-hle.md`

---

## English

### Objectives

1. Add two-DWORD addition (`+=`) logic for Block 3 in dynamic API_CODE calculation within `HardlockDevice::Complete`.
2. Fix Launcher Probe (`main.cpp`) so that the child process Hardlock device flag receives the fully resolved value.
3. Add I/O HLE injection logic to `BootstrapChildHandoffOptions` and `PrepareBootstrapChildProcess` so legacy I/O port trapping runs in the child process.
4. Enable legacy I/O ports in the `ez2dj6th` target profile and update associated unit and probe tests.
5. Verify game loop entry and I/O HLE execution via `re2dj ez2dj6th --io-config .\config\ez2dj-io.example.ini`.

### Scope

- `src/hle/hardlock/device.cpp`
- `src/tools/windows_x86_launcher_probe/child_process_handoff.h`
- `src/tools/windows_x86_launcher_probe/child_process_handoff.cpp`
- `src/tools/windows_x86_launcher_probe/main.cpp`
- `src/target/target_profile.cpp`
- `tests/unit/target_profile_test.cpp`
- `tests/unit/hardlock_engine_test.cpp`
- `src/tools/windows_product_loader_probe/main.cpp`
- `docs/work-logs/20260912-254-ez2dj6th-child-process-hardlock-and-io-hle.md`
