# 작업 412 작업 지시서 — DirectDrawEnumerateA / Task 412 work order — DirectDrawEnumerateA

설계: [20260927-412-directdraw-enumerate.md](../design/20260927-412-directdraw-enumerate.md)

## 절차 / Steps

1. Windows 제품의 처리를 확인하고 Windows 11에서 측정한다.
   *Check the Windows product's handling and measure on Windows 11.*
2. 콜백 루프를 공유해 ddraw `DirectDrawEnumerateA`를 구현한다.
   *Implement ddraw `DirectDrawEnumerateA` on the shared callback loop.*
3. 단위 테스트, 1st 실행으로 다음 경계 확인, Windows 기록으로 DX6 호출 범위 조사, 문서.
   *Unit tests, a 1st run for the next boundary, a survey of the DX6 calls in the Windows log, and documentation.*

## 완료 조건 / Done when

- Windows x86과 Linux 두 폭이 build와 테스트를 통과한다.
  *Windows x86 and both Linux widths build and pass their tests.*
- Linux 1st가 `DirectDrawEnumerateA`를 지난다.
  *On Linux, 1st gets past `DirectDrawEnumerateA`.*
