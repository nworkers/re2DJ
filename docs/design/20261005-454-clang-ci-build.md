# 작업 454 설계 — CI의 Linux x64 clang 빌드 실패 / Task 454 design — the failing Linux x64 clang build in CI

## 문제 / Problem

GitHub Actions `ci`의 `linux-x64 (clang)` 작업이 2026-10-01(dfa8e69)부터 빌드 단계에서 계속 실패했다. v0.0.63(0814b2c) push에서도 같았다. 이 작업이 실패하면 matrix의 `linux-x64 (gcc)`가 취소되어 gcc 결과도 보이지 않았다.

WSL clang 18.1.3으로 CI와 같은 구성(`cmake --preset linux-x64-debug -DRE2DJ_WARNINGS_AS_ERRORS=ON`)을 빌드하니 오류는 하나였다. `src/hle/modules/kernel32_module.cpp`의 `kHeapGenerateExceptions`가 어디에서도 쓰이지 않아 `-Wunused-const-variable`이 `-Werror`로 오류가 된다. gcc는 이 경고를 내지 않아 로컬 Linux 빌드에서는 드러나지 않았다.

*The `linux-x64 (clang)` job of the `ci` workflow had failed at its build step since 2026-10-01 (dfa8e69), the v0.0.63 push (0814b2c) included, and its failure cancelled the matrix's `linux-x64 (gcc)` job so gcc results were never shown. Building the CI configuration (`cmake --preset linux-x64-debug -DRE2DJ_WARNINGS_AS_ERRORS=ON`) with WSL clang 18.1.3 gives one error: `kHeapGenerateExceptions` in `src/hle/modules/kernel32_module.cpp` is used nowhere, and `-Wunused-const-variable` becomes an error under `-Werror`. gcc does not issue this warning, so local Linux builds never showed it.*

## 결정 / Decision

상수를 지운다. `CheckHeapFlags`는 허용 목록(`allowed | HEAP_NO_SERIALIZE`) 밖의 플래그를 모두 거부하므로 `HEAP_GENERATE_EXCEPTIONS`도 이름 없이 이미 거부되고, 그 사실은 함수 위 주석에 적혀 있다. 동작은 바뀌지 않는다.

*Remove the constant. `CheckHeapFlags` refuses every flag outside the allowed set (`allowed | HEAP_NO_SERIALIZE`), so `HEAP_GENERATE_EXCEPTIONS` is already refused without a name, as the comment above the function says; behaviour does not change.*
