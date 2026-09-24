# 작업 355 작업 지시서 — Linux x64 facade와 in-process 진단 연결 / Task 355 work order — Linux x64 facades and in-process diagnostics

설계: [20260924-355-linux-x64-facade-diagnostics.md](../design/20260924-355-linux-x64-facade-diagnostics.md)
선행: [작업 354 작업 로그](../work-logs/20260924-354-linux-x64-pe-session.md)

## 단계 / Steps

1. `native_guest_module_image`, `native_guest_module_set`, `native_kernel32_diagnostic`, `native_dynamic_thunk`를 루트로 `git mv`하고 include를 고친다. dynamic thunk와 stop stub을 `MapNativeLowMemory`로 할당한다.
2. `original_runner.cpp`의 GetVersion 진단을 `x86/native_getversion_observation.cpp`로 옮기고, `x64/native_getversion_observation.cpp`에 명시적 미지원 구현을 둔다. 나머지 `#if defined(__i386__)`를 `original_runner.cpp`, `native_create_file_observation.cpp`, `native_continuation_observation.cpp`에서 제거한다.
3. x64 in-process probe에 합성 facade·dynamic thunk 검사를 추가한다.
4. CMake 목록을 갱신하고 Linux x64·x86 debug와 helper를 빌드한 뒤 CTest, probe, helper 스크립트를 실행한다.
5. 실제 4th CHD로 x64·x86 다섯 진단을 실행해 비교한다.
6. 분석 문서, `ARCHITECTURE.md`, `src/platform/linux/README.md`, 설계 353, `docs/TODO.md`, 작업 로그를 갱신하고 커밋한다.

*Steps: (1) `git mv` `native_guest_module_image`, `native_guest_module_set`, `native_kernel32_diagnostic`, and `native_dynamic_thunk` to the root, fix includes, and allocate dynamic thunks and the stop stub with `MapNativeLowMemory`; (2) move the GetVersion diagnostic from `original_runner.cpp` to `x86/native_getversion_observation.cpp`, put an explicit unsupported implementation in `x64/native_getversion_observation.cpp`, and remove the remaining `#if defined(__i386__)` from `original_runner.cpp`, `native_create_file_observation.cpp`, and `native_continuation_observation.cpp`; (3) add synthetic facade and dynamic-thunk checks to the x64 in-process probe; (4) update the CMake lists, build Linux x64/x86 debug and the helper, and run CTest, the probes, and the helper script; (5) run the five diagnostics on the real 4th CHD on x64 and x86 and compare; (6) update the analysis, `ARCHITECTURE.md`, `src/platform/linux/README.md`, design 353, `docs/TODO.md`, and the work log, then commit.*

## 완료 조건 / Completion criteria

* x64 합성 facade 검사와 기존 x64·x86 CTest가 통과한다.
* 실제 4th CHD의 x64 결과가 설계의 비교 항목과 일치하고, x86 결과는 작업 354와 같다.

*Completion: the x64 synthetic facade check and the existing x64/x86 CTest runs pass; on the real 4th CHD, the x64 results match the design's comparison items and the x86 results equal Task 354.*
