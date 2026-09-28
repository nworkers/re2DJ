# 작업 423 작업 지시서 — vsync가 막지 않는 host의 소프트웨어 페이싱 / Task 423 work order — software present pacing where vsync does not block

설계: [20260929-423-software-present-pacing.md](../design/20260929-423-software-present-pacing.md)

## 절차 / Steps

1. `PresentPacer`와 단위 테스트를 만든다.
   *Add `PresentPacer` and its unit tests.*
2. 공용 backend의 present 뒤에 연결하고, 두 host에서 켜질 때 기록한다.
   *Hook it in after the shared backend's present, recording engagement on both hosts.*
3. Linux 두 폭과 Windows에서 테스트한다. Linux 1st SE와 4th, Windows 1st SE를 실행한다.
   *Test on both Linux widths and Windows; run Linux 1st SE and 4th, and Windows 1st SE.*

## 완료 조건 / Done when

- 모든 build와 테스트가 통과한다.
  *Every build and test passes.*
- WSLg의 1st SE가 60 FPS로 돈다.
  *1st SE on WSLg runs at 60 FPS.*
- 실제 vsync가 있는 Windows에서는 pacing이 켜지지 않는다.
  *On Windows, where vsync does block, pacing does not engage.*
