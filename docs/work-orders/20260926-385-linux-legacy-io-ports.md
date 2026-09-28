# 작업 385 작업 지시서 — Linux IO 보드 포트 입출력 / Task 385 work order — I/O board port access on Linux

설계: [20260926-385-linux-legacy-io-ports.md](../design/20260926-385-linux-legacy-io-ports.md)

## 절차 / Steps

1. `legacy_io_trap.h/.cpp` core를 만들고, Windows handler의 판정을 core로 바꾼다.
   *Build the `legacy_io_trap.h/.cpp` core and move the Windows handler's decisions onto it.*
2. Linux `native_legacy_io`를 만들어 두 폭의 SIGSEGV 경로에 연결한다. 실행 환경과 결과에 정책과 활동을 더한다.
   *Add Linux `native_legacy_io` on both widths' SIGSEGV paths, with the policy and activity in the run environment and result.*
3. CLI가 프로필의 계약을 넘기고 활동을 출력하게 한다.
   *Have the CLI pass the profile's contract and print the activity.*
4. 검증과 문서.
   - 변경 전 build(`6280268`)와 Windows 실제 4th를 비교한다(그래픽·IO 포트 기록).
   - 단위 테스트를 추가한다.
   - 문서를 쓴다.

   *Compare the real 4th on Windows against the pre-change build (`6280268`) on its graphics and I/O port records, add unit tests, and write the documentation.*

## 완료 조건 / Done when

- Windows x86과 Linux 두 폭이 build와 CTest를 통과한다. Windows 실제 4th의 기록이 변경 전과 같다.
  *Windows x86 and both Linux widths build and pass CTest, and the real 4th's record on Windows matches the pre-change build.*
- Linux 실제 4th가 두 폭에서 IO 보드 포트 읽기를 지나간다.
  *On both Linux widths the real 4th gets past its I/O board port reads.*
