# 단일 프로세스 x86 실행 backend / Single-process x86 execution backends

## 결정 / Decision

원본 x86 PE와 HLE import 처리는 가능한 경우 하나의 runtime 프로세스 안에서 실행한다. Wine 코드나 Wine runtime은 사용하지 않는다. `ExecutionBackend`는 이 공통 목표를 유지하되, x64 Linux와 x64 Windows의 운영체제 계약 차이 때문에 mode-transition 방법은 플랫폼별로 분리한다.

*Run original x86 PE code and HLE import handling in one runtime process whenever possible. Do not use Wine code or the Wine runtime. `ExecutionBackend` retains this shared goal, while platform-specific mode-transition methods remain separate because x64 Linux and x64 Windows have different operating-system contracts.*

| Host | 선택한 runtime 구조 | 상태 |
| --- | --- | --- |
| Linux x86 | 하나의 i386 process에서 host policy, HLE, guest PE 실행 | 설계 대상 |
| Linux x64 | 하나의 x86-64 process에서 host/HLE와 x86 compatibility-mode guest를 trampoline으로 전환 | 설계 대상 |
| Windows x64 | OS WoW64가 호스팅하는 하나의 i386 runtime process에서 HLE와 guest PE 실행 | 기존 Windows 방향과 정렬, 설계 대상 |

| Host | Selected runtime structure | Status |
| --- | --- | --- |
| Linux x86 | One i386 process executes host policy, HLE, and guest PE | Design target |
| Linux x64 | One x86-64 process switches host/HLE and x86 compatibility-mode guest through trampolines | Design target |
| Windows x64 | One i386 runtime process hosted by OS WoW64 executes HLE and guest PE | Aligned with the existing Windows direction, design target |

## Windows 경계 / Windows boundary

Windows x64에서 Linux Wine-WoW64식 사용자 구현을 그대로 복제하여 64비트 re2DJ process 안에 32비트 PE와 32비트 HLE DLL을 실행용으로 로드하는 것은 선택하지 않는다. Microsoft 문서는 64비트 process가 32비트 DLL을 실행용으로 load할 수 없고, 반대로도 불가하다고 명시한다. Windows의 지원되는 경로는 OS WoW64가 만든 32비트 process이며, 이 안에서는 guest와 re2DJ HLE runtime을 모두 i386으로 둔다.

*Do not choose a direct copy of Linux Wine-WoW64-style user implementation that loads a 32-bit PE and 32-bit HLE DLL for execution inside a 64-bit re2DJ process on Windows x64. Microsoft documents that a 64-bit process cannot load a 32-bit DLL for execution, and vice versa. The supported Windows path is a 32-bit process created by OS WoW64; both guest and re2DJ HLE runtime are i386 inside it.*

Windows WoW64는 32비트 NTDLL과 64비트 kernel 경계 사이의 thunk를 운영체제가 담당한다. re2DJ는 이를 다시 구현하지 않으며, Win32 import HLE와 원본 PE 실행 경계를 32비트 runtime 내부에 둔다. 근거: [Microsoft WOW64 implementation details](https://learn.microsoft.com/en-us/windows/desktop/WinProg64/wow64-implementation-details), [Running 32-bit applications](https://learn.microsoft.com/en-us/windows/win32/winprog64/running-32-bit-applications).

*Windows WoW64 owns the thunk between 32-bit NTDLL and the 64-bit kernel boundary. re2DJ does not reimplement it; Win32-import HLE and original PE execution remain inside the 32-bit runtime. Sources: [Microsoft WOW64 implementation details](https://learn.microsoft.com/en-us/windows/desktop/WinProg64/wow64-implementation-details), [Running 32-bit applications](https://learn.microsoft.com/en-us/windows/win32/winprog64/running-32-bit-applications).* 

## Linux x64 경계 / Linux x64 boundary

Linux x64 backend는 x86 compatibility-mode guest code와 64비트 host/HLE trampoline을 같은 thread에서 전환한다. guest PE와 stack은 32비트 addressable range에 mapping하고, transition마다 x86 general registers, EFLAGS, ESP, FS/TEB state를 명시적으로 저장·복원한다. signal/fault와 nested callback은 별도 후속 단계이며, 이 설계만으로 해결되었다고 주장하지 않는다.

*The Linux x64 backend switches x86 compatibility-mode guest code and 64-bit host/HLE trampolines in the same thread. It maps guest PE and stack in the 32-bit addressable range and explicitly saves/restores x86 general registers, EFLAGS, ESP, and FS/TEB state at every transition. Signals/faults and nested callbacks are separate later stages; this design does not claim to solve them.*

## 전환 계획 / Transition plan

1. 공용 backend capability에 in-process execution과 guest context ownership을 표현한다.
2. Linux x86 in-process backend를 먼저 synthetic PE fixture로 검증한다.
3. Linux x64 trampoline의 entry/return, import gate, EAX/stack cleanup을 fixture로 검증한다.
4. Linux original target의 첫 import completion을 최소 binding과 함께 검증한다.
5. Windows는 existing x86 launcher/runtime를 in-process backend 계약으로 정리하고 WoW64 환경에서 같은 fixture를 검증한다.
6. 현재 i386 helper IPC backend는 각 in-process gate가 확인될 때까지 diagnostic fallback으로 유지한다.

*1. Express in-process execution and guest-context ownership in shared backend capabilities.
2. Validate Linux x86 in-process backend first with a synthetic PE fixture.
3. Validate Linux x64 trampoline entry/return, import gate, EAX, and stack cleanup with fixtures.
4. Validate the first Linux original-target import completion with a minimal binding.
5. On Windows, organize the existing x86 launcher/runtime around the in-process backend contract and validate the same fixtures under WoW64.
6. Retain the current i386 helper IPC backend as diagnostic fallback until each in-process gate is confirmed.*
