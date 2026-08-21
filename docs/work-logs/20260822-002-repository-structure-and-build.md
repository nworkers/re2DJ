# 작업 로그: 저장소 구조와 빌드 구성

## 결과

완료.

`include/re2dj/`와 `src/` 아래에 하위 시스템별 디렉터리를 만들었다. 아직 구현이 없는 `runtime/`, `hle/`, `config/`, `platform/{windows,linux,web}/`, `third_party/`에는 목적과 향후 확장 방향을 적은 `README.md`를 두었다. `AGENTS.md`의 "초기 구조 작업이라도 디렉터리 목적을 문서로 남긴다" 규칙을 따른 것이고, Git이 빈 디렉터리를 추적하지 않는 문제도 함께 해결된다.

`CMakeLists.txt`는 `re2dj_warnings`(INTERFACE), `re2dj_core`(STATIC), 실행 파일 셋, 단위 테스트 하나를 만든다. 경고 설정을 INTERFACE 타깃 하나로 모아 컴파일러별 분기가 한곳에만 있게 했다.

`CMakePresets.json`에 Windows(Visual Studio / Ninja), Linux(Debug / Release), Web preset을 두었다.

`ARCHITECTURE.md`에 계층·디렉터리 표, HDD 입력, 게스트 경로, PE 판독, 타깃 프로파일, 계획된 런타임·HLE 계층, rePIU와의 차이표를 적었다. 각 절에 **[구현됨] / [설계됨] / [계획]** 표기를 붙여 문서와 실제 코드가 어긋나지 않게 했다.

`scripts/`에 Windows와 Linux용 build·test 진입점을 두었다. `test_all` 계열은 `RE2DJ_WARNINGS_AS_ERRORS=ON`으로 configure한다.

## 판단이 필요했던 부분

**32비트 호스트 빌드.** configure 단계에서 경고만 내고 막지는 않았다. 지원 대상은 아니지만, 나중에 Windows 전용 가속 경로를 실험할 때 32비트 빌드가 유용할 수 있다.

**CI의 Windows 잡.** preset을 쓰지 않았다. Visual Studio preset은 개발자 로컬 설치를 따라가는 생성기 버전을 고정하고, Ninja preset은 개발자 명령 프롬프트를 요구한다. 러너 이미지가 바뀌어도 동작하도록 CMake 기본 생성기를 쓰게 했고, 그 이유를 워크플로에 주석으로 남겼다.

**Web 잡에 테스트가 없다.** 브라우저에서 CTest를 돌릴 수 없다. 대신 공용 코어가 WebAssembly로 여전히 컴파일되는지만 확인한다. 공용 코어에 플랫폼 가정이 스며들면 보통 여기서 먼저 깨진다.

## 검증

`scripts/test_all.ps1`로 build 트리를 지우고 다시 configure·build·test했다. `/W4 /permissive- /WX`에서 경고 없이 빌드되고 단위 테스트가 통과했다.

CI가 쓰는 명령(`cmake -S . -B build/ci -A x64 -DRE2DJ_WARNINGS_AS_ERRORS=ON`)도 로컬에서 같은 결과를 내는지 별도로 확인했다.

Linux, Web, GitHub Actions 워크플로는 **이 환경에서 검증하지 않았다.** 해당 툴체인과 러너가 없다. 첫 push 때 확인해야 한다.

## Work Log: Repository Structure and Build

## Result

Complete.

Subsystem directories were created under `include/re2dj/` and `src/`. The ones with no implementation yet — `runtime/`, `hle/`, `config/`, the three platform directories, and `third_party/` — carry a `README.md` stating their purpose and future direction. That follows the rule requiring directory purpose to be documented even for initial structure work, and it also solves Git not tracking empty directories.

`CMakeLists.txt` produces an interface warnings target, a static core library, three executables, and one test executable, with compiler-specific warning flags collected in the single interface target. `CMakePresets.json` carries Windows (Visual Studio and Ninja), Linux (Debug and Release), and Web presets.

`ARCHITECTURE.md` documents the layer and directory tables, HDD input, guest paths, PE reading, target profiles, the planned runtime and HLE layers, and the differences from rePIU. Each section is marked **[Implemented]**, **[Designed]**, or **[Planned]** so the document cannot drift from the code unnoticed.

`scripts/` holds build and test entry points for Windows and Linux, with the `test_all` scripts configuring `RE2DJ_WARNINGS_AS_ERRORS=ON`.

## Judgement calls

A 32-bit host build warns at configure time but is not blocked, since it is not a supported target yet could be useful when experimenting with a Windows-only acceleration path later.

The CI Windows job uses no preset: the Visual Studio presets pin a generator version that tracks a developer's local install, and the Ninja preset needs a developer command prompt. CMake's default generator keeps the job working as the runner image moves, and the reason is a comment in the workflow.

The Web job runs no tests, because CTest cannot drive a browser. It only proves the shared core still compiles for WebAssembly, which is usually where a platform assumption in the core breaks first.

## Verification

`scripts/test_all.ps1` was run after deleting the build tree. The build is warning-free under `/W4 /permissive- /WX` and the unit tests pass. The command CI uses was checked separately and produced the same result locally.

The Linux and Web builds and the GitHub Actions workflow were **not verified in this environment**, which has neither those toolchains nor a runner. They need checking on the first push.
