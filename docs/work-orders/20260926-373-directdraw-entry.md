# 작업 373 작업 지시서 — DirectDraw 진입 / Task 373 work order — DirectDraw entry

설계: [20260926-373-directdraw-entry.md](../design/20260926-373-directdraw-entry.md)

## 절차 / Steps

1. `DirectDrawEnumerateExA`를 32비트 PowerShell로 측정한다(flags 0·7, callback이 FALSE를 돌려줄 때).
   *Measure `DirectDrawEnumerateExA` from 32-bit PowerShell (flags 0 and 7, and a callback answering FALSE).*
2. `GuestComObjects`를 추가하고 `GuestProcess`에 둔다. `GuestUser::PrimaryMonitor()`를 추가한다.
   *Add `GuestComObjects` to `GuestProcess`, and `GuestUser::PrimaryMonitor()`.*
3. `ddraw_module`에 `DirectDrawEnumerateExA`, `DirectDrawCreateEx`, `IDirectDraw7` 메서드 export를 둔다. Linux 등록을 고친다.
   *Add `ddraw_module` with `DirectDrawEnumerateExA`, `DirectDrawCreateEx`, and the `IDirectDraw7` method exports, and register it on Linux.*
4. 단위 테스트, 문서 갱신.
   *Unit tests and documentation.*

## 완료 조건 / Done when

- Linux 두 폭과 Windows x86 build·CTest가 통과하고, 기존 진단·probe가 그대로다.
  *Both Linux widths and Windows x86 build and pass CTest, with existing diagnostics and probes unchanged.*
- 실제 4th가 두 폭에서 `IDirectDraw7`을 만들고 `QueryInterface(IID_IDirect3D7)`까지 간다.
  *On both widths the real 4th creates its `IDirectDraw7` and reaches `QueryInterface(IID_IDirect3D7)`.*
