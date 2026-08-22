# 프로젝트 헌장

## 목적

re2DJ는 EZ2DJ의 원본 실행 파일을 가능한 한 그대로 실행하고, 그 주변의 Win32 API·DirectX·하드웨어 환경만 HLE 계층으로 대체하는 이식 가능한 런타임을 만든다.

게임 로직은 원본 32비트 x86 코드가 담당해야 하며, C++ 코드는 실행 환경과 관찰 가능한 외부 서비스를 제공하는 데 집중한다.

## Purpose

re2DJ builds a portable runtime that executes the original EZ2DJ executable as directly as possible while replacing only the surrounding Win32 API, DirectX, and hardware environment with HLE layers.

Game logic must remain in the original 32-bit x86 code. C++ code should focus on the execution environment and observable external services.

---

## 지원 목표 플랫폼

| 호스트 | 툴체인 | 상태 |
| --- | --- | --- |
| 64-bit Windows | MSVC 또는 clang-cl | 1차 개발 호스트 |
| Linux x86-64 | GCC 또는 Clang | Windows 단계 완료 후 이식 목표 |
| Web (WebAssembly) | Emscripten | 2차 이식 목표 |

x86-64 Windows와 Linux는 별도 32비트 프로세스에서 원본 x86 코드를 CPU로 실행할 수 있으므로 네이티브 helper 경로를 검토한다. 64비트 프로세스 내부에 32비트 코드를 직접 적재할 수는 없으며, WebAssembly는 명령어 집합 자체가 달라 별도 x86 실행 계층이 반드시 필요하다.

## Target Host Platforms

| Host | Toolchain | Status |
| --- | --- | --- |
| 64-bit Windows | MSVC or clang-cl | Primary development host |
| Linux x86-64 | GCC or Clang | Porting target after Windows stages |
| Web (WebAssembly) | Emscripten | Second porting target |

Windows and Linux on x86-64 can let the CPU execute original x86 code in a separate 32-bit process, so a native-helper path is evaluated. A 64-bit process cannot directly load 32-bit code, while WebAssembly has a different instruction set and therefore always needs a separate x86 execution layer.

---

## 현재 1차 목표

첫 번째 목표는 사용자가 제공한 EZ2DJ HDD 디렉터리를 읽어 실행 대상 바이너리를 식별하고, PE32 이미지를 게스트 주소 공간에 적재하는 것이다.

향후 여러 버전을 지원해야 하므로 실행 파일 경로, 작업 디렉터리, 자산 루트, 버전별 HLE 특성은 타깃 프로파일로 분리한다.

## Current First Target

The first goal is to read a user-supplied EZ2DJ HDD directory, identify the executable to run, and map its PE32 image into the guest address space.

Because multiple versions must be supported later, executable paths, working directories, asset roots, and version-specific HLE behavior are separated into target profiles.

---

## 방향성

* 원본 실행 파일을 권위 있는 구현으로 취급한다.
* DOSBox, QEMU, VirtualBox 같은 전체 시스템 에뮬레이터를 통합하지 않는다.
* Wine 소스를 통합하지 않는다. 라이선스가 LGPL이므로 프로젝트 라이선스 정책과 충돌한다.
* 실행 파일 분석, 메모리 매핑, Win32 HLE, 그래픽, 입력, 타이밍, 오디오는 독립 하위 시스템으로 분리한다.
* PE32 로더 코어는 공용으로 유지하고, 게임/버전별 차이는 정적 target profile과 향후 HLE profile/override로 분리한다.
* HLE profile은 실제 게임 로직을 대체하지 않고, 원본 코드 주변 환경 서비스의 범위를 선언한다.
* 원본 HDD 내용은 사용자가 지정한 디렉터리 경로로 입력받고, 저장소는 그 내용을 절대 포함하지 않는다.
* 게스트의 파일 쓰기는 원본 디렉터리를 건드리지 않고 overlay 경로로 보낸다.
* 플랫폼 공용 코어를 먼저 설계하고 Windows/Linux/Web 세부 구현은 플랫폼 계층에 둔다.
* 코드 변경 전에는 설계와 작업 계획을 문서화한다.
* 프로젝트 버전은 `VERSION` 파일의 `major.minor.patch` 형식으로 관리한다.

## Direction

* Treat the original executable as the authoritative implementation.
* Do not integrate full-system emulators such as DOSBox, QEMU, or VirtualBox.
* Do not integrate Wine source. Its LGPL license conflicts with the project license policy.
* Keep executable analysis, memory mapping, Win32 HLE, graphics, input, timing, and audio as independent subsystems.
* Keep the PE32 loader core shared, and separate game/version-specific differences into static target profiles and future HLE profiles/overrides.
* HLE profiles declare the scope of surrounding environment services and do not replace original game logic.
* Take the original HDD contents as a user-supplied directory path, and never include those contents in the repository.
* Route guest file writes to an overlay path so the original directory is never modified.
* Design the shared platform-neutral core first and keep Windows/Linux/Web specifics in platform layers.
* Document design and work plans before changing code.
* Manage the project version in the `VERSION` file using `major.minor.patch`.

---

## 비목표

* 게임플레이, 채보 판정, 렌더링 파이프라인을 C++로 재구현하지 않는다.
* 원본 자산의 재배포 경로를 제공하지 않는다.
* 사본 보호 우회 자체를 목적으로 삼지 않는다. 실행에 필요한 환경 서비스만 구현한다.
* 완성된 게임 런처를 단기 목표로 삼지 않는다. 현재는 연구·개발 단계다.

## Non-Goals

* Do not reimplement gameplay, judgement, or the rendering pipeline in C++.
* Do not provide a redistribution path for original assets.
* Do not treat copy-protection circumvention as a goal in itself; implement only the environment services execution requires.
* Do not treat a finished game launcher as a short-term goal. This is research-stage work.
