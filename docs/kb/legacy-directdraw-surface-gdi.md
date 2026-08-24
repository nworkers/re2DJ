# Legacy DirectDraw surface와 GDI interop

`IDirectDraw4::CreateSurface`는 `DDSURFACEDESC2`로 요청된 pixel surface를 만들고 성공 시 유효한 surface interface pointer를 출력한다. surface는 video memory뿐 아니라 system memory에도 존재할 수 있다. [Microsoft CreateSurface 문서](https://learn.microsoft.com/en-us/previous-versions/ms909037%28v%3Dmsdn.10%29)

`IDirectDrawSurface::GetDC`는 surface를 GDI와 호환되는 device context로 잠그며, 이 DC는 반드시 `ReleaseDC`로 반환해야 한다. Release 전에는 surface가 잠긴 상태라는 것이 계약의 핵심이다. [Microsoft GetDC 문서](https://learn.microsoft.com/en-us/previous-versions/ms785082%28v%3Dvs.85%29), [Microsoft ReleaseDC 문서](https://learn.microsoft.com/en-us/previous-versions/ms785092%28v%3Dvs.85%29)

HLE가 이 경계를 제공할 때는 surface의 논리 pixel storage와 GDI DC의 pixel storage가 동일해야 한다. Windows adapter는 DIB section을 memory DC에 선택해 이를 제공할 수 있으며, 공용 graphics core에는 HDC나 HBITMAP을 노출하지 않는다.

---

# Legacy DirectDraw Surface and GDI Interoperation

`IDirectDraw4::CreateSurface` creates pixel surfaces described by `DDSURFACEDESC2` and must return a valid surface interface pointer on success. A surface may live in video or system memory. [Microsoft CreateSurface documentation](https://learn.microsoft.com/en-us/previous-versions/ms909037%28v%3Dmsdn.10%29)

`IDirectDrawSurface::GetDC` locks a surface behind a GDI-compatible device context, which must be returned through `ReleaseDC`. The surface remains locked until that release. [Microsoft GetDC documentation](https://learn.microsoft.com/en-us/previous-versions/ms785082%28v%3Dvs.85%29), [Microsoft ReleaseDC documentation](https://learn.microsoft.com/en-us/previous-versions/ms785092%28v%3Dvs.85%29)

An HLE boundary must make the logical surface and GDI DC share the same pixels. A Windows adapter can select a DIB section into a memory DC, while common graphics code remains free of HDC and HBITMAP types.
