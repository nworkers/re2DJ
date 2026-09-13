# 작업 지시 / Work order

설계 문서 [20260914-284](../design/20260914-284-streaming-refill-order.md)에 따라
소비량 기반 무조건 보충을 제거하고 committed ring snapshot 비교를 복원합니다.
재현 테스트로 1~2초 반복이 사라지는지와 변경 PCM이 출력되는지 확인한 뒤 빌드와
관련 테스트를 수행합니다.

Following design [20260914-284](../design/20260914-284-streaming-refill-order.md), remove
unconditional consumption-based refills and restore committed ring-snapshot comparison.
Use a regression test to prove that the 1–2 second repetition is gone and changed PCM still
reaches output, then build and run relevant tests.
