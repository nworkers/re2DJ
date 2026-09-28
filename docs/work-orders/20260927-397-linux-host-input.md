# 작업 397 작업 지시서 — Linux host 입력 / Task 397 work order — Linux host input

설계: [20260927-397-linux-host-input.md](../design/20260927-397-linux-host-input.md)

## 절차 / Steps

1. VK 상수와 키 이름 해석, EZ2DJ 키 배치와 턴테이블 계산을 core로 옮기고, Windows가 그것을 쓰게 한다.
   *Move the VK constants and key-name parsing, the EZ2DJ key map, and the turntable arithmetic into the core, and have Windows use them.*
2. host 입력 상태와 `HostPresentation::Input()`을 더하고, Linux에서 SDL 이벤트로 채운다.
   *Add the host input state and `HostPresentation::Input()`, filled from SDL events on Linux.*
3. `GetAsyncKeyState`, DirectInput, `GetCursorPos`, IO 보드가 그 상태를 읽게 한다.
   *Have `GetAsyncKeyState`, DirectInput, `GetCursorPos`, and the I/O board read it.*
4. 단위 테스트, 합성 이벤트 프로브, 실제 실행, Windows 전후 비교, 문서.
   *Unit tests, a synthetic-event probe, a real run, a Windows before/after comparison, and documentation.*

## 완료 조건 / Done when

- Windows x86과 Linux 두 폭이 build와 CTest를 통과하고, Windows 실제 4th의 그래픽·IO 포트 기록이 작업 전과 같다.
  *Windows x86 and both Linux widths build and pass CTest, and the real 4th's graphics and I/O port logs on Windows match the ones before the task.*
- Linux 창에서 누른 키로 4th에 코인을 넣고 게임을 시작할 수 있다.
  *Keys pressed in the Linux window insert coins in the 4th and start a game.*
