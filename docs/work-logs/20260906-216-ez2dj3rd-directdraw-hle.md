# 작업 로그: ez2dj3rd DirectDraw/Direct3D HLE 연결 및 단일 프라이머리 서피스·클리퍼 지원

## 한국어

### 작업 요약

`ez2dj3rd.chd`에서 CHD 기반 Hardlock 응답 맵(`cfg/hardlock-ez2dj3rd.map`) 적용 후 게임 본체 실행 시 `0x00432527`에서 발생하던 `0xc0000005` 널 포인터 역참조 크래시 문제를 규명하고 해결했습니다.

### 원인 분석

1. **DirectDraw HLE 비활성화**:
   - `ez2dj3rd` 프로파일의 `run_defaults.hle_d3d3`가 기본값 `false`로 설정되어 현대 64비트 Windows의 시스템 `DDRAW.dll`로 진입하고 있었습니다.
   - 시스템 `DDRAW.dll`의 `SetDisplayMode` 실패(`0x80004001`, `E_NOTIMPL`)로 인해 디스플레이 디바이스 초기화가 중단되고, 디바이스 컨텍스트 포인터가 NULL로 남아 `0x00432527`의 `Tree::find(key)` 호출 시 역참조 실패를 유발했습니다.

2. **단일 프라이머리 서피스 거부**:
   - `hle_d3d3`를 활성화한 후, 게스트가 백버퍼가 없는 단일 프라이머리 서피스(`flags=DDSD_CAPS, caps=DDSCAPS_PRIMARYSURFACE, dwBackBufferCount=0`) 생성을 요청했습니다.
   - 기존 `direct3d3_com_facade.cpp`의 `RootCreateSurface`는 `dwBackBufferCount != 1` 조건을 무조건 `DDERR_UNSUPPORTED`(`0x80004001`)로 거부하여 서피스 생성이 실패했습니다.

3. **DirectDraw Clipper 미구현**:
   - 단일 프라이머리 서피스 생성을 허용한 후, 게스트는 윈도우 모드 렌더링을 위해 `IDirectDraw7::CreateClipper`를 호출했습니다.
   - 기존 `directdraw7_com_facade.cpp`의 `Dd7CreateClipper`는 스텁으로 `*clipper = nullptr; return DDERR_UNSUPPORTED;`를 반환하고 있어, 반환된 클리퍼 객체의 메서드(예: `SetHWnd`)를 호출할 때 게스트가 크래시했습니다.

4. **Launcher Probe 정적 ExitProcess 검사 실패**:
   - `ez2dj3rd` 실행 파일은 `.protect` 스텁으로 보호되어 정적 IAT에 `KERNEL32.dll!ExitProcess` 슬롯이 없습니다. `windows_x86_launcher_probe`의 `break_exit_process` 검사에서 4th와 달리 3rd가 예외 대상에서 누락되어 진단 실행이 거부되었습니다.

### 적용된 변경 사항

1. **타깃 프로파일 및 테스트 갱신**:
   - `src/target/target_profile.cpp`: `ez2dj3rd` 프로파일에 `entry.profile.run_defaults.hle_d3d3 = true;`를 추가했습니다.
   - `tests/unit/target_profile_test.cpp`: `ez2dj3rd` 프로파일의 `hle_d3d3` 기대값을 `true`로 갱신했습니다.
   - `src/tools/windows_product_loader_probe/main.cpp`: 인자 개수 검증(18 -> 19) 및 인자 슬롯 위치를 갱신했습니다.

2. **단일 프라이머리 서피스 지원**:
   - `src/platform/windows/direct3d3_com_facade.cpp`: `RootCreateSurface`에서 `dwBackBufferCount == 0`을 허용하고, 백버퍼 할당 없이 단일 프라이머리 서피스(`DDSCAPS_PRIMARYSURFACE`)만 생성 및 반환하도록 확장했습니다.

3. **IDirectDrawClipper HLE 파사드 구현**:
   - `src/platform/windows/directdraw7_com_facade.cpp`: `IDirectDrawClipper` 인터페이스를 온전히 구현하는 `ClipperFacade` 구조체와 vtable(`QueryInterface`, `AddRef`, `Release`, `GetClipList`, `GetHWnd`, `Initialize`, `IsClipListChanged`, `SetClipList`, `SetHWnd`)을 추가했습니다.
   - `Dd7CreateClipper`에서 유효한 `ClipperFacade` 인스턴스를 생성하여 반환하도록 연결했습니다.

4. **Launcher Probe 예외 처리 확장**:
   - `src/tools/windows_x86_launcher_probe/main.cpp`: 정적 `ExitProcess` 슬롯 부재 시 허용 목록에 `target->id == "ez2dj3rd"`를 추가했습니다.

### 검증 결과

1. **단위 테스트**:
   - `re2dj_unit_tests.exe`: 100% 통과 (0.22s)
   - `re2dj_windows_product_loader_probe.exe`: 100% 통과 (0.07s)
2. **실행 검증**:
   - `.\build\windows-x86\bin\Release\re2dj.exe ez2dj3rd --io-config .\config\ez2dj-io.example.ini` 실행 결과 크래시 없이 정상 프로세스로 지속 실행(`RUNNING`)되었습니다.
   - 진단 로그(`*.ddraw.log`)에서 DirectDraw 초기화, 단일 프라이머리 서피스 생성(`result=0x00000000`), 클리퍼 생성, 그리고 3,100회 이상의 DirectDraw/Direct3D 그리기 호출(`LateDraw`, `CreateSurface`, `GetDC`, `ReleaseDC`)이 정상적으로 수행되어 게임 화면이 렌더링됨을 확인했습니다.
   - `*.vfs.log`에서 CHD FAT32 볼륨 내 파일들이 지속적으로 정상 스트리밍됨을 확인했습니다.

---

## English

### Summary

Investigated and resolved the `0xc0000005` null pointer dereference crash occurring at `0x00432527` during game execution of `ez2dj3rd` with the CHD Hardlock response map (`cfg/hardlock-ez2dj3rd.map`).

### Root Cause Analysis

1. **DirectDraw HLE Inactive**:
   - The `ez2dj3rd` profile defaulted to `run_defaults.hle_d3d3 = false`, falling back to the 64-bit Windows system `DDRAW.dll`.
   - The system `DDRAW.dll` failed `SetDisplayMode` with `0x80004001` (`E_NOTIMPL`), aborting display context creation and leaving the display context pointer NULL, which triggered a null dereference in `Tree::find(key)` at `0x00432527`.

2. **Standalone Primary Surface Rejection**:
   - After enabling `hle_d3d3`, the guest requested creation of a standalone primary surface without flipping back buffers (`flags=DDSD_CAPS, caps=DDSCAPS_PRIMARYSURFACE, dwBackBufferCount=0`).
   - `RootCreateSurface` in `direct3d3_com_facade.cpp` strictly enforced `dwBackBufferCount == 1`, returning `DDERR_UNSUPPORTED` (`0x80004001`, `E_NOTIMPL`).

3. **DirectDraw Clipper Unimplemented**:
   - Once standalone primary surface creation was accepted, the guest called `IDirectDraw7::CreateClipper` for windowed rendering.
   - `Dd7CreateClipper` in `directdraw7_com_facade.cpp` was an unimplemented stub returning `*clipper = nullptr; return DDERR_UNSUPPORTED;`, causing the guest to crash when dereferencing the null clipper pointer.

4. **Launcher Probe Static ExitProcess Check**:
   - The `ez2dj3rd` executable uses a `.protect` stub and does not expose `KERNEL32.dll!ExitProcess` in its static import table. The `break_exit_process` probe refused to attach because `ez2dj3rd` was missing from the exemption list.

### Key Changes

1. **Target Profile and Unit Tests**:
   - `src/target/target_profile.cpp`: Added `entry.profile.run_defaults.hle_d3d3 = true;` to `ez2dj3rd`.
   - `tests/unit/target_profile_test.cpp`: Updated `hle_d3d3` expectation to `true`.
   - `src/tools/windows_product_loader_probe/main.cpp`: Updated expected argument count (18 -> 19) and slot layout.

2. **Standalone Primary Surface Support**:
   - `src/platform/windows/direct3d3_com_facade.cpp`: Allowed `dwBackBufferCount == 0` in `RootCreateSurface` and allocated a single primary surface without back buffers.

3. **IDirectDrawClipper HLE Facade Implementation**:
   - `src/platform/windows/directdraw7_com_facade.cpp`: Implemented `ClipperFacade` with its vtable (`QueryInterface`, `AddRef`, `Release`, `GetClipList`, `GetHWnd`, `Initialize`, `IsClipListChanged`, `SetClipList`, `SetHWnd`).
   - Updated `Dd7CreateClipper` to return a valid `ClipperFacade` instance.

4. **Launcher Probe Exemption**:
   - `src/tools/windows_x86_launcher_probe/main.cpp`: Added `target->id == "ez2dj3rd"` to the exemption list for static `ExitProcess` import checks.

### Verification Results

1. **Unit Tests**:
   - `re2dj_unit_tests.exe`: 100% passed (0.22s)
   - `re2dj_windows_product_loader_probe.exe`: 100% passed (0.07s)
2. **Runtime Verification**:
   - Executing `.\build\windows-x86\bin\Release\re2dj.exe ez2dj3rd --io-config .\config\ez2dj-io.example.ini` ran continuously in the `RUNNING` state without crashing.
   - Diagnostic trace logs (`*.ddraw.log`) showed successful DirectDraw initialization, primary surface creation (`result=0x00000000`), clipper creation, and over 3,100 drawing operations (`LateDraw`, `CreateSurface`, `GetDC`, `ReleaseDC`).
   - Diagnostic VFS logs (`*.vfs.log`) confirmed ongoing streaming of FAT32 assets from the CHD image.
