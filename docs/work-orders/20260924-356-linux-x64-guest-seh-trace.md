# 작업 356 작업 지시서 — Linux x64 게스트 SEH 디스패치와 instruction trace / Task 356 work order — Linux x64 guest SEH dispatch and instruction trace

설계: [20260924-356-linux-x64-guest-seh-trace.md](../design/20260924-356-linux-x64-guest-seh-trace.md)
선행: [작업 355 작업 로그](../work-logs/20260924-355-linux-x64-facade-diagnostics.md)

## 단계 / Steps

1. `NativeTrapRegisters`를 추가한다. SEH 타입과 판단 로직을 `native_guest_seh`로, trace 선언과 상태기계를 `native_instruction_trace`로 공용화한다. x86 bootstrap이 이것을 쓰도록 바꾼다.
2. x64 asm signal entry가 C handler 반환값에 따라 게스트 FS를 다시 적재하고 게스트로 복귀하게 한다. C handler에 trace와 SEH 디스패치(중첩 전환, host `rsp` 슬롯 저장·복원)를 추가한다. import landing에서 trace를 재개한다.
3. x64 bootstrap이 SEH 통계를 보고하게 한다. GetVersion 진단을 루트로 되돌리고 x64 stub을 지운다.
4. compat-mode probe에 SEH 검사를, x64 in-process probe에 trace 검사를 추가한다.
5. Linux x64·x86 debug와 helper를 빌드해 CTest, probe, helper 스크립트, x64 Clang을 실행한다. 실제 4th CHD 다섯 진단을 두 폭에서 비교한다.
6. 분석, `ARCHITECTURE.md`, README, 설계 353, KB, `docs/TODO.md`, 작업 로그를 갱신하고 커밋한다.

*Steps: (1) add `NativeTrapRegisters`, share the SEH types and decision logic as `native_guest_seh` and the trace declarations and state machine as `native_instruction_trace`, and move the x86 bootstrap onto them; (2) make the x64 asm signal entry reload the guest FS and return into the guest based on the C handler's result, add the trace and SEH dispatch (nested transition with host `rsp` slot save/restore) to the C handler, and resume the trace at the import landing; (3) have the x64 bootstrap report SEH statistics, move the GetVersion diagnostic back to the root, and delete the x64 stub; (4) add an SEH check to the compat-mode probe and a trace check to the x64 in-process probe; (5) build Linux x64/x86 debug and the helper, run CTest, the probes, the helper script, and x64 Clang, and compare the five real-4th-CHD diagnostics on both widths; (6) update the analysis, `ARCHITECTURE.md`, README, design 353, KB, `docs/TODO.md`, and the work log, then commit.*

## 완료 조건 / Completion criteria

* x64에서 실제 4th CHD가 `#0016 GetProcAddress(ExitProcess)`에 도달하고, SEH 통계와 API 기록이 x86과 같다.
* x64 GetVersion 진단이 x86과 같은 경계를 기록한다.
* x86 결과가 작업 355와 같다.

*Completion: on x64 the real 4th CHD reaches `#0016 GetProcAddress(ExitProcess)` with SEH statistics and API records equal to x86; the x64 GetVersion diagnostic records the same boundary as x86; and the x86 results equal Task 355.*
