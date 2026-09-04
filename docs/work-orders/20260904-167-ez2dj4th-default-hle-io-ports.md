# 20260904-167 EZ2DJ 4th hle-io-ports 기본 활성화 구현 계획서
# 20260904-167 EZ2DJ 4th Enable hle-io-ports by Default Implementation Work Order

## 1. 목적 (Goal)

`ez2dj4th` 타겟 프로파일의 기본 실행 옵션에 `--hle-io-ports`(`legacy_io_ports_default = true`)를 추가하여, `re2dj.exe ez2dj4th` 실행 시 `0xc0000096` 특권 명령 예외 없이 I/O 포트 트랩이 활성화되도록 한다.

Add `--hle-io-ports` (`legacy_io_ports_default = true`) to the `ez2dj4th` target profile's default run options so that running `re2dj.exe ez2dj4th` activates the I/O port trap without triggering the `0xc0000096` privileged instruction exception.

---

## 2. 작업 단계 (Tasks)

1. `src/target/target_profile.cpp`의 `ez2dj4th` 프로파일에 `entry.profile.run_defaults.lptdi.legacy_io_ports_default = true;` 추가.
2. `src/tools/windows_product_loader_probe/main.cpp`의 `chd_handoff` 테스트를 갱신하여 17개 인자 및 `legacy_io_ports_default` 검증.
3. 빌드 및 테스트:
   - `scripts/build_win32.bat`
   - `re2dj_unit_tests.exe`
   - `re2dj_windows_product_loader_probe.exe`
4. `re2dj.exe ez2dj4th` 실행 및 동작 확인.
