# 작업 354 작업 지시서 — Linux x64 PE session과 in-process runner / Task 354 work order — Linux x64 PE session and in-process runner

설계: [20260924-354-linux-x64-pe-session.md](../design/20260924-354-linux-x64-pe-session.md)
선행: [작업 353 작업 로그](../work-logs/20260924-353-linux-x64-compat-mode-adapter.md)

## 단계 / Steps

1. `native_pe_image`, `native_import_thunks`, `native_pe_session`, `native_fault_observation`, `native_in_process_runner`, `native_process_bootstrap.h`, `native_import_bridge.h`를 `x86/`에서 루트로 `git mv`한다. instruction trace 선언은 `x86/native_instruction_trace.h`로 분리한다.
2. 루트에 `native_low_memory.h`를 두고 `x86/`·`x64/` 구현을 추가한다. import thunk 영역이 이것을 쓰도록 바꾼다.
3. x64 전환 page를 프로세스 전역으로 바꾸고, `x64/native_import_bridge.cpp`와 `x64/native_process_bootstrap.cpp`를 추가한다.
4. `x64/native_in_process_probe.cpp`를 추가하고 CMake와 CTest에 등록한다. x86·helper source 목록과 include 경로를 갱신한다.
5. Linux x64 debug(GCC)·x86 debug·x86 helper를 빌드하고 CTest와 probe를 실행한다. x64는 Clang으로도 확인한다.
6. `ARCHITECTURE.md`, `src/platform/linux/README.md`, 설계 353, `docs/TODO.md`, 작업 로그를 갱신하고 커밋한다.

*Steps: (1) `git mv` `native_pe_image`, `native_import_thunks`, `native_pe_session`, `native_fault_observation`, `native_in_process_runner`, `native_process_bootstrap.h`, and `native_import_bridge.h` from `x86/` to the root, splitting the instruction-trace declarations into `x86/native_instruction_trace.h`; (2) add a root `native_low_memory.h` with `x86/` and `x64/` implementations and have the import-thunk region use it; (3) make the x64 transition page process-global and add `x64/native_import_bridge.cpp` and `x64/native_process_bootstrap.cpp`; (4) add `x64/native_in_process_probe.cpp`, register it with CMake and CTest, and update the x86/helper source lists and include paths; (5) build Linux x64 debug (GCC), x86 debug, and the x86 helper, run CTest and the probes, and also check x64 with Clang; (6) update `ARCHITECTURE.md`, `src/platform/linux/README.md`, design 353, `docs/TODO.md`, and the work log, then commit.*

## 완료 조건 / Completion criteria

* x64 in-process probe가 설계 검증 항목 1~3을 통과한다.
* Linux x86 기존 probe·단위 테스트와 helper build가 회귀 없이 통과한다.

*Completion: the x64 in-process probe passes design checks 1–3, and the existing Linux x86 probes, unit tests, and helper build pass without regression.*
