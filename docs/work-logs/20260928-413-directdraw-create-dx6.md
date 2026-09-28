# 작업 413 작업 로그 — DirectDrawCreate와 DirectX 6 객체 / Task 413 work log — DirectDrawCreate and the DirectX 6 objects

설계: [20260928-413-directdraw-create-dx6.md](../design/20260928-413-directdraw-create-dx6.md) · 지시서: [20260928-413-directdraw-create-dx6.md](../work-orders/20260928-413-directdraw-create-dx6.md)

## 2026-09-28

- Windows 제품의 1st 기록에서 facade가 받은 IID는 `{9C59509A-39BD-11D1-8C4A-00C04FD930C5}`(`IDirectDraw4`)와 `{BB223240-E72B-11D0-A9B4-00AA00C0993E}`(`IDirect3D3`)였다. 두 값과 `IDirectDraw`의 IID를 `abi.h`에 더했다.
  *In the Windows product's 1st log the facade was asked for `IDirectDraw4` and `IDirect3D3`; both IIDs, with `IDirectDraw`'s, went into `abi.h`.*
- ddraw의 COM 메서드 export는 145개에서 185개가 되었다.
  *The ddraw module's COM method exports went from 145 to 185.*
- 결과: Windows x86 CTest 6개 통과(단위 4721 checks), Linux x64·x86 CTest 4개 통과(단위 4718 checks), 실패 0. 긴 실제 실행 회귀는 하지 않았다.
  *Windows x86: all 6 CTest tests pass (4721 unit checks); Linux x64 and x86: all 4 CTest tests pass (4718 unit checks); no failures. No long real-run regression.*
- Linux 1st(x64): `DirectDrawCreate` → `IDirectDraw4::QueryInterface`(`IDirectDraw4`) → 처음 객체의 `Release` 순서로 진행한다. 이어서 호출 969번째 `IDirectDraw4::SetCooperativeLevel(hwnd, 0x811)`에서 멈춘다.
  *Linux 1st (x64) goes `DirectDrawCreate` → `IDirectDraw4::QueryInterface` (`IDirectDraw4`) → `Release` of the first reference, and stops at `IDirectDraw4::SetCooperativeLevel(hwnd, 0x811)` (call 969).*
