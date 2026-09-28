# 작업 414 설계 — IDirectDraw4의 협력 수준과 화면 모드 / Task 414 design — IDirectDraw4's cooperative level and display mode

선행: [작업 413 설계](20260928-413-directdraw-create-dx6.md)

## 배경 / Background

작업 413 뒤 Linux의 EZ2DJ 1st는 `IDirectDraw4::SetCooperativeLevel(hwnd, 0x811)`에서 멈췄다. Windows 제품의 DX6 facade(`RootSetCooperativeLevel`, `RootSetDisplayMode`)는 DX7 facade와 같은 공용 core(`directx::SetCooperativeLevel`, `directx::SetDisplayMode`)를 쓴다. 호스트 정책은 제품의 창 모드다.

*After Task 413, EZ2DJ 1st on Linux stopped at `IDirectDraw4::SetCooperativeLevel(hwnd, 0x811)`. The Windows product's DX6 facade (`RootSetCooperativeLevel`, `RootSetDisplayMode`) uses the same shared cores as the DX7 facade (`directx::SetCooperativeLevel`, `directx::SetDisplayMode`), with the product's window mode as the host policy.*

## 결정 / Decisions

1. Linux의 `IDirectDraw7::SetCooperativeLevel`·`SetDisplayMode` 본문을 객체 종류를 인자로 받는 `SetCooperativeLevelOf`·`SetDisplayModeOf`로 떼어 낸다. `IDirectDraw4`도 이 함수를 쓴다. host 정책은 그대로 창을 guest 크기로 보이는 것이다.
   *The bodies of Linux's `IDirectDraw7::SetCooperativeLevel` and `SetDisplayMode` move into `SetCooperativeLevelOf` and `SetDisplayModeOf`, which take the object kind; `IDirectDraw4` uses them too, with the same host policy of showing the window at the guest's size.*
2. 1st는 아직 `SetDisplayMode`를 부르지 않는다. 같은 공용 core이므로 함께 연결하고 단위 테스트로 확인한다.
   *1st does not call `SetDisplayMode` yet; it shares the core, so it is connected now and covered by a unit test.*
