# 작업 지시 348: 앞선 resolver 진단의 facade 합류 / Work order 348: Early resolver diagnostics convergence

## 목표 / Objective

[작업 348 설계](../design/20260923-348-early-resolver-diagnostics-convergence.md)에 따라 `original_runner.cpp`에 남은 두 Linux i386 진단(`--linux-in-process-first-resolver`, `--linux-in-process-getversion-call`)의 pseudo `kernel32` 경로를 작업 344의 facade와 registry 경로로 교체하고, 세 resolver 진단이 공용 진단 문맥 하나를 쓰도록 정리합니다.

*Following the [Task 348 design](../design/20260923-348-early-resolver-diagnostics-convergence.md), replace the pseudo-`kernel32` path in the two remaining Linux i386 diagnostics in `original_runner.cpp` (`--linux-in-process-first-resolver`, `--linux-in-process-getversion-call`) with Task 344's facade and registry path, and have all three resolver diagnostics share one diagnostic context.*

## 작업 / Work

1. `CreateFileContext`의 services 구현, session 준비 콜백, identity 관측을 `src/platform/linux/x86/native_kernel32_diagnostic.{h,cpp}`의 `NativeKernel32Diagnostic`으로 추출하고 CMake에 등록합니다.
2. identity 필드를 `OriginalCreateFileObservation`에서 새 `OriginalResolverIdentity`로 옮기고 `registry_get_version`, `unhandled_import`를 추가합니다.
3. `RunOriginalInProcessCreateFileCall`을 공용 문맥으로 옮깁니다. 동작은 바꾸지 않습니다.
4. `RunOriginalInProcessFirstResolver`에서 pseudo handle과 EAX=1 응답을 제거하고, `GetProcAddress` facade gate의 `GetVersion` 요청 복귀 지점에서 제한합니다.
5. `RunOriginalInProcessGetVersionCall`에서 pseudo handle과 `0xF1000001` dynamic thunk를 제거하고, `GetVersion` facade gate 호출 복귀 지점에서 제한합니다. 미처리 동적 요청과 미처리 정적 import를 관측한 그대로 기록합니다.
6. CLI가 세 진단 모두에서 identity와 미처리 요청을 출력하도록 갱신합니다.
7. 빌드·단위 테스트·probe와 실제 4th CHD 세 진단을 실행하고, analysis·ARCHITECTURE·TODO·작업 로그를 갱신합니다.

*Extract the services, setup callback, and identity observation into `NativeKernel32Diagnostic` and register it in CMake; move identity fields into a new `OriginalResolverIdentity` with `registry_get_version` and `unhandled_import`; move the CreateFileA diagnostic onto the shared context unchanged; remove the pseudo handle and EAX=1 reply from the first-resolver diagnostic and bound it at the `GetVersion` request's return through the `GetProcAddress` facade gate; remove the pseudo handle and `0xF1000001` dynamic thunk from the GetVersion-call diagnostic and bound it at the return of the `GetVersion` facade-gate call, recording unhandled dynamic requests and static imports as observed; update the CLI to print identity and unhandled requests for all three; then run builds, unit tests, probes, and all three diagnostics against the real 4th CHD, and update analysis, ARCHITECTURE, TODO, and the work log.*

## 완료 조건 / Completion criteria

- `src/platform/linux/`에 `0x7F000001`과 `0xF1000001`이 남지 않습니다.
- 실제 4th CHD의 `--linux-in-process-createfile-call`이 작업 344와 같은 identity와 `\\.\NTICE` 경계를 재현합니다.
- 실제 4th CHD의 `--linux-in-process-first-resolver`가 caller 복귀 `0x00af0b99`, SIGTRAP EIP `0x00af0b9a`를 유지하고, 게스트가 받은 `GetVersion` 주소가 registry 값과 일치합니다.
- `--linux-in-process-getversion-call`의 새 경계를 관측한 그대로 analysis에 확인됨/미확정으로 기록합니다.
- 원본 CHD는 읽기 전용으로 사용하고 추출 PE를 저장소에 추가하지 않습니다.
- 검증 결과와 남은 범위를 작업 로그에 기록하고 하나의 Git 커밋으로 남깁니다.

*No `0x7F000001` or `0xF1000001` remains under `src/platform/linux/`; the CreateFileA diagnostic reproduces Task 344's identity and `\\.\NTICE` boundary; the first-resolver diagnostic keeps caller return `0x00af0b99` and SIGTRAP EIP `0x00af0b9a` with the guest-received `GetVersion` address matching the registry; the GetVersion-call diagnostic's new boundary is recorded as observed, split into confirmed and unresolved; the original CHD stays read-only and no extracted PE is added; and validation plus remaining scope are recorded in one task commit.*
