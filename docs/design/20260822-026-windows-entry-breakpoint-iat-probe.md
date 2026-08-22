# Windows Entry Hardware-Breakpoint IAT Probe

## 한국어

첫 debugger breakpoint는 import binding 이전이었다. 이 probe는 그 지점에서 primary WOW64 thread의 `DR0`에 원본 PE entry VA를 설정하고 `DR7` local execution breakpoint를 켠다. debugger가 loader event를 계속 처리하면 CPU는 guest entry의 첫 명령을 실행하기 전에 `EXCEPTION_SINGLE_STEP`을 보고한다.

그 정지점에서 IAT를 읽기 전용으로 검증한다. `VirtualProtectEx`, `WriteProcessMemory`, injection, entry continue는 금지한다.

## English

The first debugger breakpoint was before import binding. This probe sets the original PE entry VA in `DR0` and enables a local execution breakpoint in `DR7` on the primary WOW64 thread at that point. After continuing loader events, the CPU reports `EXCEPTION_SINGLE_STEP` before it executes the first guest entry instruction.

The IAT is verified read-only at that stop. `VirtualProtectEx`, `WriteProcessMemory`, injection, and entry continuation are prohibited.
