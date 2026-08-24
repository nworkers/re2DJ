# 구형 Direct3D Immediate Mode와 COM HLE 경계

## interface 획득 흐름

DirectX 6 세대 코드는 `DirectDrawCreate`로 `IDirectDraw`를 만든 뒤 `QueryInterface`로 `IDirectDraw4`와 `IDirect3D3`를 얻을 수 있다. 따라서 PE import 표면에 `DirectDrawCreate`만 있어도 Direct3D를 사용하지 않는다고 결론 내릴 수 없다. Microsoft의 [DirectDrawCreate 문서](https://learn.microsoft.com/en-us/windows/win32/api/ddraw/nf-ddraw-directdrawcreate)는 이 함수가 DirectDraw 객체를 만들며 Direct3D interface 지원 범위가 객체 생성 방식에 따라 달라짐을 명시한다.

Windows SDK의 `d3d.h`에서 `IDirect3D3` vtable은 `FindDevice`, `CreateDevice`, `EnumZBufferFormats`를 포함한다. `IDirect3DDevice3`는 `BeginScene`, `EndScene`, `SetRenderState`, `SetTexture` 같은 fixed-function Immediate Mode method를 제공한다.

*DirectX 6-era code can create `IDirectDraw` through `DirectDrawCreate` and obtain `IDirectDraw4` and `IDirect3D3` through `QueryInterface`. A PE that imports only `DirectDrawCreate` may therefore still use Direct3D. The Windows SDK `d3d.h` defines the relevant `IDirect3D3` and `IDirect3DDevice3` vtables.*

## hardware-only FindDevice

Windows SDK `d3dcaps.h`의 `D3DFINDDEVICESEARCH`는 `dwFlags`와 `bHardware`를 포함한다. `D3DFDS_HARDWARE`(`0x00000004`)를 세우고 `bHardware=TRUE`로 지정하면 hardware/software 여부를 검색 조건으로 사용한다. 일치하는 device가 없을 때 관찰 가능한 DirectDraw HRESULT 중 하나가 `DDERR_NOTFOUND`이며, SDK `ddraw.h` 정의 `MAKE_DDHRESULT(255)`의 값은 `0x887600ff`다.

*`D3DFINDDEVICESEARCH` carries `dwFlags` and `bHardware`. With `D3DFDS_HARDWARE` and `bHardware=TRUE`, hardware/software status becomes a selection criterion. `DDERR_NOTFOUND` is `MAKE_DDHRESULT(255)`, or `0x887600ff`.*

## HLE 설계 영향

`FindDevice`는 DLL import가 아니라 COM vtable method다. import thunk 하나만 바꿔서는 이 호출을 가로챌 수 없다. 안전한 HLE 경계는 다음과 같다.

```mermaid
flowchart LR
    G["guest DirectDrawCreate import"] --> P["HLE IDirectDraw proxy"]
    P --> Q["proxy QueryInterface"]
    Q --> D["HLE IDirect3D3 proxy"]
    D --> F["FindDevice policy"]
    D --> H["forwarded host methods"]
```

proxy는 COM identity와 `QueryInterface`/`AddRef`/`Release` 수명을 일관되게 유지해야 한다. host 객체의 공유 vtable을 직접 수정하면 같은 process의 다른 객체와 system runtime까지 오염할 수 있으므로 피한다. 하나의 논리 객체 그래프 안에서 system Direct3D 객체와 HLE 객체를 섞지 않고, 지원 method는 플랫폼 중립 graphics core로 전달하며 미지원 method는 결정적인 오류와 진단을 반환하는 구조가 import-thunk 원칙과 교체 가능성에 맞는다. EZ2DJ용 구체 설계는 [Direct3D 3 OpenGL HLE](../design/20260825-061-direct3d3-opengl-hle.md)에 둔다.

*Because `FindDevice` is a COM vtable method rather than a DLL import, a single import replacement cannot intercept it. A safe boundary starts with a proxy returned from the `DirectDrawCreate` import and preserves COM identity and reference counting across `QueryInterface`. Do not mix system Direct3D and HLE objects in one logical graph or mutate a shared host-object vtable. Route supported methods into a platform-neutral graphics core and return deterministic diagnostics for unsupported methods.*

## 구현으로 확인된 최소 초기화 집합

EZ2DJ 1st SE의 초기화 경로는 `FindDevice` 성공 뒤 16비트 Z-buffer format 열거, 640×480×16 primary/back flip chain, HAL device, viewport 생성과 설정을 순서대로 요구한다. device 생성 뒤에는 RGB565 texture format 열거와 device capability 조회도 실행한다. 작업 61의 guest 소유 facade가 이 집합을 제공했을 때 다섯 graphics stage가 모두 통과했으므로, 이 집합은 현재 target의 확인된 최소 초기화 계약이다. 실제 texture resource, primitive draw와 present 계약은 아직 포함하지 않는다.

*The confirmed minimum initialization set for EZ2DJ 1st SE is hardware-device discovery, 16-bit Z-format enumeration, a 640×480×16 primary/back flip chain, HAL device and viewport creation, RGB565 texture-format enumeration, and device-capability lookup. A guest-owned facade providing this set passes all five original graphics stages. Texture resources, primitive drawing, and presentation are not part of this confirmed minimum yet.*
