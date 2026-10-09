# 작업 460 작업 로그 — spdlog 1.15.3으로 올리기 / Task 460 work log — moving to spdlog 1.15.3

설계: [20261005-460-spdlog-fmt-clang21.md](../design/20261005-460-spdlog-fmt-clang21.md) · 지시서: [20261005-460-spdlog-fmt-clang21.md](../work-orders/20261005-460-spdlog-fmt-clang21.md)

## 2026-10-05

- **발견**: [작업 458](20261005-458-linux-desktop-validation.md) 도중 사용자가 clang을 설치했다(Ubuntu clang 21.1.8). CI 구성 빌드가 spdlog v1.14.1의 번들 fmt 10.2.1에서 `consteval` 오류로 실패했다. 별도 빌드 디렉터리에서 `FETCHCONTENT_SOURCE_DIR_SPDLOG`로 v1.15.3을 시험해, re2DJ 쪽에는 `fmt::localtime` deprecated 경고 하나만 남는다는 것을 확인했다. 사용자가 진행을 승인했다.
- **구현**: `CMakeLists.txt`의 `find_package`·`GIT_TAG`를 1.15.3으로 바꿨다. `logging.cpp`의 `MakeDefaultLogPath`는 `std::localtime`이 돌려준 `std::tm` 복사본을 형식에 넘긴다. `THIRD_PARTY_NOTICES.md`, `docs/sites/site.toml`, kb `spdlog-runtime-logging.md`의 버전과 링크를 고쳤다.

  *Discovery: during [task 458](20261005-458-linux-desktop-validation.md) the user installed clang (Ubuntu clang 21.1.8), and the CI configuration failed with `consteval` errors in spdlog v1.14.1's bundled fmt 10.2.1; a separate build directory tried v1.15.3 through `FETCHCONTENT_SOURCE_DIR_SPDLOG` and left only the `fmt::localtime` deprecation on re2DJ's side, and the user approved going ahead. Implementation: `find_package` and `GIT_TAG` in `CMakeLists.txt` move to 1.15.3, `MakeDefaultLogPath` in `logging.cpp` formats a copy of the `std::tm` from `std::localtime`, and the version and links in `THIRD_PARTY_NOTICES.md`, `docs/sites/site.toml` and kb `spdlog-runtime-logging.md` are updated.*

- **검증**(Ubuntu 26.04)
  - clang 21.1.8, `linux-x64-debug` + `RE2DJ_WARNINGS_AS_ERRORS=ON`(새 빌드 디렉터리 `build/linux-x64-clang`, 번들 fmt `FMT_VERSION 110200`): 전체 빌드 성공(경고 없음), CTest 5개 통과.
  - GCC 15.2, `linux-x64-debug`·`linux-x86-debug`(경고를 오류로): 둘 다 fmt 11.2.0으로 다시 받아 빌드 성공(경고 없음), CTest 각 5개 통과. `linux-x64-release`도 경고 없이 빌드.
  - 실제 실행(4th `--post-shader crt`, 12초): x64 GCC Release, x64 clang Debug, x86 GCC Debug 모두 `Log file: logs/re2dj-20261005-0942xx-mmm.log` 형식(현지 시각)이 그대로였고, crt 적용 뒤 `host window closed`로 끝났다.
  - `site.toml`은 `tomllib`로 읽혀 spdlog `v1.15.3`을 돌려준다. 사이트 전체 빌드는 이 머신에 `mdit_py_plugins`가 없어 돌리지 못했다. Pages 워크플로에서 확인한다.
  - **하지 못한 것**: Windows x86 MSVC 빌드와 CI의 clang 18·GCC 12. 이 머신에서 돌릴 수 없으므로 push 뒤 CI에서 확인한다.

  *Verification (Ubuntu 26.04): clang 21.1.8 with `linux-x64-debug` and `RE2DJ_WARNINGS_AS_ERRORS=ON` (fresh `build/linux-x64-clang`, bundled `FMT_VERSION 110200`) builds in full with no warnings and passes 5 CTest tests; GCC 15.2 `linux-x64-debug` and `linux-x86-debug` (warnings as errors), both refetched with fmt 11.2.0, build with no warnings and pass 5 CTest tests each, and `linux-x64-release` builds with no warnings; real 12-second 4th runs with `--post-shader crt` from the x64 GCC Release, x64 clang Debug and x86 GCC Debug builds keep the `Log file: logs/re2dj-20261005-0942xx-mmm.log` form in local time and end with `host window closed` after applying crt; `site.toml` parses with `tomllib` and gives spdlog `v1.15.3`, while the full site build could not run here without `mdit_py_plugins` and is left to the Pages workflow. Not done: the Windows x86 MSVC build and CI's clang 18 and GCC 12, which cannot run on this machine, to be checked in CI after the push.*

- **CI**: 브랜치 push 뒤 `ci` [실행 37251046072](https://github.com/reexec/re2DJ/actions/runs/37251046072)에서 `windows-x86`(MSVC), `linux-x86`(Debian 12 GCC 12), `linux-x64` gcc·clang이 모두 성공했다([#1 로그](20261005-i001-branch-ci-and-issue-workflow.md)).

  *CI: after the branch push, `ci` [run 37251046072](https://github.com/reexec/re2DJ/actions/runs/37251046072) passed `windows-x86` (MSVC), `linux-x86` (Debian 12 GCC 12) and `linux-x64` gcc and clang ([#1 log](20261005-i001-branch-ci-and-issue-workflow.md)).*
