# 작업 지시 338: Linux x86 resolver-continuation instruction trace / Work order 338: Linux x86 resolver-continuation instruction trace

4th Trax의 GetProcAddress(kernel32, GetVersion) 반환 직후부터 null read까지의 실제 명령어와 레지스터 흐름을 제한적으로 수집합니다. process-local return-address breakpoint와 trap flag single-step을 사용하되 최대 128개 frame으로 제한하고, 원본 바이트 복원과 terminal 상태 구분을 보장합니다.

*Collect the actual instruction and register flow from immediately after 4th Trax GetProcAddress(kernel32, GetVersion) returns until the null read. Use a process-local return-address breakpoint and trap-flag single stepping, limit it to 128 frames, and guarantee original-byte restoration and distinct terminal states.*

완료 기준은 다음과 같습니다.

*Completion criteria:*

1. trace controller가 arm, 시작, 한도 도달, 정리 상태를 구조화해 보존합니다.
   *The trace controller structurally preserves arm, start, limit, and cleanup state.*
2. signal handler가 breakpoint와 trace trap을 구분하여 제한된 register frame만 기록합니다.
   *The signal handler distinguishes the breakpoint from trace traps and records only bounded register frames.*
3. synthetic probe와 실제 4th CHD 진단으로 trace 결과를 검증하고 analysis를 갱신합니다.
   *Validate trace results with the synthetic probe and real 4th CHD diagnostic, then update analysis.*
4. Linux x86/x64 build, 작업 로그, Git commit을 남깁니다.
   *Leave Linux x86/x64 builds, a work log, and a Git commit.*