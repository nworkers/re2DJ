# 작업 408 작업 지시서 — EnumDisplaySettingsA와 ChangeDisplaySettingsExA / Task 408 work order — EnumDisplaySettingsA and ChangeDisplaySettingsExA

설계: [20260927-408-display-settings.md](../design/20260927-408-display-settings.md)

## 절차 / Steps

1. Windows 제품의 두 함수 처리와 1st의 요청을 확인하고, `EnumDisplaySettingsA`가 채우는 DEVMODEA 바이트를 Windows 11에서 측정한다.
   *Check how the Windows product handles both functions and what 1st asks for, and measure the DEVMODEA bytes `EnumDisplaySettingsA` writes on Windows 11.*
2. host 데스크톱 모드 조회(`HostPresentation`, SDL backend), user32 두 함수를 구현한다.
   *Implement the host desktop mode query (`HostPresentation`, the SDL backend) and both user32 functions.*
3. 단위 테스트, 1st 실행으로 다음 경계 확인, 문서.
   *Unit tests, a 1st run for the next boundary, and documentation.*

## 완료 조건 / Done when

- Windows x86과 Linux 두 폭이 build와 테스트를 통과한다.
  *Windows x86 and both Linux widths build and pass their tests.*
- Linux 1st가 두 함수를 지난다.
  *On Linux, 1st gets past both functions.*
