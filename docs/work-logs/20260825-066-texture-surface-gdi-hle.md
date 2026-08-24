# texture surface GDI upload HLE 작업 로그

관련 설계: [texture surface GDI upload HLE](../design/20260825-066-texture-surface-gdi-hle.md)

관련 작업 지시: [texture surface GDI upload HLE 작업 지시](../work-orders/20260825-066-texture-surface-gdi-hle.md)

## 결과

`0x0042292b` AV를 `DDSCAPS_TEXTURE` surface의 `GetDC` 호출로 귀속했다. Windows x86 facade에 다음 확인 계약을 구현했다.

- RGB565 primary/back/texture CPU-visible DIB backing과 DWORD-aligned pitch
- texture `CreateSurface`, GDI `GetDC`/`ReleaseDC`
- `DDCKEY_SRCBLT` color key 저장
- surface와 수명을 공유하는 `IDirect3DTexture2` identity
- 첫 구현 실행에서 추가 확인된 `DDBLT_COLORFILL` rectangle fill
- surface release 시 HDC/HBITMAP 정리

Windows x86 Debug build와 CTest 2/2가 통과했다. 중간 실행 `20260825-020704-982.jsonl`은 기존 null surface AV가 제거되고 null Blt slot return `0x0042333e`로 진행했음을 확인했다. color-fill 구현 뒤 최종 로그는 다음 두 개다.

- `20260825-020941-109.jsonl`
- `20260825-021024-959.jsonl`

두 실행 모두 기존 `0x0042292b` read AV와 Blt execute AV를 제거했다. 새 최초 경계는 execute address 0, stack return `0x0042325f`로 동일하다. 정적 call site `0x0042325c`는 `IDirect3DDevice3` vtable `+0x70`의 `DrawPrimitive`이며 인자는 primitive type 5, vertex type `0x1c4`, count 4, vertex array `0x0045d528`, flags 0이다. 이 draw를 성공으로 가장하지 않고 다음 OpenGL HLE 작업으로 남겼다. 원본 자산은 변경하지 않았다.

---

# Texture-Surface GDI Upload HLE Work Log

Related design: [Texture-Surface GDI Upload HLE](../design/20260825-066-texture-surface-gdi-hle.md)

Related work order: [Texture-Surface GDI Upload HLE Work Order](../work-orders/20260825-066-texture-surface-gdi-hle.md)

Task 66 attributes AV 0x0042292b to GetDC on a DDSCAPS_TEXTURE surface and adds RGB565 DIB backing for primary, back, and texture surfaces; texture CreateSurface; GDI GetDC/ReleaseDC; DDCKEY_SRCBLT storage; shared-lifetime IDirect3DTexture2 identity; the subsequently confirmed DDBLT_COLORFILL rectangle path; and deterministic GDI cleanup.

Windows x86 Debug builds and CTest passes 2/2. Intermediate log `20260825-020704-982.jsonl` confirms removal of the old surface AV and exposes the null Blt slot. Final logs `20260825-020941-109.jsonl` and `20260825-021024-959.jsonl` remove both surface AVs and identically stop at execute address zero with stack return 0x0042325f. Static call site 0x0042325c is IDirect3DDevice3::DrawPrimitive vtable +0x70 with primitive type 5, vertex type 0x1c4, count four, vertex array 0x0045d528, and flags zero. Draw translation remains the next OpenGL HLE task. Original assets were not modified.
