# 작업 413 설계 — DirectDrawCreate와 DirectX 6 객체 / Task 413 design — DirectDrawCreate and the DirectX 6 objects

선행: [작업 412 설계](20260927-412-directdraw-enumerate.md)

## 배경 / Background

작업 412 뒤 Linux의 EZ2DJ 1st는 `ddraw!DirectDrawCreate`에서 멈췄다. Windows 제품은 이 호출을 DX6 facade(`direct3d3_com_facade.cpp`)로 받는다. facade는 루트 객체 하나에 `IDirectDraw4`와 `IDirect3D3` 인터페이스를 두고, `IUnknown`·`IDirectDraw`·`IDirectDraw2`·`IDirectDraw4`에는 같은 `IDirectDraw4` 표를 준다. Windows 기록에서 1st가 요청하는 인터페이스는 `IDirectDraw4`와 `IDirect3D3` 두 개다.

*After Task 412, EZ2DJ 1st on Linux stopped at `ddraw!DirectDrawCreate`, which the Windows product routes to its DX6 facade (`direct3d3_com_facade.cpp`): one root object carries `IDirectDraw4` and `IDirect3D3`, handing the same `IDirectDraw4` table out for `IUnknown`, `IDirectDraw`, `IDirectDraw2`, and `IDirectDraw4`. Per the Windows log, 1st asks for `IDirectDraw4` and `IDirect3D3`.*

## 결정 / Decisions

1. **DX6 객체는 DX7 객체와 다른 종류다.** 새 파일 `ddraw_direct_draw4.cpp`에 `IDirectDraw4`(28 메서드)와 `IDirect3D3`(12 메서드)를 ddraw.h·d3d.h의 vtable 순서로 둔다. 객체 종류도 따로 두어 DX7 메서드가 DX6 객체를 받지 않게 한다.
   ***DX6 objects are their own kinds.** A new `ddraw_direct_draw4.cpp` holds `IDirectDraw4` (28 methods) and `IDirect3D3` (12) in ddraw.h and d3d.h vtable order, with object kinds of their own so DirectX 7 methods never take them.*
2. **`DirectDrawCreate`**: NULL GUID로 `IDirectDraw4` 표를 가진 객체를 준다. 결과 코드는 facade와 같다. NULL `lplpDD`는 `DDERR_INVALIDPARAMS`, 집계는 출력을 NULL로 두고 `CLASS_E_NOAGGREGATION`이다. 장치 GUID는 멈춘다.
   ***`DirectDrawCreate`** gives, for a null GUID, an object with the `IDirectDraw4` table. As the facade answers, a null `lplpDD` is `DDERR_INVALIDPARAMS` and aggregation `CLASS_E_NOAGGREGATION` with the output set to null; a device GUID stops.*
3. **`QueryInterface`** (두 인터페이스 공통, facade처럼): `IUnknown`·`IDirectDraw`·`IDirectDraw4`는 DirectDraw 객체다. `IDirect3D3`는 그 위의 Direct3D 객체이며 DirectDraw 객체를 붙잡는다. `IDirectDraw2`는 1st가 쓰지 않고 측정하지 않았으므로, 다른 인터페이스처럼 멈춘다.
   ***`QueryInterface`**, shared by both as in the facade: `IUnknown`, `IDirectDraw`, and `IDirectDraw4` give the DirectDraw object, `IDirect3D3` a Direct3D object on it that holds it alive. `IDirectDraw2`, unused by 1st and unmeasured, stops like any other interface.*
4. 나머지 메서드는 호출되면 이름을 대고 멈춘다. 다음 경계부터 하나씩 채운다. 가능하면 DX7에서 쓴 공용 core(협력 수준·모드·표면 계획)를 재사용한다.
   *Every other method stops, naming itself, to be filled in boundary by boundary, reusing the DirectX 7 shared cores (cooperative level, mode, surface plans) where they apply.*
