# 작업 369 작업 지시서 — 정적 초기화에서 WinMain까지 / Task 369 work order — From static initializers to WinMain

설계: [20260925-369-static-initializers-to-winmain.md](../design/20260925-369-static-initializers-to-winmain.md)

## 절차 / Steps

1. `GuestProcess`에 event를 두고, `CreateEventA`·`SetEvent`·`ResetEvent`·`WaitForSingleObject`를 구현한다. `CloseHandle`이 event도 닫게 한다.
   *Add events to `GuestProcess` with `CreateEventA`, `SetEvent`, `ResetEvent`, and `WaitForSingleObject`, and have `CloseHandle` close events.*
2. `ReadClock` 서비스와 `win32_time`을 추가하고, 시간 export 다섯 개를 구현한다. Linux 진단은 host 시계를 제공한다.
   *Add the `ReadClock` service and `win32_time`, implement the five time exports, and have the Linux diagnostic provide the host clock.*
3. 단위 테스트, 분석·TODO·ARCHITECTURE 갱신.
   *Unit tests and analysis/TODO/ARCHITECTURE updates.*

## 완료 조건 / Done when

- Linux 두 폭과 Windows x86 build·CTest가 통과한다. 기존 진단·probe가 그대로다.
  *Both Linux widths and Windows x86 build and pass CTest, with existing diagnostics and probes unchanged.*
- 실제 4th가 두 폭에서 WinMain의 `timeBeginPeriod`에서 멈춘다. 주소를 정규화한 기록이 같다.
  *On both widths the real 4th stops at WinMain's `timeBeginPeriod`, with identical records after address normalization.*
