# 작업 320: Import event loop / Task 320: Import event loop

설계: [Import event loop](../design/20260918-320-import-event-loop.md)

*Design: [Import event loop](../design/20260918-320-import-event-loop.md)*

## 작업 / Work

1. 공용 HLE에 import event loop와 terminal result를 추가합니다.
2. loaded gate 검증, dispatcher failure terminal stop, import budget을 구현합니다.
3. fake backend unit test로 두 import의 연속 completion, 미등록 import stop, terminal event를 검증합니다.
4. architecture와 L2 계획 및 작업 로그를 갱신합니다.

*1. Add an import event loop and terminal result to shared HLE.
2. Implement loaded-gate validation, terminal stop on dispatcher failure, and an import budget.
3. Use a fake-backend unit test to verify two consecutive completions, unregistered-import stop, and a terminal event.
4. Update architecture, the L2 plan, and the work log.*
