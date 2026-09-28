# 작업 393 작업 지시서 — 현재 디렉터리 / Task 393 work order — the current directory

설계: [20260927-393-current-directory.md](../design/20260927-393-current-directory.md)

## 절차 / Steps

1. `GetCurrentDirectoryA`와 `SetCurrentDirectoryA`를 Windows 11에서 측정한다.
   *Measure `GetCurrentDirectoryA` and `SetCurrentDirectoryA` on Windows 11.*
2. `GuestFiles`에 현재 디렉터리를 두고 상대 경로가 그것을 기준으로 풀리게 한다.
   *Give `GuestFiles` a current directory that relative paths resolve against.*
3. kernel32의 두 export를 구현한다.
   *Implement the two kernel32 exports.*
4. 단위 테스트와 문서.
   *Unit tests and documentation.*

## 완료 조건 / Done when

- Windows x86과 Linux 두 폭이 build와 CTest를 통과한다.
  *Windows x86 and both Linux widths build and pass CTest.*
- Linux 실제 4th가 두 폭에서 현재 디렉터리를 옮겨 다음 장면의 자원을 읽는다.
  *On both Linux widths the real 4th moves its current directory and reads the next scene's resources.*
