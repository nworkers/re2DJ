# 작업 431 작업 지시서 — Linux 6th와 자식 프로세스 / Task 431 work order — Linux 6th and child processes

설계: [20260930-431-linux-6th-child-process.md](../design/20260930-431-linux-6th-child-process.md)

## 절차 / Steps

1. Linux에서 6th을 실행해 멈추는 곳을 찾고, launcher와 자식이 서로 무엇을 주고받는지 원본에서 확인한다.
   *Run 6th on Linux, find where it stops, and establish from the original what the launcher and the child hand each other.*
2. `CreateProcessA`와 관련 호출, 그리고 자식이 이어서 만나는 호출을 Windows 11에서 측정한다.
   *Measure `CreateProcessA` and the calls around it, and the calls the child then reaches, on Windows 11.*
3. 공용 인터페이스, kernel32·user32·gdi32, Linux host 실행기, CLI 옵션을 구현한다.
   *Implement the shared interfaces, kernel32, user32 and gdi32, the Linux host launcher, and the CLI options.*
4. 단위 테스트를 만들고 세 host에서 테스트한다. Linux 두 폭에서 6th을 코인·모드 선택까지 실행한다.
   *Add unit tests, test on all three hosts, and run 6th on both Linux widths through coins and mode select.*

## 완료 조건 / Done when

- 모든 build와 테스트가 통과한다.
  *Every build and test passes.*
- Linux 두 폭에서 6th이 launcher를 거쳐 게임으로 들어가고, 시간 제한까지 멈추지 않는다.
  *6th enters the game through its launcher on both Linux widths and does not stop before the timeout.*
