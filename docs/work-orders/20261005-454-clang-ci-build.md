# 작업 454 작업 지시서 — CI의 Linux x64 clang 빌드 실패 / Task 454 work order — the failing Linux x64 clang build in CI

설계: [20261005-454-clang-ci-build.md](../design/20261005-454-clang-ci-build.md)

## 절차 / Steps

1. `src/hle/modules/kernel32_module.cpp`에서 쓰이지 않는 `kHeapGenerateExceptions`를 지운다.
   *Remove the unused `kHeapGenerateExceptions` from `src/hle/modules/kernel32_module.cpp`.*
2. WSL에서 CI와 같은 구성을 clang과 gcc로 각각 빌드하고 CTest를 돌린다.
   *In WSL, build the CI configuration with clang and with gcc and run CTest.*

## 완료 조건 / Done when

두 컴파일러 모두 경고를 오류로 둔 빌드가 성공하고 CTest가 통과한다.

*Both compilers build with warnings as errors and CTest passes.*
