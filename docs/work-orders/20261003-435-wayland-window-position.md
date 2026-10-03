# 작업 435 작업 지시서 — Wayland에서 창 가운데 정렬 / Task 435 work order — centring the window on Wayland

설계: [20261003-435-wayland-window-position.md](../design/20261003-435-wayland-window-position.md)

## 절차 / Steps

1. `Sdl3OpenGlBackend::ResizeWindow`가 `SDL_SetWindowPosition`의 실패를 무시하고 크기 변경 실패만 돌려주게 한다. 헤더 주석을 맞춘다.
   *Make `Sdl3OpenGlBackend::ResizeWindow` ignore a failed `SDL_SetWindowPosition` and report only a failed resize; align the header comment.*
2. `ARCHITECTURE.md`와 작업 396 설계의 `ResizeWindow` 설명을 갱신한다.
   *Update the `ResizeWindow` description in `ARCHITECTURE.md` and task 396's design.*
3. Linux x64를 경고를 오류로 하여 빌드하고 CTest를 돌린다. Wayland 세션에서 6th CHD를 실행한다.
   *Build Linux x64 with warnings as errors and run CTest; run the 6th CHD on the Wayland session.*

## 완료 조건 / Done when

- build와 CTest가 통과한다.
  *The build and CTest pass.*
- Wayland 세션에서 6th 자식이 `SetCooperativeLevel`을 지나 창을 연다.
  *On the Wayland session the 6th child passes `SetCooperativeLevel` and opens its window.*
