# 작업 414 작업 로그 — IDirectDraw4의 협력 수준과 화면 모드 / Task 414 work log — IDirectDraw4's cooperative level and display mode

설계: [20260928-414-dx6-cooperative-level.md](../design/20260928-414-dx6-cooperative-level.md) · 지시서: [20260928-414-dx6-cooperative-level.md](../work-orders/20260928-414-dx6-cooperative-level.md)

## 2026-09-28

- 결과: Windows x86 CTest 6개 통과(단위 4727 checks), Linux x64·x86 CTest 4개 통과(단위 4724 checks), 실패 0. 긴 실제 실행 회귀는 하지 않았다. 4th가 쓰는 DX7 경로는 같은 본문을 부르고, 기존 단위 테스트가 그대로 통과한다.
  *Windows x86: all 6 CTest tests pass (4727 unit checks); Linux x64 and x86: all 4 CTest tests pass (4724 unit checks); no failures. No long real-run regression; 4th's DX7 path calls the same bodies and its existing unit tests still pass.*
- Linux 1st(x64): `SetCooperativeLevel`이 성공해 host 창이 열린다. `QueryInterface(IDirect3D3)` 뒤 호출 971번째 `IDirect3D3::FindDevice`에서 멈춘다.
  *Linux 1st (x64): `SetCooperativeLevel` succeeds and opens the host window; after `QueryInterface(IDirect3D3)` it stops at `IDirect3D3::FindDevice` (call 971).*
