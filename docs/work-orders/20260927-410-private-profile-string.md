# 작업 410 작업 지시서 — GetPrivateProfileStringA / Task 410 work order — GetPrivateProfileStringA

설계: [20260927-410-private-profile-string.md](../design/20260927-410-private-profile-string.md)

## 절차 / Steps

1. Windows 11에서 `GetPrivateProfileStringA`의 값·기본값·버퍼·목록 규칙을 측정한다.
   *Measure `GetPrivateProfileStringA`'s rules for values, defaults, buffers, and lists on Windows 11.*
2. 공용 INI core와 kernel32 handler를 구현한다.
   *Implement the shared INI core and the kernel32 handler.*
3. 단위 테스트, 1st 실행으로 다음 경계 확인, 문서.
   *Unit tests, a 1st run for the next boundary, and documentation.*

## 완료 조건 / Done when

- Windows x86과 Linux 두 폭이 build와 테스트를 통과한다.
  *Windows x86 and both Linux widths build and pass their tests.*
- Linux 1st가 `GetPrivateProfileStringA`를 지난다.
  *On Linux, 1st gets past `GetPrivateProfileStringA`.*
