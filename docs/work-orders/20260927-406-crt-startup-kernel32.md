# 작업 406 작업 지시서 — 1st CRT 시작의 kernel32 함수 / Task 406 work order — kernel32 functions of 1st's CRT startup

설계: [20260927-406-crt-startup-kernel32.md](../design/20260927-406-crt-startup-kernel32.md)

## 절차 / Steps

1. Windows 11에서 critical section, TLS, Interlocked, `GetCurrentThread`, `IsBad*Ptr`를 측정한다.
   *Measure critical sections, TLS, the Interlocked functions, `GetCurrentThread`, and `IsBad*Ptr` on Windows 11.*
2. kernel32 facade에 구현하고 resolve-only에서 옮긴다.
   *Implement them in the kernel32 facade, moving them out of the resolve-only list.*
3. 단위 테스트, 1st 실행으로 다음 경계 확인, 문서.
   *Unit tests, a 1st run to find the next boundary, and documentation.*

## 완료 조건 / Done when

- Windows x86과 Linux 두 폭이 build와 테스트를 통과한다.
  *Windows x86 and both Linux widths build and pass their tests.*
- Linux 1st가 CRT 시작을 지난다.
  *On Linux, 1st gets past its CRT startup.*
