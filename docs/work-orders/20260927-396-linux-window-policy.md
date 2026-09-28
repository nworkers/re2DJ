# 작업 396 작업 지시서 — Linux 창 크기와 단축키 / Task 396 work order — the Linux window's size and shortcuts

설계: [20260927-396-linux-window-policy.md](../design/20260927-396-linux-window-policy.md)

## 절차 / Steps

1. Windows 제품의 창 정책을 정리하고, 배율 상수와 FPS 측정을 공용 헤더로 둔다.
   *Summarize the Windows product's window policy and put the scale constants and FPS measurement in a shared header.*
2. backend에 창 크기·전체 화면·제목 조작을 더한다.
   *Add window size, fullscreen, and title controls to the backend.*
3. Linux 창에 기본 2배, Alt+1/2/3, 더블클릭 전체 화면, 제목 FPS, `--fullscreen`/`--windowed`를 적용한다.
   *Give the Linux window the default 2x scale, Alt+1/2/3, double-click fullscreen, the title FPS, and `--fullscreen`/`--windowed`.*
4. 단위 테스트, 합성 이벤트 프로브, 실제 실행, Windows 전후 비교, 문서.
   *Unit tests, a synthetic-event probe, a real run, a Windows before/after comparison, and documentation.*

## 완료 조건 / Done when

- Windows x86과 Linux 두 폭이 build와 CTest를 통과하고, Windows 실제 4th 그래픽 기록이 작업 전과 같다.
  *Windows x86 and both Linux widths build and pass CTest, and the real 4th's graphics log on Windows matches the one before the task.*
- Linux 창이 1280×960으로 열리고, 단축키와 더블클릭이 Windows와 같이 동작하며, 제목에 FPS가 나온다.
  *The Linux window opens at 1280×960, the shortcuts and double click behave as on Windows, and the title shows the FPS.*
