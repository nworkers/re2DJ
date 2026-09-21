# 작업 지시 337: Linux x86 fault provenance window / Work order 337: Linux x86 fault provenance window

`0x00af0c22` null read의 EAX producer를 확인할 수 있도록 Linux x86 fault instruction window를 80 bytes로 확장합니다. 실행 중 live bytes를 bounded disassembly하고, 확인된 control-flow와 EAX write만 analysis에 기록합니다.

*Expand the Linux x86 fault instruction window to 80 bytes so the EAX producer for the `0x00af0c22` null read can be investigated. Disassemble bounded live bytes and record only confirmed control flow and EAX writes in analysis.*

완료 기준은 다음과 같습니다.

*The completion criteria are:*

1. window가 EIP 전 64 bytes와 후 16 bytes를 보존하고 EIP offset을 출력합니다.
   *The window preserves 64 bytes before EIP and 16 after it, and prints the EIP offset.*
2. synthetic `UD2` probe가 새 offset과 byte 위치를 검증합니다.
   *The synthetic `UD2` probe verifies the new offset and byte position.*
3. 실제 4th CHD window를 bounded disassembly해 EAX producer의 확인 상태를 기록합니다.
   *The real 4th CHD window is bounded-disassembled and the confirmation status of the EAX producer is recorded.*
4. Linux x86/x64 build, work log, Git commit을 남깁니다.
   *Leave Linux x86/x64 builds, a work log, and a Git commit.*