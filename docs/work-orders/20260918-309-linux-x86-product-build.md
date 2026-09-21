# 작업 309 — Linux x86 제품 빌드 / Task 309 — Linux x86 Product Build

설계: [Linux x86 제품 호스트 빌드](../design/20260918-309-linux-x86-product-build.md).

*Design: [Linux x86 product-host build](../design/20260918-309-linux-x86-product-build.md).*

## 구현 범위 / Scope

1. `CMakeLists.txt`에서 Linux native backend·CLI 연결을 x86/x64 공통으로 만든다. helper-only preset은 SDL product targets를 만들지 않는다.
2. `CMakePresets.json`에 `linux-x86-debug`, `linux-x86-release`와 build/test preset을 추가한다.
3. `scripts/test_linux_native_helper_probe.sh`가 x64와 x86 product host probe를 모두 실행한다.
4. Linux SDL3 build guide를 x86 dependency와 ELF 확인 명령으로 갱신한다.
5. 결과를 작업 로그와 analysis에 기록하고 x64·x86·Windows 검증 후 커밋한다.

*Implement shared Linux backend/CLI linkage for x86/x64 while keeping helper-only builds free of SDL product targets; add x86 Debug/Release configure/build/test presets; run host probes from both product hosts; update the Linux SDL3 guide with x86 dependencies and ELF checks; record results and commit after x64, x86, and Windows verification.*

## 검증 / Verification

Use `-DRE2DJ_WARNINGS_AS_ERRORS=ON`, inspect ELF classes with `file`, run both CTest presets, and run each host probe against the same i386 helper. Do not use original HDD assets.

*Use warnings-as-errors, inspect ELF classes with `file`, run both CTest presets, and run each host probe against the same i386 helper. Do not use original HDD assets.*

진행 결과는 [작업 로그](../work-logs/20260918-309-linux-x86-product-build.md)와 [Linux 기준선 analysis](../analysis/linux-runtime-baseline.md)에 기록한다.

*Record the result in the [work log](../work-logs/20260918-309-linux-x86-product-build.md) and [Linux baseline analysis](../analysis/linux-runtime-baseline.md).*
