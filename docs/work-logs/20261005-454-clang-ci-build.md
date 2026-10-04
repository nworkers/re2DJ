# 작업 454 작업 로그 — CI의 Linux x64 clang 빌드 실패 / Task 454 work log — the failing Linux x64 clang build in CI

설계: [20261005-454-clang-ci-build.md](../design/20261005-454-clang-ci-build.md) · 지시서: [20261005-454-clang-ci-build.md](../work-orders/20261005-454-clang-ci-build.md)

## 2026-10-05

- **원인 확인**: 사용자가 v0.0.63 push의 `ci` 실패(run 37214004220)를 알렸다. 공개 API로 보면 `linux-x64 (clang)`의 Build가 실패했고 `linux-x64 (gcc)`는 취소, `windows-x86`·`linux-x86`은 성공이었다. 같은 작업이 v0.0.60(8cf7e69)·v0.0.61(00bddac)과 dfa8e69에서도 실패했다(그때는 `windows-x86` Test도 실패했으나 이번에는 통과). 작업 로그는 인증이 필요해 받지 못했고, WSL clang 18.1.3으로 CI 구성을 빌드해 `kernel32_module.cpp:825`의 `-Wunused-const-variable` 오류 하나를 재현했다.
- **수정**: `kHeapGenerateExceptions` 한 줄 삭제.

  *Cause: the user reported the `ci` failure on the v0.0.63 push (run 37214004220). The public API showed `linux-x64 (clang)` failing at Build, `linux-x64 (gcc)` cancelled and `windows-x86` and `linux-x86` passing; the same job had failed on v0.0.60 (8cf7e69), v0.0.61 (00bddac) and dfa8e69 (with `windows-x86` Test failing then as well, passing now). The job log needs authentication, so the CI configuration was built with WSL clang 18.1.3, reproducing a single `-Wunused-const-variable` error at `kernel32_module.cpp:825`. Fix: the one `kHeapGenerateExceptions` line removed.*

- **검증**
  - WSL clang 18.1.3, `linux-x64-debug` + `RE2DJ_WARNINGS_AS_ERRORS=ON`(`build/linux-x64-clang`): 전체 빌드 성공, 경고 없음, CTest 5개 통과.
  - WSL gcc, 같은 구성(`build/linux-x64-gcc-ci`): 전체 빌드 성공, 경고 없음, CTest 5개 통과. CI에서 취소되어 보이지 않던 gcc 쪽도 통과함을 확인했다.
  - Windows·Linux x86은 CI에서 이미 성공했고 이 수정은 Linux·Windows 공용 HLE의 상수 삭제뿐이라 따로 돌리지 않았다. 실제 결과는 push 뒤 CI에서 확인한다.

  *Verification: WSL clang 18.1.3 with `linux-x64-debug` and `RE2DJ_WARNINGS_AS_ERRORS=ON` (`build/linux-x64-clang`) builds in full with no warnings and passes 5 CTest tests; WSL gcc with the same configuration (`build/linux-x64-gcc-ci`) does the same, confirming the gcc side CI had been cancelling. Windows and Linux x86 already passed in CI and the fix only deletes a constant in the shared HLE, so they were not rerun; the real result comes from CI after the push.*
