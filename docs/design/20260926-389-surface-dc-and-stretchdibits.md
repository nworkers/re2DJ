# 작업 389 설계 — 표면 DC와 StretchDIBits / Task 389 design — surface DCs and StretchDIBits

선행: [작업 381 설계](20260926-381-directx-surfaces.md), [작업 388 설계](20260926-388-input-queries.md)

## 배경 / Background

작업 388 뒤 Linux 실행은 텍스처 표면의 `IDirectDrawSurface7::GetDC`에서 멈췄다. 4th는 텍스처를 다음 순서로 올린다.

1. `System\warning.abm`을 읽는다. 16bit BI_RGB(5-5-5) 1024×512 BMP다.
2. 1024×512 RGB565 텍스처를 만든다.
3. `GetDC`로 DC를 받는다.
4. `StretchDIBits(SRCCOPY, DIB_RGB_COLORS)`로 BMP를 그린다.
5. `ReleaseDC`, `SetColorKey(DDCKEY_SRCBLT)`를 부른다.

*After Task 388 a Linux run stopped at `IDirectDrawSurface7::GetDC` on a texture surface. The 4th uploads a texture in this order:*

1. *It reads `System\warning.abm`, a 16-bit BI_RGB (5-5-5) 1024×512 BMP.*
2. *It creates a 1024×512 RGB565 texture.*
3. *It takes a DC with `GetDC`.*
4. *It draws the BMP with `StretchDIBits(SRCCOPY, DIB_RGB_COLORS)`.*
5. *It calls `ReleaseDC` and `SetColorKey(DDCKEY_SRCBLT)`.*

Windows facade는 표면마다 RGB565 DIB section이 선택된 memory DC를 두고, GDI 호출은 실제 gdi32가 처리한다. 규칙은 다음과 같다.

- 픽셀이 없는 표면의 `GetDC`는 `DDERR_UNSUPPORTED`다.
- guest가 이미 DC를 갖고 있으면 `DDERR_DCALREADYCREATED`다.
- `ReleaseDC`는 준 DC만, 쥔 동안만 받는다. 아니면 `DDERR_INVALIDPARAMS`다.
- `SetColorKey`는 `DDCKEY_SRCBLT`와 key가 있을 때만 받는다.

*The Windows facade keeps a memory DC per surface with an RGB565 DIB section selected, and the real gdi32 handles the GDI calls. The rules:*

- *`GetDC` on a surface without pixels is `DDERR_UNSUPPORTED`.*
- *If the guest already holds the DC, it is `DDERR_DCALREADYCREATED`.*
- *`ReleaseDC` takes back only the DC it gave, while held, else `DDERR_INVALIDPARAMS`.*
- *`SetColorKey` takes only `DDCKEY_SRCBLT` with a key.*

Windows 11 GDI가 RGB565 DIB section에 `StretchDIBits`한 결과를 측정했다.

- **채널 변환**: 넓힐 때는 상위 비트를 반복하고(5bit 초록 → 6bit는 `(g<<1)|(g>>4)`), 좁힐 때는 하위 비트를 버린다(24bit → 565).
- **원본 행**: bottom-up DIB의 `ySrc`는 아래에서부터 센다.
- **확대**: 가장 가까운 원본 픽셀을 반복한다.
- **대상 밖**: bitmap 밖의 대상 픽셀은 잘린다.
- **반환값**: `ySrc + SrcHeight`다. (0,2)→2, (0,1)→1, (1,1)→2로 측정했다. last error는 그대로다.

*Windows 11 GDI's `StretchDIBits` into an RGB565 DIB section was measured:*

- ***Channel conversion:** widening repeats the high bits (5-bit green to 6-bit is `(g<<1)|(g>>4)`), and narrowing drops the low bits (24-bit to 565).*
- ***Source rows:** `ySrc` of a bottom-up DIB counts from its bottom.*
- ***Stretching:** repeats the nearest source pixel.*
- ***Outside the destination:** pixels outside the bitmap are clipped.*
- ***Result:** `ySrc + SrcHeight`, measured as (0,2)→2, (0,1)→1, and (1,1)→2, with the last error untouched.*

## 결정 / Decisions

1. **core (`directdraw_surface.h`).** `CheckGetDc`, `CheckReleaseDc`, `CheckSetColorKey`, 그리고 `DDERR_DCALREADYCREATED`와 `DDCKEY_SRCBLT` 상수를 둔다. Windows facade도 이 규칙을 쓴다.
   ***Core (`directdraw_surface.h`):** `CheckGetDc`, `CheckReleaseDc`, and `CheckSetColorKey`, with the `DDERR_DCALREADYCREATED` and `DDCKEY_SRCBLT` constants. The Windows facade uses these rules too.*
2. **GDI 모델(`guest_gdi.h`, `gdi_raster.h`).**
   - `GuestGdi`는 DC와 bitmap을 갖는다. bitmap의 픽셀은 guest 메모리에 있다.
   - GDI handle은 `0x0A000010`부터 할당한다. USER handle(`0x00010010`부터)과 stock object(`0x0088xxxx`~`0x028Axxxx`) 범위를 피한 값이다.
   - `gdi_raster`에는 측정한 채널·픽셀 변환, 확대 매핑, DIB 행 크기가 있다.
   - Windows는 계속 실제 GDI를 쓰므로, 이 모델은 HLE 쪽에 둔다.

   ***GDI model (`guest_gdi.h`, `gdi_raster.h`):***
   - *`GuestGdi` holds DCs and bitmaps, with bitmap pixels in guest memory.*
   - *GDI handles are allocated from `0x0A000010`, away from USER handles (from `0x00010010`) and the stock objects (`0x0088xxxx` to `0x028Axxxx`).*
   - *`gdi_raster` holds the measured channel and pixel conversions, the stretch mapping, and DIB row sizes.*
   - *Windows keeps using the real GDI, so this model lives on the HLE side.*
3. **Linux 표면.**
   - `GetDC`는 처음 불릴 때 표면의 RGB565 픽셀을 가리키는 bitmap과 그것이 선택된 DC를 만들고, 표면이 사라질 때까지 재사용한다. 표면이 사라질 때 DC와 bitmap도 지운다.
   - `ReleaseDC`와 `SetColorKey`는 core 규칙을 따른다. color key는 그리기 단계를 위해 저장한다.

   ***Linux surfaces:***
   - *`GetDC` makes, at its first call, a bitmap over the surface's RGB565 pixels and a DC with it selected, reused until the surface goes; the DC and bitmap go with the surface.*
   - *`ReleaseDC` and `SetColorKey` follow the core rules, and the color key is kept for drawing.*
4. **`gdi32!StretchDIBits`.** 측정한 형태만 구현한다. SRCCOPY, DIB_RGB_COLORS, 16bit(5-5-5 또는 bitfields)와 24bit 원본, 양수 크기, 원본 안의 사각형, top-down 원본은 `ySrc` 0이다. 이 밖의 형태는 불리면 멈춘다.
   ***`gdi32!StretchDIBits`** implements only the measured shapes: SRCCOPY, DIB_RGB_COLORS, 16-bit (5-5-5 or bitfields) and 24-bit sources, positive extents, a rectangle inside the source, and `ySrc` 0 for a top-down source. Any other shape stops.*

## 범위 밖 / Out of scope

- 다른 GDI 함수(`CreateCompatibleDC`, `CreateDIBSection`, `BitBlt`, 글자 그리기): 게임이 도달하면 다룬다. / *Other GDI functions (`CreateCompatibleDC`, `CreateDIBSection`, `BitBlt`, text): taken up when the game reaches them.*
- `user32!GetCursorPos`부터 시작하는 메인 루프 입력: 다음 작업. / *Main-loop input from `user32!GetCursorPos`: the next task.*
