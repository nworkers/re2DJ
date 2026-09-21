# 작업 지시 335: Linux x86 GetVersion thunk 연속 실행 진단 / Work order 335: Linux x86 GetVersion thunk continuation diagnostic

Linux x86 in-process resolver diagnostic이 `GetProcAddress(kernel32, "GetVersion")`에 실행 가능한 process-local thunk를 반환하게 합니다. 원본이 그 주소를 호출하면 dynamic gate와 caller return을 관측하고, 호출 전에 멈추면 signal/EIP 경계를 명시적으로 기록합니다.

*Make the Linux x86 in-process resolver diagnostic return an executable process-local thunk for `GetProcAddress(kernel32, "GetVersion")`. Observe the dynamic gate and caller return if the original calls that address; otherwise explicitly record the signal/EIP boundary where it stops before the call.*

완료 기준은 다음과 같습니다.

*The completion criteria are:*

1. 동적 thunk가 기존 import bridge ABI를 사용하고 zero-argument cleanup을 적용합니다.
   *The dynamic thunk uses the existing import-bridge ABI and applies zero-argument cleanup.*
2. 실제 4th CHD 실행이 호출 도달 또는 호출 전 중단을 구분해 출력합니다.
   *A real 4th CHD run distinguishes call arrival from a stop before the call.*
3. Linux x86 product build와 synthetic in-process regression이 통과합니다.
   *The Linux x86 product build and synthetic in-process regression pass.*
4. analysis, architecture, work log와 하나의 Git commit을 남깁니다.
   *Leave analysis, architecture, a work log, and one Git commit.*