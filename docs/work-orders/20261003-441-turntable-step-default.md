# 작업 441 작업 지시서 — 턴테이블 step 기본값 2 / Task 441 work order — a default turntable step of 2

설계: [20261003-441-turntable-step-default.md](../design/20261003-441-turntable-step-default.md)

## 절차 / Steps

1. `kEz2DjDefaultTurntableStep`을 2로 바꾸고, 공용 테스트에 예제 INI `step` 대조를 더한다.
   *Set `kEz2DjDefaultTurntableStep` to 2 and add the example INI's `step` to the shared test's comparison.*
2. 작업 085·306 설계의 기본값 언급을 고친다.
   *Correct the default mentioned in tasks 085 and 306's designs.*
3. Linux x64 build와 CTest, 작업 로그.
   *The Linux x64 build and CTest, and the work log.*

## 완료 조건 / Done when

build와 CTest가 통과한다.

*The build and CTest pass.*
