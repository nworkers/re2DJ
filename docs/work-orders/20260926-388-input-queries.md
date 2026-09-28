# 작업 388 작업 지시서 — 키 상태 조회와 mixer 식별 / Task 388 work order — key state queries and mixer identification

설계: [20260926-388-input-queries.md](../design/20260926-388-input-queries.md)

## 절차 / Steps

1. `GetAsyncKeyState`를 측정하고 구현한다.
   *Measure and implement `GetAsyncKeyState`.*
2. mixer ID 자리에 handle을 넘기는 경우를 측정하고, mixer 식별 규칙을 고친다.
   *Measure a handle passed where a mixer ID is expected, and fix the mixer identification rule.*
3. 단위 테스트와 문서.
   *Unit tests and documentation.*

## 완료 조건 / Done when

- Windows x86과 Linux 두 폭이 build와 CTest를 통과한다.
  *Windows x86 and both Linux widths build and pass CTest.*
- Linux 실제 4th가 두 폭에서 키 상태 조회와 mixer 설정을 지난다.
  *On both Linux widths the real 4th gets past its key state query and mixer setup.*
