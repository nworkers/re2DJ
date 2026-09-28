# 작업 389 작업 지시서 — 표면 DC와 StretchDIBits / Task 389 work order — surface DCs and StretchDIBits

설계: [20260926-389-surface-dc-and-stretchdibits.md](../design/20260926-389-surface-dc-and-stretchdibits.md)

## 절차 / Steps

1. `GetDC`/`ReleaseDC`/`SetColorKey` 규칙을 core로 옮기고, Windows facade가 쓰게 한다.
   *Move the `GetDC`/`ReleaseDC`/`SetColorKey` rules into the core and use them from the Windows facade.*
2. GDI 모델(`GuestGdi`, `gdi_raster`)을 만들고, Linux 표면 DC를 구현한다.
   *Build the GDI model (`GuestGdi`, `gdi_raster`) and implement Linux surface DCs.*
3. Windows 11에서 `StretchDIBits`를 측정하고 구현한다.
   *Measure `StretchDIBits` on Windows 11 and implement it.*
4. 검증과 문서.
   - 변경 전 build(`cb0a444`)와 Windows 실제 4th를 비교한다.
   - 단위 테스트를 추가한다.
   - 문서를 쓴다.

   *Compare the real 4th on Windows against the pre-change build (`cb0a444`), add unit tests, and write the documentation.*

## 완료 조건 / Done when

- Windows x86과 Linux 두 폭이 build와 CTest를 통과한다. Windows 실제 4th의 기록이 변경 전과 같다.
  *Windows x86 and both Linux widths build and pass CTest, and the real 4th's record on Windows matches the pre-change build.*
- Linux 실제 4th가 두 폭에서 텍스처 업로드를 마친다.
  *On both Linux widths the real 4th finishes its texture upload.*
