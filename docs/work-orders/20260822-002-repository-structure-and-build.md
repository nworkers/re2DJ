# 작업 지시: 저장소 구조와 빌드 구성

## 목표

플랫폼 공용 코어와 플랫폼별 구현을 분리한 디렉터리 골격을 세우고, 세 호스트에서 빌드 가능한 CMake 구성을 만든다.

## 범위

* `include/re2dj/{exe,hdd,storage,target,runtime,hle,platform,config}/` 생성
* `src/{exe,hdd,storage,target,runtime,hle,config}/` 생성
* `src/platform/{windows,linux,web}/` 생성
* `src/host/cli/`, `src/tools/{hdd_probe,pe_analyzer}/`, `tests/unit/`, `third_party/` 생성
* 아직 비어 있는 디렉터리마다 목적과 향후 확장 방향을 적은 `README.md` 추가
* `CMakeLists.txt`와 `CMakePresets.json` 작성
* `.gitignore`, `.gitattributes`, `.editorconfig` 작성
* `ARCHITECTURE.md`와 `README.md` 작성
* `scripts/` 빌드·테스트 진입점 작성
* CI 워크플로 작성

## 검증

* 64비트 Windows에서 configure, build, ctest가 모두 성공한다.
* 경고 없이 빌드된다.
* `RE2DJ_WARNINGS_AS_ERRORS=ON`으로도 빌드된다.
* `.gitignore`가 원본 자산 확장자와 빌드 트리를 제외한다.

## Work Order: Repository Structure and Build

## Goal

Establish a directory skeleton that separates the platform-neutral core from platform-specific implementations, and a CMake configuration that builds on all three hosts.

## Scope

Create the include, source, platform, host, tools, tests, and third-party directories; add a `README.md` stating the purpose and future direction of every directory that is still empty; write `CMakeLists.txt`, `CMakePresets.json`, `.gitignore`, `.gitattributes`, `.editorconfig`, `ARCHITECTURE.md`, `README.md`, the `scripts/` entry points, and the CI workflow.

## Verification

Configure, build, and `ctest` all succeed on 64-bit Windows; the build is warning-free; it also builds with `RE2DJ_WARNINGS_AS_ERRORS=ON`; and `.gitignore` excludes original-asset extensions and build trees.
