# 작업 지시 336: Linux x86 fault context 관측 / Work order 336: Linux x86 fault-context observation

`--linux-in-process-getversion-call`에서 확인된 `SIGSEGV`의 원인을 다음 작업으로 넘기지 않기 위해, Linux x86 in-process runner에 구조화된 fault context 관측을 추가합니다. 원인을 가정한 HLE 반환값은 추가하지 않습니다.

*Add structured fault-context observation to the Linux x86 in-process runner so the `SIGSEGV` found by `--linux-in-process-getversion-call` is investigated in this task. Do not add guessed HLE return values.*

완료 기준은 다음과 같습니다.

*The completion criteria are:*

1. signal address/code, x86 error code와 일반 레지스터가 `NativeGuestFault`에 보존됩니다.
   *Signal address/code, the x86 error code, and general registers are preserved in `NativeGuestFault`.*
2. image/stack 범위를 확인한 bounded instruction 및 stack window가 in-process 결과로 전달됩니다.
   *Range-checked bounded instruction and stack windows are delivered in the in-process result.*
3. 실제 4th CHD가 `0x00af0c22` fault context를 출력하고, synthetic `UD2` probe가 window 내용을 검증합니다.
   *The real 4th CHD prints the `0x00af0c22` fault context, and the synthetic `UD2` probe verifies window contents.*
4. Linux x86/x64 product builds, 관련 probe, 작업 로그가 완료됩니다.
   *Linux x86/x64 product builds, the relevant probe, and a work log are complete.*