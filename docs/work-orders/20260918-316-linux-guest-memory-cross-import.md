# 작업 316 — Linux guest memory cross-import persistence / Task 316 — Linux guest memory cross-import persistence

설계: [Linux guest memory cross-import persistence](../design/20260918-316-linux-guest-memory-cross-import.md).

*Design: [Linux guest memory cross-import persistence](../design/20260918-316-linux-guest-memory-cross-import.md).*

## 작업 / Work

1. Linux 전용 native IPC probe가 첫 import에서 allocation을 유지하도록 수정합니다.
2. 두 번째 import에서 저장된 base의 pattern을 읽고 exact-base free 및 free 뒤 read 거부를 검사합니다.
3. Linux x64/x86 검증과 작업 로그를 갱신합니다.

*1. Change the Linux-only native IPC probe so the first import retains its allocation.
2. At the second import, read the saved base's pattern, then verify exact-base free and rejected read after free.
3. Revalidate Linux x64/x86 and update the work log.*

## 완료 기준 / Completion criteria

첫 import completion 뒤 두 번째 import에서 같은 32비트 guest address의 pattern을 읽을 수 있어야 합니다. 두 번째 import에서 free 뒤 read는 오류 packet으로 실패해야 하며, 기존 dispatcher, fault, stop fixture 결과는 유지되어야 합니다.

*After the first import completes, the second import must read the pattern at the same 32-bit guest address. Read after free at the second import must fail through an error packet, while existing dispatcher, fault, and stop fixture outcomes remain valid.*
