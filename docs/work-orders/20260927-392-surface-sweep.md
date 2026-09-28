# 작업 392 작업 지시서 — 표면 점검: EnumSurfaces와 RestoreAllSurfaces / Task 392 work order — the surface sweep: EnumSurfaces and RestoreAllSurfaces

설계: [20260927-392-surface-sweep.md](../design/20260927-392-surface-sweep.md)

## 절차 / Steps

1. 32비트 측정 프로그램으로 Windows 11의 `EnumSurfaces`(대상, 순서, 참조, 설명, 중단, 거절)와 `RestoreAllSurfaces`를 측정한다.
   *Measure Windows 11's `EnumSurfaces` (which surfaces, order, references, descriptions, cancelling, refusals) and `RestoreAllSurfaces` with a 32-bit program.*
2. 플래그 규칙을 core에 두고 Windows facade가 쓰게 한다.
   *Put the flag rules in the core and have the Windows facade use them.*
3. Linux `ddraw.dll`에 `EnumSurfaces`와 `RestoreAllSurfaces`를 구현한다.
   *Implement `EnumSurfaces` and `RestoreAllSurfaces` in Linux's `ddraw.dll`.*
4. 단위 테스트, Windows 실제 4th 전후 비교, 문서.
   *Unit tests, a before/after comparison of the real 4th on Windows, and documentation.*

## 완료 조건 / Done when

- Windows x86과 Linux 두 폭이 build와 CTest를 통과한다.
  *Windows x86 and both Linux widths build and pass CTest.*
- Windows 실제 4th의 그래픽 기록이 작업 전과 같다.
  *The real 4th's graphics log on Windows matches the one before the task.*
- Linux 실제 4th가 두 폭에서 표면 점검을 지난다.
  *On both Linux widths the real 4th gets past the surface sweep.*
