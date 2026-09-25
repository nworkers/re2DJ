# 작업 377 작업 지시서 — DirectX core 2단계: 협조 수준과 표시 모드 / Task 377 work order — DirectX core phase 2: cooperative level and display mode

설계: [20260926-377-directx-cooperative-level-and-mode.md](../design/20260926-377-directx-cooperative-level-and-mode.md)

## 절차 / Steps

1. core에 `directdraw_display.h/.cpp`를 두고, ABI 상수를 더한다.
   *Add `directdraw_display.h/.cpp` to the core, with the ABI constants.*
2. Windows `RootFacade`를 `DirectDrawDisplay`로 옮기고, `SetCooperativeLevel`/`SetDisplayMode`/`IDirectDraw7::GetDisplayMode`가 core를 쓰게 한다.
   *Move Windows `RootFacade` onto `DirectDrawDisplay`, with `SetCooperativeLevel`/`SetDisplayMode`/`IDirectDraw7::GetDisplayMode` on the core.*
3. Linux `GuestComObject` 상태 slot과 DirectDraw 상태, 두 메서드, `SetRect`를 추가한다.
   *Add the Linux `GuestComObject` state slot, the DirectDraw state, the two methods, and `SetRect`.*
4. 변경 전 build를 worktree에서 만들고 Windows 실제 4th를 전후 비교한다. 단위 테스트, 문서.
   *Build the pre-change commit in a worktree and compare the real 4th on Windows before and after; unit tests and documentation.*

## 완료 조건 / Done when

- Windows x86과 Linux 두 폭이 build·CTest를 통과하고, Windows 실제 4th의 기록이 변경 전과 같다.
  *Windows x86 and both Linux widths build and pass CTest, and the real 4th's record on Windows matches the pre-change build.*
- Linux 실제 4th가 두 폭에서 `SetDisplayMode`를 지나 `CreateSurface`까지 간다.
  *On both Linux widths the real 4th passes `SetDisplayMode` and reaches `CreateSurface`.*
