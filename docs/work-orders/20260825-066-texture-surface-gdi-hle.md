# texture surface GDI upload HLE 작업 지시

관련 설계: [texture surface GDI upload HLE](../design/20260825-066-texture-surface-gdi-hle.md)

## 상태

**완료**

## 범위

1. 확인된 RGB565 texture CreateSurface 경로를 추가한다.
2. SurfaceFacade에 Windows GDI backing, GetDC/ReleaseDC, source color key를 구현한다.
3. surface 수명과 공유하는 IDirect3DTexture2 QueryInterface를 제공한다.
4. 첫 실행에서 추가 확인된 `DDBLT_COLORFILL` rectangle Blt와 primary/back RGB565 backing을 구현한다.
5. Windows x86 build, CTest와 canonical 2회 실행으로 `0x0042292b` 및 null Blt slot AV 제거와 다음 경계를 확인한다.
6. 누적 분석과 아키텍처, 작업 로그를 갱신한다.

---

# Texture-Surface GDI Upload HLE Work Order

Related design: [Texture-Surface GDI Upload HLE](../design/20260825-066-texture-surface-gdi-hle.md)

## Status and scope

**Complete.** Added the confirmed RGB565 texture CreateSurface path, Windows GDI backing, GetDC/ReleaseDC, source color key, shared-lifetime IDirect3DTexture2 identity, and the subsequently confirmed `DDBLT_COLORFILL` rectangle mode. Builds, tests, and two final runs remove both surface AVs and identify DrawPrimitive as the next boundary.
