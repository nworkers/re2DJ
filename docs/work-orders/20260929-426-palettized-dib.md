# 작업 426 작업 지시서 — StretchDIBits의 8비트 팔레트 DIB / Task 426 work order — 8-bit palettized DIBs in StretchDIBits

설계: [20260929-426-palettized-dib.md](../design/20260929-426-palettized-dib.md)

## 절차 / Steps

1. 사용자 로그(`re2dj_log.txt`)에서 멈춘 호출과 DIB 형식을 확인한다.
   *Identify the stopping call and the DIB format in the user's log (`re2dj_log.txt`).*
2. Windows에서 8비트 DIB의 변환을 측정한다.
   *Measure the conversion of 8-bit DIBs on Windows.*
3. gdi32 `StretchDIBits`와 단위 테스트를 고친다. Linux 두 폭과 Windows에서 테스트하고, Linux 5th를 두 폭에서 실행한다.
   *Update gdi32 `StretchDIBits` and its unit tests, test on both Linux widths and Windows, and run Linux 5th on both widths.*

## 완료 조건 / Done when

- 모든 build와 테스트가 통과한다.
  *Every build and test passes.*
- Linux 5th가 `fadeblack.abm`을 지난다.
  *Linux 5th gets past `fadeblack.abm`.*
