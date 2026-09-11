# ez2dj6th VFS GetFullPathNameA HLE 및 그래픽 에셋 로드 정상화 작업 로그
# Work Log: ez2dj6th VFS GetFullPathNameA HLE and Graphic Asset Loading Normalization

## 1. 개요 (Overview)

- **작업 일자**: 2026-09-12
- **작업 내용**: `ez2dj6th` 실행 시 비트맵 대신 파일명 텍스트(`6logo`, `ttr base`, `type`, `press_s`)가 화면에 파란색 플레이스홀더로 렌더링되던 문제 해결.
- **원인 규명**:
  - `EZ2DJ6th.EXE`의 애니메이션 클립 로더(`LoadAniClip`, RVA `0x00011e40`)는 스크립트(`.str`)를 로드할 때 `GetFullPathNameA`를 호출하여 스크립트가 존재하는 전체 디렉터리 경로를 얻은 뒤, 스크립트 내부에 정의된 비트맵 파일명(`.abm`)을 이 디렉터리 경로와 결합하여 `CreateFileA`를 호출함.
  - VFS HLE thunk 목록에 `GetFullPathNameA`가 누락되어 호스트 OS의 `KERNEL32!GetFullPathNameA`가 호출되었고, 호스트 OS 작업 디렉터리는 `C:\...\Temp\re2dj\chd\ez2dj6th\EZ2DJ`로 고정되어 있어 가상 현재 디렉터리(`System\Title` 등)가 무시된 채 `C:\...\EZ2DJ\Title.str`이 반환됨.
  - 이로 인해 비트맵 경로가 `C:\...\EZ2DJ\tt_base.abm` 등으로 잘못 생성되어 CHD 내 실제 경로(`EZ2DJ/SYSTEM/Title/TT_BASE.ABM`)를 찾지 못하고 `ERROR_FILE_NOT_FOUND`로 실패함.
  - `LoadAniClip`은 실패 시 `LoadAniClip: Not Found '%s'` 메시지와 함께 화면에 파일명 텍스트를 대체 렌더링하는 폴백을 수행하였음.
- **해결 내역**:
  - `src/platform/windows/injected_runtime.cpp`에 `Re2djVfsGetFullPathNameA` 구현 및 dllexport 추가 (`g_guest_directory_components` 가상 디렉터리 반영).
  - `Re2djHleGetProcAddress`의 동적 리졸버에 `GetFullPathNameA` 매핑 추가.
  - `child_process_handoff.cpp` 및 `main.cpp`의 IAT thunk 테이블(`vfs_exports`, `vfs_imports`)에 `GetFullPathNameA` 추가.
  - `src/host/cli/main.cpp`의 `PrepareChdStaging`에서 `ez2dj6th` 프로필 실행 시 자식 프로세스 실행 파일(`EZ2DJ/EZ2DJ6th.EXE`) 자동 머티리얼라이즈 추가.
  - `src/tools/windows_vfs_runtime_probe/main.cpp`에 `GetFullPathNameA` 단위 테스트 추가.

---

- **Date**: 2026-09-12
- **Summary**: Resolved issue where filename strings (`6logo`, `ttr base`, `type`, `press_s`) were rendered on screen as placeholder text in `ez2dj6th` instead of actual bitmap graphics.
- **Root Cause**:
  - `EZ2DJ6th.EXE`'s animation clip loader (`LoadAniClip`, RVA `0x00011e40`) called `GetFullPathNameA` to retrieve the directory path of the script (`.str`), and then combined it with bitmap names (`.abm`) to call `CreateFileA`.
  - Because `GetFullPathNameA` was not hooked by VFS, native `KERNEL32!GetFullPathNameA` was called, resolving paths against the host process working directory (`C:\...\EZ2DJ`) rather than the virtual guest current directory (`System\Title`).
  - This produced incorrect paths like `C:\...\EZ2DJ\tt_base.abm`, which failed to open (`ERROR_FILE_NOT_FOUND`) as the file actually resides at `EZ2DJ/SYSTEM/Title/TT_BASE.ABM` in the CHD.
  - `LoadAniClip` fell back to rendering the asset filename string on screen upon failure.
- **Solution**:
  - Implemented `Re2djVfsGetFullPathNameA` in `injected_runtime.cpp` reflecting `g_guest_directory_components`.
  - Added `GetFullPathNameA` to dynamic resolver and IAT thunk tables.
  - Ensured `EZ2DJ/EZ2DJ6th.EXE` is staged in `PrepareChdStaging`.
  - Added unit test in `windows_vfs_runtime_probe`.

---

## 2. 검증 결과 (Verification Results)

1. **단위 테스트**:
   - `re2dj_windows_vfs_runtime_probe.exe --vfs-enumeration-only`: 통과 (100%).
   - `re2dj_unit_tests.exe`: 1,679개 검사 전원 통과 (checks: 1679, failures: 0).
   - `re2dj_windows_product_loader_probe.exe`: 전원 통과 (`profile-defaults=ok second-defaults=ok unsupported-target=ok resolve-iat-slot=ok`).
2. **실기 실행 검증 (`re2dj.exe ez2dj6th --io-config .\config\ez2dj-io.example.ini`)**:
   - `re2dj:vfs:get-full-path:request=LOGO.str:resolved=C:\...\EZ2DJ\System\AmuseLogo\LOGO.str:success=1`
   - `re2dj:vfs:get-full-path:request=Title.str:resolved=C:\...\EZ2DJ\System\Title\Title.str:success=1`
   - `re2dj:vfs:create-file:stage=chd:request=C:\...\EZ2DJ\System\AmuseLogo\amuse_back.abm:mapped=chd://:success=1:error=0`
   - `re2dj:vfs:create-file:stage=chd:request=C:\...\EZ2DJ\System\Title\tt_base.abm:mapped=chd://:success=1:error=0`
   - 로그 전체에서 `success=0` 0건 기록 (모든 에셋 로드 성공).
