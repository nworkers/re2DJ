# 작업 395 작업 지시서 — 창을 닫을 때까지 실행 / Task 395 work order — running until the window is closed

설계: [20260927-395-run-until-closed.md](../design/20260927-395-run-until-closed.md)

## 절차 / Steps

1. 호출 한도를 실행 환경의 선택 값으로 바꾸고, CLI `--call-limit`을 더한다.
   *Make the call limit an optional run setting and add the CLI's `--call-limit`.*
2. backend 이벤트 관찰자와 Linux 창 닫기 요청, continuation의 `kContinuationHostClosed` 경계를 더한다.
   *Add the backend event observer, Linux close requests, and the continuation's `kContinuationHostClosed` boundary.*
3. API 기록을 처음 32,768번으로 제한한다.
   *Limit the API log to the first 32,768 calls.*
4. 실제 실행으로 확인하고 문서를 쓴다.
   *Check with real runs and write the documentation.*

## 완료 조건 / Done when

- Windows x86과 Linux 두 폭이 build와 CTest를 통과하고, Windows 실제 4th 그래픽 기록이 작업 전과 같다.
  *Windows x86 and both Linux widths build and pass CTest, and the real 4th's graphics log on Windows matches the one before the task.*
- Linux 실제 4th가 한도 없이 계속 돌고, 창을 닫으면 정상 종료한다.
  *The real 4th on Linux keeps running with no limit and exits normally when its window is closed.*
