# 작업 376 작업 로그 — 모든 버전 표시를 한 머리말로 / Task 376 work log — one banner for every version display

설계: [20260926-376-consistent-version-banner.md](../design/20260926-376-consistent-version-banner.md)
작업 지시서: [20260926-376-consistent-version-banner.md](../work-orders/20260926-376-consistent-version-banner.md)

## 진행 / Progress

Windows build에서 `re2dj_windows_vfs_runtime_probe`가 `re2dj/version.h`를 찾지 못해 실패했다. 이 include는 작업 375에서 넣었으므로, 작업 375의 build에서도 이 probe는 실패했다. 그때는 전역 정의 변경으로 외부 library의 C4819 경고가 출력 앞부분을 채워 오류가 보이지 않았다. CTest는 이전에 build된 probe로 통과했다. probe target에 `include/`를 주었다. 이번에는 probe 실행 파일을 지운 뒤 build해서 새로 만들어졌는지 확인했다. 외부 경고를 걸러 낸 build 오류가 없고 exit code가 0인 것도 확인했다.

*On the Windows build `re2dj_windows_vfs_runtime_probe` failed to find `re2dj/version.h`. That include came in with Task 375, so the probe failed to build then too; the global definition change had filled the head of the output with third-party C4819 warnings, hiding the error, and CTest passed on the previously built probe. The probe target now has `include/`, and this time the probe binary was deleted before the build to confirm it was rebuilt, with the build's exit code 0 and no errors once third-party warnings are filtered.*

## 변경 / Changes

- **`include/re2dj/version.h`**: `VersionBanner()`.
- **Windows**: `window_mode.cpp`(제목), `game_controls.cpp`(OSD). / *`window_mode.cpp` (title) and `game_controls.cpp` (OSD).*
- **CLI**: `--help` 첫 줄, `--version`, 시작 log. / *the first `--help` line, `--version`, and the start-up log.*
- **진단 도구 / diagnostic tools**: `re2dj_code_score`, `re2dj_hdd_probe`, `re2dj_pe_analyzer`, `re2dj_pe_loader`.
- **`windows_vfs_runtime_probe`**: 제목이 `VersionBanner("re2DJ", RE2DJ_VERSION)`로 시작하는지 본다. target에 `include/`를 추가했다. / *checks the title starts with `VersionBanner("re2DJ", RE2DJ_VERSION)`; the target gains `include/`.*
- **단위 테스트**: `VersionBanner` 형식. / ***Unit test:** the `VersionBanner` format.*

## 검증 / Validation

| 항목 / Item | 결과 / Result |
| --- | --- |
| Windows x86 build, CTest | exit 0, 우리 코드 경고·오류 없음, 6/6. probe를 새로 build했고, 실제 창 제목이 `re2DJ v0.0.52 (Win/x86 Debug)`로 시작함을 확인 / exit 0, no warnings or errors from this project, 6/6; the probe was rebuilt and finds the real window title starting with `re2DJ v0.0.52 (Win/x86 Debug)` |
| Windows `re2dj --version`, `--help` | `re2DJ v0.0.52 (Win/x86 Debug)`, `re2DJ v0.0.52 (Win/x86 Debug) - run the original EZ2DJ executable on modern hosts` |
| Linux x64·x86 build, CTest | 경고·오류 없음, 각각 3/3 / no warnings or errors, 3/3 each |
| Linux 출력 / output | `re2DJ v0.0.52 (Linux/x64 Debug)`, `re2DJ v0.0.52 (Linux/x86 Debug)`; `re2dj_pe_loader v0.0.52 (Linux/x64 Debug) - map a PE32 image without executing it`; log `re2DJ v0.0.52 (Linux/x64 Debug) starting` |
