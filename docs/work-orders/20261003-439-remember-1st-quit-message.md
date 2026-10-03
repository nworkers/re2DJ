# 작업 439 작업 지시서 — Remember 1st의 WM_QUIT / Task 439 work order — Remember 1st's WM_QUIT

설계: [20261003-439-remember-1st-quit-message.md](../design/20261003-439-remember-1st-quit-message.md)

## 절차 / Steps

1. 1st WinMain의 끝(`WM_DESTROY`, `PostQuitMessage`, `WM_QUIT` 펌프, 반환값)을 원본에서 확인한다.
   *Confirm the end of 1st's WinMain (`WM_DESTROY`, `PostQuitMessage`, the `WM_QUIT` pump, the return value) in the original.*
2. `GuestUser`의 quit 표시, user32 `PostQuitMessage`, `PeekMessageA`의 `WM_QUIT`, 단위 테스트.
   *`GuestUser`'s quit flag, user32 `PostQuitMessage`, `WM_QUIT` from `PeekMessageA`, and unit tests.*
3. Linux x64 build와 CTest, 분석·작업 로그.
   *The Linux x64 build and CTest, the analysis and the work log.*

## 완료 조건 / Done when

- build와 CTest가 통과한다.
  *The build and CTest pass.*
- 사용자가 1st 한 판 뒤 6th로 돌아가는 것을 확인한다.
  *The user returns to 6th after one game of 1st.*
