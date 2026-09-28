# 작업 390 작업 지시서 — 메인 루프의 입력과 메시지 / Task 390 work order — main-loop input and messages

설계: [20260926-390-main-loop-input-and-messages.md](../design/20260926-390-main-loop-input-and-messages.md)

## 절차 / Steps

1. `GetCursorPos`, `ScreenToClient`, `PeekMessageA`, `TranslateMessage`, `DispatchMessageA`를 Windows 11에서 측정한다.
   *Measure `GetCursorPos`, `ScreenToClient`, `PeekMessageA`, `TranslateMessage`, and `DispatchMessageA` on Windows 11.*
2. 커서 위치, timer 기준 시각, 메시지 큐를 모델링하고 다섯 export를 구현한다.
   *Model the cursor position, timer base times, and the message queue, and implement the five exports.*
3. 단위 테스트와 문서.
   *Unit tests and documentation.*

## 완료 조건 / Done when

- Windows x86과 Linux 두 폭이 build와 CTest를 통과한다.
  *Windows x86 and both Linux widths build and pass CTest.*
- Linux 실제 4th가 두 폭에서 메인 루프의 입력과 메시지 조회를 지나 프레임 그리기에 들어간다.
  *On both Linux widths the real 4th gets past the main loop's input and message queries into drawing a frame.*
