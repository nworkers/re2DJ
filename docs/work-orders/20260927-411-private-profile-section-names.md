# 작업 411 작업 지시서 — GetPrivateProfileSectionNamesA / Task 411 work order — GetPrivateProfileSectionNamesA

설계: [20260927-411-private-profile-section-names.md](../design/20260927-411-private-profile-section-names.md)

## 절차 / Steps

1. Windows 11에서 측정한다.
   *Measure it on Windows 11.*
2. kernel32 handler를 공용 INI core로 구현한다.
   *Implement the kernel32 handler on the shared INI core.*
3. 단위 테스트, 1st 실행으로 다음 경계 확인, 문서.
   *Unit tests, a 1st run for the next boundary, and documentation.*

## 완료 조건 / Done when

- Windows x86과 Linux 두 폭이 build와 테스트를 통과한다.
  *Windows x86 and both Linux widths build and pass their tests.*
- Linux 1st가 이 함수를 지나고 결과가 Windows 기록과 같다.
  *On Linux, 1st gets past it with the Windows log's result.*
