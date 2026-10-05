# 작업 460 설계 — clang 20 이상에서 빌드되도록 spdlog 1.15.3으로 올리기 / Task 460 design — moving to spdlog 1.15.3 so clang 20 and later build

선행: [작업 345 설계(spdlog 런타임 로깅)](20260921-345-spdlog-runtime-logging.md), [작업 458 로그(Linux 실기 검증)](../work-logs/20261005-458-linux-desktop-validation.md)

## 문제 / Problem

Ubuntu 26.04의 clang 21.1.8로 CI와 같은 구성(`linux-x64-debug`, 경고를 오류로)을 빌드하면 spdlog v1.14.1의 번들 fmt 10.2.1에서 실패한다. `bundled_fmtlib_format.cpp`와 `async.cpp`에서 `call to consteval function 'fmt::basic_format_string<…>' is not a constant expression`이 나오고, 근거는 `core.h:704`의 `subexpression not valid in a constant expression`이다. re2DJ 코드가 아니라 의존성의 문제이며, CI의 clang 18과 GCC 12~15에서는 나지 않는다.

이 빌드 디렉터리에서만 spdlog를 v1.15.3(번들 fmt 11.2.0, `FMT_VERSION 110200`)으로 바꿔 시험했다(`FETCHCONTENT_SOURCE_DIR_SPDLOG`). 의존성 오류는 사라졌고, re2DJ 코드에서는 `src/logging/logging.cpp`의 `fmt::localtime`이 fmt 11.2에서 deprecated가 되어 `-Werror,-Wdeprecated-declarations` 오류 하나만 남았다. 이 경고를 오류에서 빼면 나머지는 경고 없이 빌드되고 CTest 5개가 통과했다.

*Building the CI configuration (`linux-x64-debug`, warnings as errors) with Ubuntu 26.04's clang 21.1.8 fails in spdlog v1.14.1's bundled fmt 10.2.1: `bundled_fmtlib_format.cpp` and `async.cpp` report `call to consteval function 'fmt::basic_format_string<…>' is not a constant expression`, from `subexpression not valid in a constant expression` at `core.h:704`. The problem is in the dependency, not re2DJ's code, and does not occur with CI's clang 18 or GCC 12 to 15. A trial in that build directory alone with spdlog v1.15.3 (bundled fmt 11.2.0, `FMT_VERSION 110200`, through `FETCHCONTENT_SOURCE_DIR_SPDLOG`) removed the dependency errors and left one error in re2DJ's code: `fmt::localtime` in `src/logging/logging.cpp`, deprecated in fmt 11.2, under `-Werror,-Wdeprecated-declarations`. With that warning not an error, everything else built without warnings and passed 5 CTest tests.*

## 결정 / Decisions

1. spdlog를 **v1.15.3**으로 올린다(`find_package(spdlog 1.15.3 EXACT …)`와 FetchContent `GIT_TAG`). v1.16 이후는 fmt 12를 번들하므로 변화가 더 크다. clang 20 이상 문제를 고치는 가장 작은 단계로 1.15.3을 고른다. 라이선스는 MIT 그대로다.
2. `MakeDefaultLogPath`는 `fmt::localtime(time)` 대신 `std::localtime(&time)`이 돌려준 `std::tm`을 복사해 fmt의 chrono 형식(`{:%Y%m%d-%H%M%S}`)에 넘긴다. `src/host/cli/main.cpp`의 이미지 덤프 이름도 이미 `std::localtime`을 쓴다. 이 함수는 시작할 때 한 번만 불리므로 `std::localtime`의 정적 버퍼를 공유해도 문제가 없다. MSVC는 `_CRT_SECURE_NO_WARNINGS`가 정의되어 있어 경고가 나지 않는다. 플랫폼 중립 코어이므로 `localtime_r`·`localtime_s`는 쓰지 않는다.
3. 고정 버전을 적은 곳을 함께 고친다: `THIRD_PARTY_NOTICES.md`, `docs/sites/site.toml`, kb `spdlog-runtime-logging.md`의 license 링크. 지난 작업의 설계·지시서·로그는 기록이므로 두고, CREDITS는 버전을 적지 않아 그대로다.

*1. Move spdlog to **v1.15.3** (`find_package(spdlog 1.15.3 EXACT …)` and the FetchContent `GIT_TAG`); v1.16 and later bundle fmt 12, a larger change, so 1.15.3 is the smallest step that fixes clang 20 and later, still under the MIT license. 2. `MakeDefaultLogPath` passes a copy of the `std::tm` from `std::localtime(&time)` to fmt's chrono format (`{:%Y%m%d-%H%M%S}`) instead of `fmt::localtime(time)`, as the image-dump name in `src/host/cli/main.cpp` already uses `std::localtime`; the function runs once at start, so sharing `std::localtime`'s static buffer is harmless, MSVC stays quiet under the defined `_CRT_SECURE_NO_WARNINGS`, and `localtime_r`/`localtime_s` are avoided in the platform-neutral core. 3. Update where the pinned version is written: `THIRD_PARTY_NOTICES.md`, `docs/sites/site.toml` and the license link in kb `spdlog-runtime-logging.md`; earlier tasks' designs, work orders and logs stay as records, and CREDITS names no version.*

## 검증 / Verification

- Linux x64: GCC 15.2 Debug(경고를 오류로)·Release, clang 21.1.8 Debug(CI 구성, 경고를 오류로). CTest와 실제 실행에서 로그 파일 이름(`re2dj-YYYYMMDD-HHMMSS-mmm.log`)이 그대로인지 확인한다.
- Linux x86: GCC 15.2 Debug(경고를 오류로)와 CTest.
- Windows x86 MSVC와 CI의 clang 18·GCC 12는 이 머신에서 돌릴 수 없으므로 push 뒤 CI에서 확인한다.

*Linux x64: GCC 15.2 Debug (warnings as errors) and Release, and clang 21.1.8 Debug (the CI configuration, warnings as errors), checking CTest and that a real run keeps the log file name (`re2dj-YYYYMMDD-HHMMSS-mmm.log`); Linux x86: GCC 15.2 Debug (warnings as errors) and CTest. Windows x86 MSVC and CI's clang 18 and GCC 12 cannot run on this machine and are checked in CI after the push.*
