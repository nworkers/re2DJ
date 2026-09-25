# 작업 375 작업 로그 — 창 제목의 빌드 표시 / Task 375 work log — build label in the window title

설계: [20260926-375-window-title-build-label.md](../design/20260926-375-window-title-build-label.md)
작업 지시서: [20260926-375-window-title-build-label.md](../work-orders/20260926-375-window-title-build-label.md)

## 진행 / Progress

처음 요청은 OS·아키텍처였고, 작업 도중 빌드 형식(Debug/Release)이 더해졌다. 창 제목은 Windows injected runtime의 `Re2djUpdateWindowTitle`만 만든다. 이 DLL은 core를 link하지 않으므로 표시 문자열을 header-only로 두었다. 빌드 형식은 전역 compile definition이므로 첫 build에서 외부 library까지 모두 다시 컴파일되었다. 이때 SDL3·spdlog·libchdr의 기존 C4819(code page 949) 경고가 다시 보였고, 우리 코드의 경고는 없었다.

*The request began with the OS and architecture and gained the build type (Debug/Release) midway. Only the Windows injected runtime's `Re2djUpdateWindowTitle` builds the title, and that DLL does not link the core, so the text is header-only. The build type is a global compile definition, so the first build recompiled everything including third-party libraries, which re-showed SDL3's, spdlog's, and libchdr's existing C4819 (code page 949) warnings; none came from this project's code.*

## 변경 / Changes

- **`CMakeLists.txt`**: `add_compile_definitions(RE2DJ_BUILD_CONFIG="$<CONFIG>")`.
- **`include/re2dj/version.h`**: `RE2DJ_HOST_OS_LABEL`, `RE2DJ_HOST_ARCH_LABEL`, `RE2DJ_BUILD_CONFIG` fallback, `BuildLabel()`.
- **`window_mode.cpp`**: `re2DJ v%s (%s) - Build %s - SDL3 OpenGL - FPS : %.1f`.
- **`windows_vfs_runtime_probe`**: 제목에 `(<BuildLabel()>)`가 있는지도 본다. / *also checks the title for `(<BuildLabel()>)`.*
- **단위 테스트**(`version_test.cpp`): 표시가 test binary의 OS·포인터 폭·CMake 구성과 맞는지 본다. / ***Unit test** (`version_test.cpp`): the label matches the test binary's OS, pointer width, and CMake configuration.*
- **`ARCHITECTURE.md`**: 제목 형식. / *the title format.*

## 검증 / Validation

| 항목 / Item | 결과 / Result |
| --- | --- |
| Windows x86 build, CTest | 6/6. **정정(작업 376)**: 이때 `re2dj_windows_vfs_runtime_probe`는 `include/` 경로가 없어 build에 실패했다. 외부 library의 C4819 경고에 가려 보지 못했고, CTest는 이전 probe로 통과했다. 작업 376에서 고치고 새 probe로 제목을 확인했다 / 6/6. **Correction (Task 376):** `re2dj_windows_vfs_runtime_probe` in fact failed to build here for want of the `include/` path, hidden by third-party C4819 warnings, and CTest passed on the previous probe; Task 376 fixes it and checks the title with a rebuilt probe |
| Linux x64·x86 build, CTest | 경고·오류 없음, 각각 3/3(`Linux/x64 Debug`, `Linux/x86 Debug`) / no warnings or errors, 3/3 each (`Linux/x64 Debug`, `Linux/x86 Debug`) |
