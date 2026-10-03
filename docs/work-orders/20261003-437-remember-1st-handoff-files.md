# 작업 437 작업 지시서 — Remember 1st로 넘기는 파일 / Task 437 work order — the files handed to Remember 1st

설계: [20261003-437-remember-1st-handoff-files.md](../design/20261003-437-remember-1st-handoff-files.md)

## 절차 / Steps

1. 6th의 넘김 루틴(`0x0044b250`, `0x0044b0a0`)을 원본에서 확인한다.
   *Confirm 6th's hand-over routine (`0x0044b250`, `0x0044b0a0`) in the original.*
2. `GuestFiles`: overlay 경로의 대소문자 무시, whiteout 목록, `Delete`. `kWin32ErrorSharingViolation`.
   *`GuestFiles`: case-insensitive overlay paths, the whiteout list, and `Delete`; `kWin32ErrorSharingViolation`.*
3. kernel32 `DeleteFileA`(Linux facade).
   *kernel32 `DeleteFileA` (Linux facade).*
4. 단위 테스트, Linux x64 build와 CTest.
   *Unit tests, the Linux x64 build and CTest.*
5. 문서: 작업 434 분석의 미확정 항목, `ARCHITECTURE.md`, 작업 로그.
   *Documents: task 434's unresolved analysis item, `ARCHITECTURE.md`, and the work log.*

## 완료 조건 / Done when

- build와 CTest가 통과한다.
  *The build and CTest pass.*
- 사용자가 Linux에서 6th를 거쳐 Remember 1st 타이틀까지 가는 것을 확인한다.
  *The user reaches Remember 1st's title through 6th on Linux.*
