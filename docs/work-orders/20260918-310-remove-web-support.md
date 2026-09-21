# Web 지원 범위 폐기 작업 지시

## 목적

WebAssembly/Emscripten 경로를 현재 제품 지원 범위에서 제거하고, Linux x86·x86-64와 64비트 Windows 데스크톱 경로만 활성 목표로 정렬합니다. 세부 설계는 [Web 지원 범위 폐기 설계](../design/20260918-310-remove-web-support.md)를 따릅니다.

*Purpose*

Remove the WebAssembly/Emscripten path from the active product scope and align the project around Linux x86/x86-64 and 64-bit Windows desktop paths. Follow the [Web support retirement design](../design/20260918-310-remove-web-support.md).

## 작업 범위

1. `AGENTS.md`, `docs/PROJECT_CHARTER.md`, `README.md`, `ARCHITECTURE.md`, `docs/CODING_STYLE.md`의 활성 호스트와 플랫폼 표기를 갱신합니다.
2. `CMakePresets.json`, `CMakeLists.txt`, `.github/workflows/ci.yml`에서 Web/Emscripten 전용 구성을 제거합니다.
3. `src/platform/web/`의 빈 안내 파일을 제거합니다.
4. HDD 경로 주석과 Linux 원본 실행 설계처럼 현재 동작을 설명하는 문서에서 Web 전용 가정을 제거합니다.
5. 과거 설계·작업 로그는 삭제하지 않고, 새 설계 문서에서 역사적 기록으로 보존되는 이유를 설명합니다.
6. Linux x64/x86 구성·빌드·테스트와 preset 목록을 검증하고 작업 로그를 남깁니다.

*Scope*

Update active host/platform statements in `AGENTS.md`, the project charter, README, architecture, and coding style; remove Web/Emscripten configuration from CMake presets, CMake, and CI; delete the empty Web platform guide; remove Web-only assumptions from current behavior documents; preserve historical plans as historical records; then verify Linux x64/x86 builds, tests, and the preset list.

## 검증

* `cmake --list-presets`
* `cmake --preset linux-x64-debug`, build, and `ctest --preset linux-x64-debug`
* `cmake --preset linux-x86-debug`, build, and `ctest --preset linux-x86-debug`
* `cmake --preset linux-x86-helper`, build the helper target
* `rg`로 활성 설정에 남은 Web/Emscripten 참조를 확인

*Verification*

Run `cmake --list-presets`, configure/build/test the Linux x64 and x86 presets, build the Linux i386 helper target, and use `rg` to review remaining Web/Emscripten references in active configuration.
