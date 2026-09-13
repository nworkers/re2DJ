# 작업 지시서: EZ2Dancer coin counter 입력 수정
# Work Order: Fix EZ2Dancer Coin Counter Input

## 한국어

설계 문서 [20260913-268](../design/20260913-268-ez2dancer-coin-counter.md)에 따라
사용자의 실제 실행에서 동작하지 않은 `ez2d2m` coin compatibility path를 수정합니다.

- [x] `0x304` coin level mapping을 rising-edge counter로 변경합니다.
- [x] board 단위 시험에서 최초 press, hold, release, 재입력, wrap 동작을 확인합니다.
- [x] runtime I/O 설정 초기화 결과를 VFS 진단 로그에 한 번 기록합니다.
- [x] I/O trace budget 이후에도 `0x304` 값 변화는 별도 coin 진단 로그로 기록합니다.
- [x] 설정 예제와 README, ARCHITECTURE의 동작 설명을 counter semantics에 맞게 갱신합니다.
- [x] `docs/analysis/ez2dancer-io-map.md`의 확인 상태와 이전 추정 내용을 갱신합니다.
- [x] Windows x86 Debug 빌드와 관련 CTest를 실행합니다.
- [x] 작업 로그를 작성하고 변경을 커밋합니다.

원본 I/O 카드의 register 의미는 실행 검증 전까지 **미확정**으로 유지합니다.

## English

Following [design 20260913-268](../design/20260913-268-ez2dancer-coin-counter.md), fix
the `ez2d2m` coin compatibility path that did not work in the user's real run.

- [x] Replace the `0x304` held-level mapping with a rising-edge counter.
- [x] Cover first press, hold, release, re-press, and wrap behaviour in board tests.
- [x] Record the runtime I/O configuration initialization result once in the VFS log.
- [x] Record `0x304` value changes in a separate coin diagnostic event after the I/O trace budget is exhausted.
- [x] Update the example configuration, README, and ARCHITECTURE to describe counter
  semantics.
- [x] Update the confirmation status and previous inference in
  `docs/analysis/ez2dancer-io-map.md`.
- [x] Run the Windows x86 Debug build and related CTest tests.
- [x] Write the work log and commit the change.

The original I/O card register meaning remains **unresolved** until runtime validation.
