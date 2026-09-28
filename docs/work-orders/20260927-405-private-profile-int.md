# 작업 405 작업 지시서 — GetPrivateProfileIntA와 디렉터리 덤프 파일 / Task 405 work order — GetPrivateProfileIntA and directory-dump files

설계: [20260927-405-private-profile-int.md](../design/20260927-405-private-profile-int.md)

## 절차 / Steps

1. Windows 11에서 `GetPrivateProfileIntA`의 해석 규칙과 last error를 측정한다.
   *Measure `GetPrivateProfileIntA`'s parsing rules and last errors on Windows 11.*
2. 공용 INI core와 `DemoVolume` 정책을 만들고 Windows 제품이 그 정책을 쓰게 한다.
   *Build the shared INI core and DemoVolume policy, and have the Windows product use the policy.*
3. `GuestFiles` 디렉터리 원본, Linux kernel32 `GetPrivateProfileIntA`, 1st의 import 이름 resolve-only 등록을 구현한다.
   *Implement the `GuestFiles` directory source, Linux kernel32 `GetPrivateProfileIntA`, and resolve-only registration of 1st's import names.*
4. 단위 테스트, 실제 `bookkeeping.ini`로 Windows 기록과 값 비교, 두 폭의 4th 회귀, 1st 실행, 문서.
   *Unit tests, a value comparison on the real `bookkeeping.ini` against the Windows log, a 4th regression on both widths, a 1st run, and documentation.*

## 완료 조건 / Done when

- Windows x86과 Linux 두 폭이 build와 테스트를 통과한다.
  *Windows x86 and both Linux widths build and pass their tests.*
- 실제 `bookkeeping.ini`의 값이 Windows 제품 기록과 같다.
  *The real `bookkeeping.ini` values match the Windows product's log.*
- Linux 1st가 이름 조회를 모두 지나고, 4th는 전과 같다.
  *Linux 1st gets past every name lookup, and 4th runs as before.*
