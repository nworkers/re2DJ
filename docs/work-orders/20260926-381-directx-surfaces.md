# 작업 381 작업 지시서 — DirectX core 3단계: 표면 / Task 381 work order — DirectX core phase 3: surfaces

설계: [20260926-381-directx-surfaces.md](../design/20260926-381-directx-surfaces.md)

## 절차 / Steps

1. core에 `directdraw_surface.h/.cpp`를 두고, HRESULT·caps·IID 상수를 더한다.
   *Add `directdraw_surface.h/.cpp` to the core, with the HRESULT, caps, and IID constants.*
2. Windows `RootCreateSurface`와 attach, 설명, 형식, pitch가 core를 쓰게 하고, SDK 값 검사를 더한다.
   *Move Windows `RootCreateSurface`, attachments, descriptions, the format, and the pitch onto the core, and add the SDK value checks.*
3. Linux에서 다음을 추가한다.
   - `GuestComState` 수명 hook
   - `ddraw_surface7.cpp`
   - `IDirectDraw7::CreateSurface`

   *On Linux, add the `GuestComState` lifetime hooks, `ddraw_surface7.cpp`, and `IDirectDraw7::CreateSurface`.*
4. 검증과 문서.
   - 변경 전 build(`854f600`)와 Windows 실제 4th를 비교한다.
   - 단위 테스트를 추가한다(`directx_surface_test.cpp`, ddraw).
   - 문서를 쓴다.

   *Compare the real 4th on Windows against the pre-change build (`854f600`), add unit tests (`directx_surface_test.cpp`, ddraw), and write the documentation.*

## 완료 조건 / Done when

- Windows x86과 Linux 두 폭이 build와 CTest를 통과한다. Windows 실제 4th의 기록이 변경 전과 같다.
  *Windows x86 and both Linux widths build and pass CTest, and the real 4th's record on Windows matches the pre-change build.*
- Linux 실제 4th가 두 폭에서 `CreateSurface`를 지나 `IDirect3D7::CreateDevice`까지 간다.
  *On both Linux widths the real 4th passes `CreateSurface` and reaches `IDirect3D7::CreateDevice`.*
