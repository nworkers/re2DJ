# 작업 460 작업 지시서 — spdlog 1.15.3으로 올리기 / Task 460 work order — moving to spdlog 1.15.3

설계: [20261005-460-spdlog-fmt-clang21.md](../design/20261005-460-spdlog-fmt-clang21.md)

## 절차 / Steps

1. `CMakeLists.txt`: `find_package`와 FetchContent의 spdlog 버전을 1.15.3으로 바꾼다.
   *Change the spdlog version in `find_package` and FetchContent to 1.15.3.*
2. `src/logging/logging.cpp`: `fmt::localtime`을 `std::localtime`이 돌려준 `std::tm` 복사본으로 바꾼다.
   *Replace `fmt::localtime` with a copy of the `std::tm` from `std::localtime`.*
3. `THIRD_PARTY_NOTICES.md`, `docs/sites/site.toml`, kb `spdlog-runtime-logging.md`의 버전과 링크를 고친다.
   *Update the version and links in `THIRD_PARTY_NOTICES.md`, `docs/sites/site.toml` and kb `spdlog-runtime-logging.md`.*
4. 설계의 검증 항목을 실행한다.
   *Run the design's verification.*

## 완료 조건 / Done when

GCC 15.2(x64·x86)와 clang 21.1.8(x64)이 경고를 오류로 둔 빌드에 성공하고 CTest가 통과하며, 로그 파일 이름이 바뀌지 않는다.

*GCC 15.2 (x64 and x86) and clang 21.1.8 (x64) build with warnings as errors, CTest passes, and the log file name is unchanged.*
