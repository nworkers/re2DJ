# 저장소 구조와 빌드 구성 설계

## 배경

re2DJ는 Linux, 64비트 Windows, Web 세 호스트를 동시에 지원해야 한다. 구조를 잘못 잡으면 플랫폼 하나에서만 성립하는 가정이 공용 코어에 스며들고, 나중에 그것을 걷어내는 비용이 커진다.

## 계층 설계

하위 시스템을 다음 축으로 나눈다.

| 계층 | 경로 | 책임 |
| --- | --- | --- |
| 자산 입력 | `hdd/` | 사용자가 준 디렉터리 검증, 경로 해석, 실행 파일 스캔 |
| 게스트 경로 | `storage/` | Win32 경로 파싱·정규화, overlay 정책 |
| 실행 파일 분석 | `exe/` | PE32 헤더·섹션·디렉터리 판독 |
| 타깃 프로파일 | `target/` | 버전별 실행 파일 경로와 HLE 프로파일 |
| 런타임 | `runtime/` | 게스트 주소 공간, 컨텍스트, 실행 backend |
| HLE | `hle/` | Win32·DirectX 모듈 테이블과 구현 |
| 설정 | `config/` | INI 파싱, 키 바인딩 |
| 플랫폼 | `platform/{windows,linux,web}/` | 창·렌더·오디오·입력·시간 호스트 구현 |
| 호스트 | `host/` | 실행 진입점 |
| 도구 | `tools/` | 비실행 분석 도구 |

공개 헤더는 `include/re2dj/<subsystem>/`, 구현은 `src/<subsystem>/`에 둔다. 한 하위 시스템 안에서만 쓰는 헤더는 `src/` 쪽에 둔다.

## 이식성 경계

공용 코어는 호스트 OS 헤더를 포함하지 않는다. `<windows.h>`, `<unistd.h>`, `<emscripten.h>`는 `src/platform/` 아래에서만 등장한다. 파일 시스템 접근은 `std::filesystem`을 쓴다.

게스트 주소는 호스트 포인터로 노출하지 않는다. 64비트 호스트에서는 폭이 맞지 않고, WebAssembly의 선형 메모리 모델과도 어긋난다. 근거는 [64비트·Web 호스트에서 32비트 게스트 실행](../kb/x86-32-guest-on-64-bit-host.md)에 정리했다.

## 빌드 구성

CMake 3.20 이상, C++20. 타깃은 다음과 같다.

| 타깃 | 종류 | 내용 |
| --- | --- | --- |
| `re2dj_warnings` | INTERFACE | 컴파일러별 경고 설정을 한곳에 모은다 |
| `re2dj_core` | STATIC | 플랫폼 공용 코어 |
| `re2dj` | 실행 파일 | 명령행 호스트 |
| `re2dj_hdd_probe` | 실행 파일 | HDD 디렉터리 스캔 도구 |
| `re2dj_pe_analyzer` | 실행 파일 | PE32 헤더 분석 도구 |
| `re2dj_unit_tests` | 실행 파일 | CTest에 등록된 단위 테스트 |

버전은 `VERSION` 파일에서 읽어 형식을 검사하고 `RE2DJ_VERSION` 컴파일 정의로 넘긴다. 그 정의는 `re2dj_core`에만 PRIVATE으로 걸리므로, 소비자는 `re2dj::VersionString()`을 통해 값을 얻는다.

`CMakePresets.json`에 호스트별 preset을 둔다. Web preset은 `EMSDK` 환경 변수에서 툴체인 파일을 찾고, 브라우저에서 CTest를 돌릴 수 없으므로 테스트를 끈다.

## 테스트 전략

외부 테스트 프레임워크를 도입하지 않는다. 라이선스 확인과 빌드 시간 증가를 수반하는 결정이므로 별도 설계로 다룰 일이다. 대신 `tests/unit/test_support.h`에 최소 검사 매크로만 둔다.

테스트 픽스처는 실행 중에 만든다. 합성 PE32 이미지를 코드로 생성하고, HDD 트리는 임시 디렉터리에 만들었다가 지운다. 저장소에 바이너리를 커밋하지 않는다는 규칙과도 맞고, 원본 자산 없이 테스트가 통과한다는 요구와도 맞는다.

## Background

re2DJ must support Linux, 64-bit Windows, and the Web at once. A wrong structure lets single-platform assumptions seep into the shared core, and removing them later is expensive.

## Layer design

Subsystems split along the axes listed in the table above, with public headers in `include/re2dj/<subsystem>/` and implementations in `src/<subsystem>/`. A header used only inside one subsystem stays next to its sources.

## Portability boundary

The shared core includes no host OS header; `<windows.h>`, `<unistd.h>`, and `<emscripten.h>` appear only under `src/platform/`. File-system access goes through `std::filesystem`. Guest addresses are never exposed as host pointers, because the widths do not match on a 64-bit host and the model does not fit WebAssembly linear memory. The reasoning is in [Running a 32-bit Guest on 64-bit and Web Hosts](../kb/x86-32-guest-on-64-bit-host.md).

## Build configuration

CMake 3.20 or newer with C++20, producing the interface, static library, executable, and test targets listed above. The version is read from `VERSION`, format-checked, and passed as the `RE2DJ_VERSION` compile definition. That definition is PRIVATE to `re2dj_core`, so consumers read the value through `re2dj::VersionString()`. `CMakePresets.json` carries a preset per host; the Web preset locates its toolchain file through `EMSDK` and disables tests, since CTest cannot drive a browser.

## Test strategy

No third-party test framework is introduced: that decision carries a license review and a build-time cost and belongs in its own design note. `tests/unit/test_support.h` holds minimal check macros instead. Fixtures are built at run time — a synthetic PE32 image is generated in code, and the HDD tree is created in a temporary directory and removed afterwards. That satisfies both the no-binaries-in-the-repository rule and the requirement that tests pass without original assets.
