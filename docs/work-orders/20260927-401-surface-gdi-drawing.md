# 작업 401 작업 지시서 — 표면 DC의 GDI 그리기 / Task 401 work order — GDI drawing into surface DCs

설계: [20260927-401-surface-gdi-drawing.md](../design/20260927-401-surface-gdi-drawing.md)

## 절차 / Steps

1. `CreateSolidBrush`, `DeleteObject`, `FillRect`, `SetTextColor`, `SetBkMode`, `DrawTextA`를 Windows 11에서 측정한다.
   *Measure `CreateSolidBrush`, `DeleteObject`, `FillRect`, `SetTextColor`, `SetBkMode`, and `DrawTextA` on Windows 11.*
2. `GuestGdi` 브러시, `gdi_raster`의 채우기 규칙, gdi32·user32 함수를 구현한다.
   *Implement `GuestGdi` brushes, `gdi_raster`'s fill rules, and the gdi32 and user32 functions.*
3. 단위 테스트, 실제 실행, 문서.
   *Unit tests, a real run, and documentation.*

## 완료 조건 / Done when

- Windows x86과 Linux 두 폭이 build와 CTest를 통과한다.
  *Windows x86 and both Linux widths build and pass CTest.*
- Linux 실제 4th가 곡 정보 화면의 대체 텍스처 그리기를 지난다.
  *On Linux the real 4th gets past drawing the song info screen's stand-in texture.*
