# 작업 414 작업 지시서 — IDirectDraw4의 협력 수준과 화면 모드 / Task 414 work order — IDirectDraw4's cooperative level and display mode

설계: [20260928-414-dx6-cooperative-level.md](../design/20260928-414-dx6-cooperative-level.md)

## 절차 / Steps

1. Windows DX6 facade가 쓰는 공용 core를 확인한다.
   *Check the shared cores the Windows DX6 facade uses.*
2. DX7 handler 본문을 공용 함수로 떼어 DX6에서도 쓴다.
   *Move the DX7 handler bodies into shared functions DX6 uses too.*
3. 단위 테스트, 1st 실행으로 다음 경계 확인, 문서.
   *Unit tests, a 1st run for the next boundary, and documentation.*

## 완료 조건 / Done when

- Windows x86과 Linux 두 폭이 build와 테스트를 통과한다.
  *Windows x86 and both Linux widths build and pass their tests.*
- Linux 1st가 `SetCooperativeLevel`을 지난다.
  *On Linux, 1st gets past `SetCooperativeLevel`.*
