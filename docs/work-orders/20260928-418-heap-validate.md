# 작업 418 작업 지시서 — HeapValidate / Task 418 work order — HeapValidate

설계: [20260928-418-heap-validate.md](../design/20260928-418-heap-validate.md)

## 절차 / Steps

1. Windows에서 `HeapValidate`를 측정한다.
   *Measure `HeapValidate` on Windows.*
2. kernel32 handler와 단위 테스트를 만든다.
   *Add the kernel32 handler and unit tests.*
3. Linux 두 폭과 Windows에서 테스트하고, Linux 1st를 실행한다.
   *Test on both Linux widths and Windows, and run Linux 1st.*

## 완료 조건 / Done when

- 모든 build와 테스트가 통과한다.
  *Every build and test passes.*
- Linux 1st가 `HeapValidate`를 지난다.
  *Linux 1st gets past `HeapValidate`.*
