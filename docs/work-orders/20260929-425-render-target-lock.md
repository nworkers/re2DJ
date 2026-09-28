# 작업 425 작업 지시서 — 렌더 타깃 Lock의 되읽기와 올리기 / Task 425 work order — reading back and writing the render target on Lock

설계: [20260929-425-render-target-lock.md](../design/20260929-425-render-target-lock.md)

## 절차 / Steps

1. 공용 backend에 렌더 타깃 읽기·쓰기를 더한다.
   *Add render-target reads and writes to the shared backend.*
2. `HostPresentation`, Linux 구현, ddraw 모듈의 Lock/Unlock, Windows facade를 연결한다.
   *Wire them through `HostPresentation`, the Linux implementation, the ddraw module's Lock/Unlock, and the Windows facade.*
3. 모듈 단위 테스트를 만든다. Linux 두 폭과 Windows에서 테스트하고, 두 host의 4th에서 F1을 눌러 화면을 캡처한다.
   *Add module unit tests, test on both Linux widths and Windows, and capture the screen after F1 in 4th on both hosts.*

## 완료 조건 / Done when

- 모든 build와 테스트가 통과한다.
  *Every build and test passes.*
- 두 host에서 F1 뒤 4th 테스트 모드 메뉴가 보인다.
  *4th's test-mode menu shows after F1 on both hosts.*
