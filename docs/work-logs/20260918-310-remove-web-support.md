# Web 지원 범위 폐기 작업 로그

## 결과

WebAssembly/Emscripten을 현재 제품 지원 범위에서 제거했습니다. 지원 목표는 Linux x86(i386), Linux x86-64, 64비트 Windows의 Win32 x86 경로로 정렬했습니다.

*Result*

WebAssembly/Emscripten was removed from the active product scope. The target is now aligned to Linux x86 (i386), Linux x86-64, and the Win32 x86 path on 64-bit Windows.

## 변경 내용

* `CMakePresets.json`에서 Web configure/build preset을 제거했습니다.
* `CMakeLists.txt`에서 `EMSCRIPTEN` compile definition과 Web 전용 OpenGL probe 조건을 제거했습니다.
* ImGui와 SDL3/OpenGL backend의 Emscripten/WebGL shader·context 분기를 제거하고 desktop OpenGL 2.1/GLSL 1.20 경로만 유지했습니다.
* `.github/workflows/ci.yml`에서 Emscripten setup/build job을 제거했습니다.
* `src/platform/web/`의 빈 안내 파일을 제거했습니다.
* `AGENTS.md`, `README.md`, `ARCHITECTURE.md`, `docs/PROJECT_CHARTER.md`, `docs/WIN32_HLE_PORTING_PLAN.md`, `docs/CODING_STYLE.md`의 활성 호스트 정책을 Windows/Linux로 갱신했습니다.
* Linux 원본 실행·Direct3D/OpenGL 설계와 32비트 게스트 KB를 데스크톱 helper 기준으로 갱신했습니다.
* 기존 Web 실행 엔진 조사는 삭제하지 않고 `docs/kb/web-x86-execution-engines.md`에 역사적 기록임을 표시했습니다. 과거 작업 로그와 작업 지시도 당시 상태를 보존하므로 삭제하지 않았습니다.

*Changes*

The Web configure/build presets, Emscripten compile definition and Web-only OpenGL condition, Emscripten CI job, and Emscripten/WebGL shader and context branches were removed. The desktop OpenGL 2.1/GLSL 1.20 path remains. The empty `src/platform/web/` guide was deleted. Active host policy in `AGENTS.md`, README, architecture, charter, porting plan, coding style, and current Linux/graphics/guest-execution designs now names Windows/Linux desktop hosts only. The Web engine survey and older work records remain as historical evidence and are explicitly marked as such where they are still indexed.

## 검증 증거

* `python -m json.tool CMakePresets.json` 통과.
* Windows 호스트의 `cmake --list-presets`에는 Windows preset만 표시되며 `web` preset은 없습니다.
* WSL Ubuntu 24.04.1에서 `cmake --list-presets`는 `linux-x64-debug`, `linux-x64-release`, `linux-x86-debug`, `linux-x86-release`, `linux-x86-helper`만 표시했습니다.
* WSL `linux-x64-debug` configure와 build 통과, `ctest --preset linux-x64-debug`: 1/1 통과.
* WSL `linux-x86-debug` configure와 build 통과, `ctest --preset linux-x86-debug`: 1/1 통과.
* WSL `linux-x86-helper` configure와 `re2dj_linux_native_ipc_helper` build 통과.
* 기존 Windows x86 Visual Studio solution을 `msbuild .../re2DJ.sln /p:Configuration=Debug`로 확인했고, 기존 Windows CTest 2/2 통과.
* `git diff --check` 통과.

*Verification evidence*

`CMakePresets.json` parses successfully. The Windows host lists no `web` preset, and WSL Ubuntu 24.04.1 lists only the five Linux presets above. Linux x86-64 and x86 configure/build/test checks passed, and the i386 helper built successfully. The existing Windows x86 Visual Studio solution was checked with MSBuild and its two CTest cases passed. `git diff --check` passed.

## 제한 사항

Web 지원 제거 변경과 무관하게, 이 환경에서 새 Windows CMake build tree를 만들 때 SDL3 FetchContent clone은 네트워크 프록시 연결 실패로 중단되었습니다. 기존 생성된 Windows solution의 build/test는 통과했으며, 다음 Windows 의존성 갱신 시 네트워크가 가능한 환경에서 새 configure를 반복해야 합니다.

*Limitations*

When creating a fresh Windows CMake build tree in this environment, the SDL3 FetchContent clone stopped because the network proxy could not connect. The existing generated Windows solution build and tests passed; a fresh configure should be repeated in an environment with network access when Windows dependencies are refreshed.
