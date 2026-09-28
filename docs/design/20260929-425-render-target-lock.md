# 작업 425 설계 — 렌더 타깃 Lock의 되읽기와 올리기 / Task 425 design — reading back and writing the render target on Lock

선행: [작업 424 설계](20260929-424-surface-lock.md)

## 배경 / Background

4th에서 F1(IO 보드 TEST 버튼)을 누르면 테스트 모드로 들어간다. 테스트 모드는 매 프레임 다음을 한다.
1. `Clear`
2. back buffer에 `Lock`/`Unlock` 세 번. 이 사이에 CPU로 글자를 쓴다.
3. `Flip`

작업 424 뒤에는 두 host 모두 화면이 검었다. 게임이 쓴 픽셀은 guest 메모리에 남고, 화면은 render backend의 GL 렌더 타깃에서 나오기 때문이다.

*Pressing F1 (the IO board's TEST button) in 4th enters test mode. Each test-mode frame does the following:*
1. *`Clear`;*
2. *three `Lock`/`Unlock` pairs on the back buffer, writing text with the CPU in between;*
3. *`Flip`.*

*After Task 424 the screen was black on both hosts: the pixels the game writes stay in guest memory, while the picture comes from the render backend's GL render target.*

## 결정 / Decisions

1. **backend의 두 연산.** 공용 `Sdl3OpenGlBackend`에 다음 두 연산을 더한다.
   - `ReadRenderTarget`: 논리 렌더 타깃의 RGB565 픽셀을 사각형 단위로 읽는다(`glReadPixels`).
   - `WriteRenderTarget`: 같은 사각형에 쓴다(렌더 타깃 텍스처에 `glTexSubImage2D`).

   OpenGL의 행은 아래에서 위로, guest의 행은 위에서 아래로 나열된다. 그래서 둘 다 packed scratch buffer를 거쳐 행 순서를 뒤집는다. 쓰기는 먼저 프레임을 시작한다(depth, 그리고 유지 모드가 아니면 색 지우기). 그래서 그 프레임의 첫 Draw가 방금 올린 픽셀을 지우지 않는다. 프레임 시작 처리는 Draw와 같은 `BeginFrame`으로 뽑았다.

   *Two backend operations. The shared `Sdl3OpenGlBackend` gains two operations:*
   - *`ReadRenderTarget` reads the logical render target's RGB565 pixels in a rectangle (`glReadPixels`);*
   - *`WriteRenderTarget` writes the same rectangle (`glTexSubImage2D` into the render target's texture).*

   *OpenGL's rows run bottom-up and the guest's top-down, so both flip the rows through a packed scratch buffer. A write starts the frame first (depth, and colour unless retained), so the frame's first Draw does not clear what it put down; the frame start is factored out as `BeginFrame`, shared with Draw.*
2. **대상 표면.** 두 연산은 DirectDraw 객체가 화면에 내보내는 표면(`presentation_surface`, 곧 렌더 타깃)에만 쓴다.
   - `Lock`: 잠그는 영역을 렌더 타깃에서 guest 픽셀로 되읽는다. 게임은 자기가 그린 그림을 본다.
   - `Unlock`: 그 영역을 렌더 타깃에 다시 올린다. 잠근 영역은 표면에 기록해 둔다.
   - 잠그지 않은 Unlock은 아무것도 올리지 않는다.
   - 다른 표면(텍스처 등)은 전처럼 자기 픽셀만 다룬다.
   - host가 읽거나 쓰지 못하면 `DDERR_GENERIC`이다.

   *The surface it applies to. Both operations apply only to the surface its DirectDraw object presents from (`presentation_surface`, the render target):*
   - *`Lock` reads the locked area back from the render target into guest pixels, so the game sees what it drew;*
   - *`Unlock` puts that area back on the render target; the locked area is recorded on the surface;*
   - *an Unlock without a Lock puts nothing back;*
   - *other surfaces (textures and the like) keep dealing in their own pixels;*
   - *a host that cannot read or write gives `DDERR_GENERIC`.*
3. **두 host가 같은 규칙.**
   - Linux: `HostPresentation`에 `ReadTarget`/`WriteTarget`을 더했다. ddraw 모듈은 guest 메모리를 행 단위로 옮긴다.
   - Windows DX6 facade(DX7 표면도 가져다 쓴다): 표면 픽셀을 그대로 backend에 넘긴다.

   *Both hosts follow the same rule.*
   - *Linux: `HostPresentation` gains `ReadTarget`/`WriteTarget`, and the ddraw module moves guest memory a row at a time.*
   - *Windows DX6 facade (which DX7 surfaces adopt): it hands the surface's pixels to the backend directly.*
