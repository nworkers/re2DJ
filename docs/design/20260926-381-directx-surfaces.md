# 작업 381 설계 — DirectX core 3단계: 표면 / Task 381 design — DirectX core phase 3: surfaces

선행: [작업 374 설계](20260926-374-shared-directx-core.md), [작업 377 설계](20260926-377-directx-cooperative-level-and-mode.md)

## 배경 / Background

작업 380 뒤 Linux 실행은 호스트 창을 띄운 다음 `IDirectDraw7::CreateSurface`에서 멈췄다. Windows facade의 `RootCreateSurface`는 요청을 다음처럼 나눈다.

- `ZBUFFER`는 depth 표면이다. 크기를 주지 않으면 표시 크기를 쓴다.
- `TEXTURE`는 caps·폭·높이·픽셀 형식이 모두 있고 RGB565여야 한다. 아니면 `DDERR_INVALIDPIXELFORMAT`다.
- `OFFSCREENPLAIN`은 caps·폭·높이가 있어야 하고, 형식을 주면 RGB565여야 한다.
- back buffer가 0개나 1개인 `PRIMARYSURFACE`는 표시 크기의 주 표면이다. back buffer가 있으면 flip 체인이 된다.
- 나머지는 `DDERR_UNSUPPORTED`다.

pitch는 폭×2를 4바이트로 올린 값이다. `AddAttachedSurface`는 depth 표면만 받는다. `GetAttachedSurface`는 back buffer나 depth 표면을 찾는다. `GetSurfaceDesc`는 RGB565 설명을 준다.

*After Task 380 a Linux run opened its host window and stopped at `IDirectDraw7::CreateSurface`. The Windows facade's `RootCreateSurface` sorts requests as follows.*

- *`ZBUFFER` is a depth surface, display-sized unless a size is given.*
- *`TEXTURE` needs caps, width, height, and pixel format, in RGB565, else `DDERR_INVALIDPIXELFORMAT`.*
- *`OFFSCREENPLAIN` needs caps, width, and height, in RGB565 if a format is given.*
- *A `PRIMARYSURFACE` with zero or one back buffer is a display-sized primary, a flip chain when it has a back buffer.*
- *Anything else is `DDERR_UNSUPPORTED`.*

*The pitch is width×2 rounded up to 4 bytes. `AddAttachedSurface` takes only depth surfaces, `GetAttachedSurface` finds the back buffer or the depth surface, and `GetSurfaceDesc` gives an RGB565 description.*

## 결정 / Decisions

1. **core (`directdraw_surface.h`).** `PlanCreateSurface(request, display)`가 위 규칙으로 `SurfacePlan`을 만든다. plan에는 결과, 표면 모양(`SurfaceShape`: 종류·크기·caps), back buffer 모양, 그리고 flip 표시 여부(`retains_frames`)가 들어 있다. 거절한 요청도 `retains_frames`는 알려 준다. Windows facade는 거절 전에도 표시 방식을 기록하기 때문이다. `Rgb565Pitch`, `SurfaceDescription`, `CheckAttachment`, `QueryAttachment`도 core에 둔다. 표면 메모리는 core 밖에 둔다. Windows는 GDI DIB를 쓰고, Linux는 guest 메모리를 쓴다.
   ***Core (`directdraw_surface.h`):** `PlanCreateSurface(request, display)` turns a request into a `SurfacePlan` by the rules above: the result, the surface's shape (`SurfaceShape`: kind, size, caps), the back buffer's shape, and whether the guest presents by flipping (`retains_frames`), which a refused request still reports because the Windows facade records the presentation style before refusing. `Rgb565Pitch`, `SurfaceDescription`, `CheckAttachment`, and `QueryAttachment` are in the core too. Surface memory stays outside it: GDI DIBs on Windows, guest memory on Linux.*
2. **Windows.** `RootCreateSurface`는 plan을 따르고, 픽셀이 있는 표면에만 GDI backing을 만든다. `AddAttachedSurface`, `GetAttachedSurface`, `GetSurfaceDesc`, RGB565 형식, pitch는 core를 쓴다. 새 HRESULT와 caps 상수는 SDK 값과 `static_assert`로 비교한다.
   ***Windows:** `RootCreateSurface` follows the plan and makes GDI backing only for surfaces with pixels; `AddAttachedSurface`, `GetAttachedSurface`, `GetSurfaceDesc`, the RGB565 format, and the pitch use the core. The new HRESULT and caps constants are checked against the SDK with `static_assert`.*
3. **Linux (`ddraw_surface7.cpp`).** `IDirectDrawSurface7` 객체의 상태(`SurfaceState`)는 모양, pitch, 픽셀, 붙은 표면을 담는다. 픽셀은 guest `VirtualAlloc` 메모리에 두고 0으로 채운다. 이후 guest의 `Lock`과 DC가 그 메모리를 직접 쓰게 하려는 것이다. 표면은 DirectDraw 객체를 parent로 참조한다. flip 주 표면은 back buffer의 첫 참조를 갖는다.
   ***Linux (`ddraw_surface7.cpp`):** an `IDirectDrawSurface7` object's state (`SurfaceState`) holds its shape, pitch, pixels, and attached surfaces. The pixels are zero-filled guest `VirtualAlloc` memory, so the guest's later `Lock` and DCs can use it directly. Each surface references its DirectDraw object as parent, and a flipping primary holds its back buffer's first reference.*
4. **`GuestComState` 수명 hook.** `HeldReferences()`는 이 객체가 참조하는 다른 facade 객체를 돌려준다. `ReleaseResources(process)`는 guest 자원을 돌려준다. `Release`가 0이 되면 순서는 다음과 같다.
   1. block을 해제한다.
   2. 자원을 돌려준다.
   3. 잡고 있던 참조를 놓는다.
   4. 마지막으로 parent를 놓는다.

   ***`GuestComState` lifetime hooks:** `HeldReferences()` names the other facade objects an object references, and `ReleaseResources(process)` returns what it owns in the guest. When `Release` reaches zero, it frees the block, returns the resources, drops the held references, and last the parent.*
5. **Linux 메서드 범위.** 이번에 구현하는 메서드는 `QueryInterface`(IUnknown·자기 자신만), `AddRef`, `Release`, `AddAttachedSurface`(다른 DirectDraw의 표면이면 `DDERR_INVALIDOBJECT`, depth 표면은 교체), `GetAttachedSurface`, `GetCaps`, `GetPixelFormat`, `GetSurfaceDesc`(둘 다 `dwSize` 검사), `IsLost`다. 나머지 49개 slot 중 남은 메서드는 불리면 멈춘다.
   ***Linux method scope:** `QueryInterface` (IUnknown and itself only), `AddRef`, `Release`, `AddAttachedSurface` (`DDERR_INVALIDOBJECT` for another DirectDraw's surface; a depth surface is replaced), `GetAttachedSurface`, `GetCaps`, `GetPixelFormat` and `GetSurfaceDesc` (both checking `dwSize`), and `IsLost`. The rest of the 49 slots stop when called.*

## 범위 밖 / Out of scope

- `Lock`/`Unlock`, `GetDC`/`ReleaseDC`와 Linux GDI DC, `Blt`, `Flip`, texture 올리기. 게임이 도달하면 그때 다룬다. / *`Lock`/`Unlock`, `GetDC`/`ReleaseDC` with a Linux GDI DC, `Blt`, `Flip`, and texture uploads, taken up when the game reaches them.*
- 4단계, 장치(`IDirect3D7::CreateDevice`)와 장치 상태. / *Phase 4, the device (`IDirect3D7::CreateDevice`) and its state.*
