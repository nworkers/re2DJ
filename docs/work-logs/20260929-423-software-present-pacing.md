# 작업 423 작업 로그 — vsync가 막지 않는 host의 소프트웨어 페이싱 / Task 423 work log — software present pacing where vsync does not block

설계: [20260929-423-software-present-pacing.md](../design/20260929-423-software-present-pacing.md) · 지시서: [20260929-423-software-present-pacing.md](../work-orders/20260929-423-software-present-pacing.md)

## 2026-09-29

- 적용 전 WSLg의 1st SE(x64)는 창 제목 FPS가 105~115였다. Windows는 60.0이었다.
  *Before the change, 1st SE (x64) on WSLg showed 105–115 FPS in its window title, against 60.0 on Windows.*
- 테스트 결과, 실패 0:
  - Windows x86: CTest 6개 통과, 단위 5292 checks.
  - Linux x64·x86: CTest 4개 통과, 단위 5289 checks.

  *Test results, no failures:*
  - *Windows x86: all 6 CTest tests pass, 5292 unit checks.*
  - *Linux x64 and x86: all 4 CTest tests pass, 5289 unit checks.*
- 실행 확인:

  | 실행 / Run | 결과 / Result |
  | --- | --- |
  | Linux 1st SE (x64, 45초) | 창이 열리고 약 1초 뒤 pacing이 켜짐(로그 기록). 창 제목은 30초·38초 모두 `FPS : 60.0` |
  | Windows 1st SE (45초) | `applied_interval=1`, present 간격 평균 16.66~16.67 ms(실제 vsync). pacing은 켜지지 않음(`software-pacing` 기록 없음) |
  | Linux 4th (x64, 90초) | pacing이 켜지고, 멈추는 import 없이 창이 닫힐 때까지 돎(1,671,627호출) |

  *Runs:*
  - *Linux 1st SE (x64, 45 s): pacing engaged about a second after the window opened (logged); the window title read `FPS : 60.0` at both 30 s and 38 s.*
  - *Windows 1st SE (45 s): `applied_interval=1`, present intervals averaging 16.66–16.67 ms (real vsync); pacing did not engage (no `software-pacing` record).*
  - *Linux 4th (x64, 90 s): pacing engaged, and it ran until the window closed with no stopping import (1,671,627 calls).*
- 같은 날 사용자가 Linux 1st SE의 화면, 코인·시작 입력, 소리가 정상임을 확인했다.
  *The same day the user confirmed that Linux 1st SE's picture, coin and start input, and sound are all fine.*
