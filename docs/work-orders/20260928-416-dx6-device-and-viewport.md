# 작업 416 작업 지시서 — DX6 장치·표면·viewport를 한 묶음으로 / Task 416 work order — the DX6 device, surfaces, and viewport in one batch

설계: [20260928-416-dx6-device-and-viewport.md](../design/20260928-416-dx6-device-and-viewport.md)

## 절차 / Steps

1. Linux 1st를 실행하고, 멈춘 DirectDraw·Direct3D 호출을 구현한 뒤 다시 실행한다. DirectX가 아닌 미구현 API가 나올 때까지 반복한다.
   *Run Linux 1st, implement the DirectDraw or Direct3D call it stops at, and run again, until an unimplemented API outside DirectX appears.*
2. Windows DX6 facade가 안에서 만들던 답은 공용 core로 옮기고, facade가 그 core를 쓰게 한다.
   *Move answers the Windows DX6 facade built inline to the shared core, and make the facade use it.*
3. core와 모듈 단위 테스트, Windows 1st 짧은 실행, Linux 두 폭 1st 실행, 문서.
   *Core and module unit tests, a short Windows 1st run, Linux 1st runs at both widths, and documentation.*

## 완료 조건 / Done when

- Windows x86과 Linux 두 폭이 build와 테스트를 통과한다.
  *Windows x86 and both Linux widths build and pass their tests.*
- Windows 1st가 전처럼 `FindDevice` → `CreateDevice` → `DrawPrimitive`로 간다.
  *Windows 1st still goes `FindDevice` → `CreateDevice` → `DrawPrimitive`.*
- Linux 1st가 DX6 초기화를 모두 지나 DirectX가 아닌 API에서 멈춘다.
  *Linux 1st gets through DX6 initialisation and stops at an API outside DirectX.*
