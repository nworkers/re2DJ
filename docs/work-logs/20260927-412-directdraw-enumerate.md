# 작업 412 작업 로그 — DirectDrawEnumerateA / Task 412 work log — DirectDrawEnumerateA

설계: [20260927-412-directdraw-enumerate.md](../design/20260927-412-directdraw-enumerate.md) · 지시서: [20260927-412-directdraw-enumerate.md](../work-orders/20260927-412-directdraw-enumerate.md)

## 2026-09-27

- 측정: 설계와 같다.
  *Measured as in the design.*
- 결과: Windows x86 CTest 6개 통과(단위 4699 checks), Linux x64·x86 CTest 4개 통과(단위 4696 checks), 실패 0. 긴 실제 실행 회귀는 하지 않았다.
  *Windows x86: all 6 CTest tests pass (4699 unit checks); Linux x64 and x86: all 4 CTest tests pass (4696 unit checks); no failures. No long real-run regression.*
- Linux 1st(x64): `DirectDrawEnumerateA`를 지나 호출 966번째 `ddraw!DirectDrawCreate`에서 멈춘다.
  *Linux 1st (x64) gets past `DirectDrawEnumerateA` and stops at `ddraw!DirectDrawCreate` (call 966).*
- Windows 제품의 1st runtime 기록에서 DX6 facade 호출을 셌다. `DirectDrawCreate` 1, `RootQueryInterface` 2, `IDirectDraw4::SetCooperativeLevel` 1, `SetDisplayMode` 1, `CreateSurface` 176, `CreateTextureSurface` 175, `IDirect3D3::FindDevice` 1, `CreateDevice` 1, `IDirect3DDevice3::DrawPrimitive` 1(기록 표본)이다.
  *In the Windows product's 1st runtime log the DX6 facade calls were `DirectDrawCreate` 1, `RootQueryInterface` 2, `IDirectDraw4::SetCooperativeLevel` 1, `SetDisplayMode` 1, `CreateSurface` 176, `CreateTextureSurface` 175, `IDirect3D3::FindDevice` 1, `CreateDevice` 1, and `IDirect3DDevice3::DrawPrimitive` 1 (a sampled record).*
