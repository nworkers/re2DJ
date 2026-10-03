# 작업 438 작업 지시서 — Remember 1st에서 6th로 돌아가기 / Task 438 work order — returning from Remember 1st to 6th

설계: [20261003-438-remember-1st-return.md](../design/20261003-438-remember-1st-return.md)

## 절차 / Steps

1. 1st의 정상 종료 경로와 종료 코드를 원본에서 확인하고, 작업 434 분석을 정정한다.
   *Confirm 1st's normal exit path and exit code in the original, and correct task 434's analysis.*
2. Linux `IDirectDraw4`·`IDirectDraw7`의 `RestoreDisplayMode`와 단위 테스트.
   *`RestoreDisplayMode` on the Linux `IDirectDraw4` and `IDirectDraw7`, with unit tests.*
3. Linux x64 build와 CTest, 작업 로그.
   *The Linux x64 build and CTest, and the work log.*

## 완료 조건 / Done when

- build와 CTest가 통과한다.
  *The build and CTest pass.*
- 사용자가 1st 한 판 뒤 6th로 돌아가는 것을 확인한다.
  *The user returns to 6th after one game of 1st.*
