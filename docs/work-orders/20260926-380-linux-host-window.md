# 작업 380 작업 지시서 — Linux 호스트 창 / Task 380 work order — Linux host window

설계: [20260926-380-linux-host-window.md](../design/20260926-380-linux-host-window.md)

## 절차 / Steps

1. WSLg와 OpenGL을 blend probe로 확인한다.
   *Confirm WSLg and OpenGL with the blend probe.*
2. `HostPresentation`, `ImportCallServices::Presentation()`, `WindowTitle`을 추가하고, ddraw `SetCooperativeLevel` 정책에 연결한다.
   *Add `HostPresentation`, `ImportCallServices::Presentation()`, and `WindowTitle`, and wire the ddraw `SetCooperativeLevel` policy.*
3. `LinuxHostPresentation`, 진단·run environment 연결, CLI `--hold-window`, CMake link를 추가한다.
   *Add `LinuxHostPresentation`, the diagnostic and run-environment wiring, the CLI's `--hold-window`, and the CMake links.*
4. 창을 확인하고, 단위 테스트, 두 host 회귀, 문서를 마친다.
   *Check the window, then unit tests, both hosts' regressions, and documentation.*

## 완료 조건 / Done when

- 실제 4th가 두 폭에서 `SetCooperativeLevel` 때 제목이 맞는 640×480 창을 띄우고, `--hold-window`로 그 창을 볼 수 있다.
  *The real 4th opens a correctly titled 640×480 window at `SetCooperativeLevel` on both widths, viewable with `--hold-window`.*
- Windows x86과 Linux 두 폭 build·CTest가 통과하고, 나머지 결과가 전과 같다.
  *Windows x86 and both Linux widths build and pass CTest, with everything else as before.*
