# 작업 430 작업 지시서 — DX7 조명과 깊이 기본값 / Task 430 work order — DX7 lighting and depth defaults

설계: [20260930-430-d3d7-lighting-defaults.md](../design/20260930-430-d3d7-lighting-defaults.md)

## 절차 / Steps

1. Win32 6th의 모드 선택 화면을 draw 진단으로 실행해, 그려지지 않는 그리기와 그 이유를 찾는다.
   *Run Win32 6th's mode select with draw diagnostics and find the draws that do not appear and why.*
2. 새 장치의 render state와 광원 없는 조명 색을 Windows 11에서 측정한다.
   *Measure a new device's render states and the colour lighting gives with no light on Windows 11.*
3. 측정값을 공용 core로 옮기고, 두 host facade의 material을 연결한다.
   *Move the measurements into the shared core and connect the material in both hosts' facades.*
4. 단위 테스트를 만든다. 세 host에서 테스트하고, 6th 모드 선택 화면과 4th·5th 타이틀을 확인한다.
   *Add unit tests, test on all three hosts, and check 6th's mode select and 4th's and 5th's title screens.*

## 완료 조건 / Done when

- 모든 build와 테스트가 통과한다.
  *Every build and test passes.*
- Win32 6th 모드 선택 화면에 모드별 그림이 나온다.
  *The per-mode pictures show in Win32 6th's mode select.*
