# Legacy DirectDraw surface와 GDI interop

`IDirectDraw4::CreateSurface`는 `DDSURFACEDESC2`로 요청된 pixel surface를 만들고 성공 시 유효한 surface interface pointer를 출력한다. surface는 video memory뿐 아니라 system memory에도 존재할 수 있다. [Microsoft CreateSurface 문서](https://learn.microsoft.com/en-us/previous-versions/ms909037%28v%3Dmsdn.10%29)

`IDirectDrawSurface::GetDC`는 surface를 GDI와 호환되는 device context로 잠그며, 이 DC는 반드시 `ReleaseDC`로 반환해야 한다. Release 전에는 surface가 잠긴 상태라는 것이 계약의 핵심이다. [Microsoft GetDC 문서](https://learn.microsoft.com/en-us/previous-versions/ms785082%28v%3Dvs.85%29), [Microsoft ReleaseDC 문서](https://learn.microsoft.com/en-us/previous-versions/ms785092%28v%3Dvs.85%29)

HLE가 이 경계를 제공할 때는 surface의 논리 pixel storage와 GDI DC의 pixel storage가 동일해야 한다. Windows adapter는 DIB section을 memory DC에 선택해 이를 제공할 수 있으며, 공용 graphics core에는 HDC나 HBITMAP을 노출하지 않는다.

## 32bpp DIB section과 GDI batch

`biBitCount` 32, `BI_RGB`인 DIB의 각 픽셀은 DWORD 하나다. 메모리에는 파랑, 초록, 빨강 순으로 놓이고 최상위 바이트는 쓰지 않는다. 따라서 little-endian DWORD로 읽으면 `0x00RRGGBB`다. [Microsoft BITMAPINFOHEADER 문서](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapinfoheader). 음수 `biHeight`는 위에서 아래로 행을 둔다. 32bpp 행은 항상 DWORD 경계에 맞으므로, pitch는 너비 × 4다.

GDI는 그리기 호출을 스레드별 batch에 모았다가 나중에 실행할 수 있다. 그래서 GDI로 DIB section에 그린 뒤 그 비트를 CPU로 직접 읽으려면 먼저 `GdiFlush`를 불러야 한다. [Microsoft CreateDIBSection 문서](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-createdibsection), [Microsoft GdiFlush 문서](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-gdiflush)

re2DJ의 32비트 색 모드는 이 두 성질을 쓴다([작업 429 설계](../design/20260929-429-true-color-surfaces.md)). Windows `GetDC`는 표면의 32bpp plane DIB를 DC에 선택한다. `ReleaseDC`는 `GdiFlush` 뒤에 plane을 RGB565로 줄인다. Windows GDI가 채널을 좁힐 때 하위 비트를 버리므로(Linux `gdi_raster.h`의 측정값), 32bpp로 그린 뒤 버림으로 줄인 결과는 GDI가 RGB565 DIB에 직접 그린 결과와 같다(**추정**: 채널 변환 측정에 근거함. 글꼴 안티앨리어싱처럼 깊이에 따라 래스터화가 달라지는 경우는 측정하지 않았다).

---

# Legacy DirectDraw Surface and GDI Interoperation

`IDirectDraw4::CreateSurface` creates pixel surfaces described by `DDSURFACEDESC2` and must return a valid surface interface pointer on success. A surface may live in video or system memory. [Microsoft CreateSurface documentation](https://learn.microsoft.com/en-us/previous-versions/ms909037%28v%3Dmsdn.10%29)

`IDirectDrawSurface::GetDC` locks a surface behind a GDI-compatible device context, which must be returned through `ReleaseDC`. The surface remains locked until that release. [Microsoft GetDC documentation](https://learn.microsoft.com/en-us/previous-versions/ms785082%28v%3Dvs.85%29), [Microsoft ReleaseDC documentation](https://learn.microsoft.com/en-us/previous-versions/ms785092%28v%3Dvs.85%29)

An HLE boundary must make the logical surface and GDI DC share the same pixels. A Windows adapter can select a DIB section into a memory DC, while common graphics code remains free of HDC and HBITMAP types.

## 32bpp DIB sections and GDI batching

In a DIB with `biBitCount` 32 and `BI_RGB`, each pixel is one DWORD holding blue, green, and red in memory order, with the high byte unused. Read as a little-endian DWORD it is `0x00RRGGBB`. [Microsoft BITMAPINFOHEADER documentation](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapinfoheader). A negative `biHeight` lays rows out top-down, and 32bpp rows are always DWORD-aligned, so the pitch is the width times four.

GDI may collect drawing calls in a per-thread batch and run them later, so code that reads a DIB section's bits directly after drawing into it through GDI must call `GdiFlush` first. [Microsoft CreateDIBSection documentation](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-createdibsection), [Microsoft GdiFlush documentation](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-gdiflush)

re2DJ's 32-bit colour mode relies on both ([Task 429 design](../design/20260929-429-true-color-surfaces.md)). Windows `GetDC` selects the surface's 32bpp plane DIB into the DC, and `ReleaseDC` narrows the plane into RGB565 after `GdiFlush`. Because Windows GDI narrows a channel by dropping low bits (measured, Linux `gdi_raster.h`), drawing at 32bpp and narrowing by truncation gives what GDI would have drawn into an RGB565 DIB directly (**inferred** from the channel-conversion measurements; rasterization that depends on depth, such as font antialiasing, was not measured).
