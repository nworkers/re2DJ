# 작업 지시 198: 목적지 색상 마스크 합성
# Work Order 198: Destination-Color Mask Composition

설계: [목적지 색상 블렌드](../design/20260905-198-destination-color-mask-blending.md)

*Design: [Destination-color blending](../design/20260905-198-destination-color-mask-blending.md).*

1. 반복 실패가 뒤 장면의 첫 실패를 숨기지 않도록 진단을 보완합니다.
2. 수정 전 Music Select의 거절된 draw를 확인합니다.
3. DESTCOLOR/INVDESTCOLOR를 공용 상태와 OpenGL에 구현합니다.
4. Win32 build와 기존 unit/CTest를 실행하고 수정 전후 화면과 로그를 비교합니다.
5. 확인됨/추정/미확정을 구분해 분석, KB, 아키텍처, 작업 로그를 갱신하고 커밋합니다.

*Improve first-failure visibility; establish the rejected Music Select draws before the fix; implement DESTCOLOR/INVDESTCOLOR across shared state and OpenGL; build, run existing tests, and compare before/after; update analysis, KB, architecture, and the work log with explicit evidence status, then commit.*
