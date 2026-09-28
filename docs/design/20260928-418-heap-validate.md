# 작업 418 설계 — HeapValidate / Task 418 design — HeapValidate

선행: [작업 417 설계](20260928-417-guest-threads.md)

## 배경 / Background

작업 417 뒤 Linux의 EZ2DJ 1st는 메인 스레드의 `kernel32!HeapValidate`에서 멈췄다. 1st에 link된 디버그 CRT는 블록을 다룰 때 `IsBadReadPtr`/`IsBadWritePtr`에 이어 `HeapValidate`로 블록을 검사한다.

*After Task 417, EZ2DJ 1st on Linux stopped at the main thread's `kernel32!HeapValidate`. The debug CRT linked into 1st checks a block with `IsBadReadPtr`/`IsBadWritePtr` followed by `HeapValidate`.*

## Windows 측정 / Windows measurements

Windows 11에서 32비트 probe로 측정했다(`scratchpad/heap418`).

- 결과 1:
  - `lpMem`이 NULL일 때(힙 전체).
  - 그 힙의 살아 있는 블록 시작 주소일 때.
  - `HeapCreate` 힙과 프로세스 힙 모두 같다.
- 결과 0:
  - 블록 안쪽 주소, 해제된 블록, 다른 힙의 블록.
  - 스택 주소.
- 플래그(`HEAP_NO_SERIALIZE`, 0x4, 0x10000)는 결과를 바꾸지 않는다.
- last error는 어느 경우에도 그대로다.
- 힙이 아닌 핸들(NULL, 0x1234, 이미 해제한 힙)은 access violation을 낸다.

*Measured with a 32-bit probe on Windows 11 (`scratchpad/heap418`):*
- *1 for a NULL `lpMem` (the whole heap) or the start of a live block of that heap, for a `HeapCreate` heap and the process heap alike.*
- *0 for an address inside a block, a freed block, another heap's block, or a stack address.*
- *The flags (`HEAP_NO_SERIALIZE`, 0x4, 0x10000) do not change the result.*
- *The last error stays in every case.*
- *A handle that is not a heap (NULL, 0x1234, a destroyed heap) raises an access violation.*

## 결정 / Decisions

1. kernel32 `HeapValidate`는 측정대로 답한다. 블록 판정은 `GuestHeap::BlockSize`(살아 있는 블록의 시작 주소만)로 한다. 모델의 힙은 guest가 쓰는 블록 경계를 따로 보관하므로 늘 일관된다. 그래서 힙 전체 검사(NULL)는 1이다.
   *kernel32 `HeapValidate` answers as measured. The block check uses `GuestHeap::BlockSize` (live block starts only). The model's heap keeps its block bounds apart from what the guest writes, so it is always consistent and a whole-heap check (NULL) gives 1.*
2. 힙이 아닌 핸들은 Windows에서 예외가 나는 경우라 모델하지 않고 멈춘다.
   *A handle that names no heap raises an exception on Windows; that is not modelled and stops.*
