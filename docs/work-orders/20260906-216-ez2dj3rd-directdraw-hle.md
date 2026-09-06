# 작업 지시서: ez2dj3rd DirectDraw/Direct3D HLE 연결 및 단일 프라이머리 서피스·클리퍼 지원

## 한국어

설계: [ez2dj3rd DirectDraw/Direct3D HLE 연결 설계](../design/20260906-216-ez2dj3rd-directdraw-hle.md)

### 목표

`ez2dj3rd` 타깃 프로파일의 `run_defaults.hle_d3d3` 옵션을 활성화하고, Direct3D 파사드의 `RootCreateSurface`에서 백버퍼 없는 단일 프라이머리 서피스 생성 지원 및 `directdraw7_com_facade.cpp`에서 `IDirectDrawClipper` 파사드를 구현하여 현대 64비트 Windows 환경에서 `DDRAW.dll` 실패 및 서피스·클리퍼 생성 실패로 발생하던 `0x00432527` 널 포인터 역참조 크래시를 해결합니다.

### 작업 항목

1. `src/target/target_profile.cpp`의 `ez2dj3rd` 프로파일 정의에 `entry.profile.run_defaults.hle_d3d3 = true;`를 추가합니다. (완료)
2. `tests/unit/target_profile_test.cpp`의 `ez2dj3rd` 프로파일 기본값 단언문(`!third->run_defaults.hle_d3d3` -> `third->run_defaults.hle_d3d3`)을 갱신합니다. (완료)
3. `src/tools/windows_product_loader_probe/main.cpp`의 `ez2dj3rd` 프로파일 인자 검증(기대 인자 수 18 -> 19 및 `--hle-d3d3` 포함 위치)을 갱신합니다. (완료)
4. `src/platform/windows/direct3d3_com_facade.cpp`의 `RootCreateSurface`에서 `dwBackBufferCount == 0`인 단일 프라이머리 서피스(`DDSCAPS_PRIMARYSURFACE`) 요청을 수용하고, 백버퍼 할당 없이 프라이머리 서피스만 정상 생성하도록 구현합니다. (완료)
5. `src/tools/windows_x86_launcher_probe/main.cpp`의 `break_exit_process` 검사에서 `ez2dj3rd`를 `ez2dj4th`와 동일하게 정적 `ExitProcess` 슬롯 부재 예외 대상에 포함합니다. (완료)
6. `src/platform/windows/directdraw7_com_facade.cpp`에 `IDirectDrawClipper` 인터페이스를 지원하는 `ClipperFacade`를 구현하고, `Dd7CreateClipper`에서 유효한 클리퍼 인스턴스를 반환하도록 연결합니다.
7. Debug 및 Release 구성을 빌드하고 `re2dj_unit_tests`와 `re2dj_windows_product_loader_probe`를 실행하여 통과 여부를 검증합니다.
8. `re2dj.exe ez2dj3rd`를 실행하여 DirectDraw HLE 인터셉션, 프라이머리 서피스 생성, 클리퍼 생성 성공 및 크래시 해소 여부를 확인합니다.
9. 작업 로그를 작성하고 관련 소스 및 문서를 커밋합니다.

### 제외 범위

- `config/ez2dj-io.example.ini` 사용자 수정 파일의 변경 또는 커밋
- 원본 CHD/HDD 자산의 저장소 커밋
- `ez2dj3rd`의 DirectInput 등 아직 필요성이 확인되지 않은 추가 서브시스템 선제적 활성화

### 완료 조건

- `tests/unit/target_profile_test.cpp`와 `src/tools/windows_product_loader_probe/main.cpp`를 포함한 모든 단위 테스트가 통과합니다.
- `ez2dj3rd` 실행 시 DirectDraw HLE에서 단일 프라이머리 서피스 및 클리퍼가 성공적으로 생성되고 `0x00432527` 널 포인터 크래시 지점을 통과합니다.
- 작업 로그 `docs/work-logs/20260906-216-ez2dj3rd-directdraw-hle.md`가 작성됩니다.

---

## English

Design: [ez2dj3rd DirectDraw/Direct3D HLE Integration Design](../design/20260906-216-ez2dj3rd-directdraw-hle.md)

### Objective

Enable `run_defaults.hle_d3d3` for `ez2dj3rd`, extend `RootCreateSurface` for standalone primary surfaces, and implement `ClipperFacade` for `IDirectDrawClipper` in `directdraw7_com_facade.cpp` to resolve the `0x00432527` null pointer crash on modern 64-bit Windows.

### Work items

1. Add `entry.profile.run_defaults.hle_d3d3 = true;` to `ez2dj3rd` in `src/target/target_profile.cpp`. (Done)
2. Update assertions in `tests/unit/target_profile_test.cpp` (`!third->run_defaults.hle_d3d3` -> `third->run_defaults.hle_d3d3`). (Done)
3. Update launcher argument checks in `src/tools/windows_product_loader_probe/main.cpp`. (Done)
4. Update `RootCreateSurface` in `src/platform/windows/direct3d3_com_facade.cpp` to support `dwBackBufferCount == 0` for standalone primary surfaces. (Done)
5. Update `break_exit_process` in `src/tools/windows_x86_launcher_probe/main.cpp` to exempt `ez2dj3rd` from requiring a static `ExitProcess` import. (Done)
6. Implement `ClipperFacade` for `IDirectDrawClipper` in `src/platform/windows/directdraw7_com_facade.cpp` and return valid instances from `Dd7CreateClipper`.
7. Build Debug and Release configurations and verify tests.
8. Launch `re2dj.exe ez2dj3rd` and verify surface/clipper allocation and crash resolution.
9. Create work log and commit related sources and documentation.

### Out of scope

- Modifying or committing the user-modified `config/ez2dj-io.example.ini`.
- Committing original CHD/HDD assets to the repository.
- Preemptively enabling other subsystems without confirmed runtime necessity.

### Completion criteria

- All unit tests pass.
- Launching `ez2dj3rd` allocates primary surface and clipper and passes beyond the `0x00432527` crash.
- Work log `docs/work-logs/20260906-216-ez2dj3rd-directdraw-hle.md` is created.
