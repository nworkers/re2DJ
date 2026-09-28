# 작업 391 작업 지시서 — DirectX 5단계: 그리기와 Linux 창 표시 / Task 391 work order — DirectX phase 5: drawing and the Linux window

설계: [20260926-391-directx-drawing.md](../design/20260926-391-directx-drawing.md)

## 절차 / Steps

1. Windows facade의 그리기 규칙을 `direct3d_draw.h` core로 옮기고, facade가 core를 부르게 한다.
   *Move the Windows facade's drawing rules into the `direct3d_draw.h` core and have the facade call it.*
2. `HostPresentation`에 그리기 계약을 더하고, Linux에서 공용 SDL3/OpenGL backend로 구현한다.
   *Add the drawing contract to `HostPresentation` and implement it on Linux with the shared SDL3/OpenGL backend.*
3. Linux `IDirect3DDevice7`의 `Clear`, `SetTexture`, `DrawPrimitive`와 표면의 `Flip`, 텍스처 revision과 폐기를 구현한다.
   *Implement Linux `IDirect3DDevice7`'s `Clear`, `SetTexture`, and `DrawPrimitive`, the surface's `Flip`, and texture revisions and discarding.*
4. 단위 테스트, Windows 실제 4th 전후 비교, 문서.
   *Unit tests, a before/after comparison of the real 4th on Windows, and documentation.*

## 완료 조건 / Done when

- Windows x86과 Linux 두 폭이 build와 CTest를 통과한다.
  *Windows x86 and both Linux widths build and pass CTest.*
- Windows 실제 4th의 그래픽 기록이 작업 전과 같다.
  *The real 4th's graphics log on Windows matches the one before the task.*
- Linux 두 폭의 창에 4th의 첫 화면(WARNING)이 보인다.
  *The 4th's first screen (WARNING) shows in the Linux window on both widths.*
