# #15 작업 지시서 — ez2dj3rd의 창 모드 DirectDraw / #15 work order — ez2dj3rd's windowed DirectDraw

이슈: [#15](https://github.com/reexec/re2DJ/issues/15) · 설계: [20261010-i015-ez2dj3rd-windowed-ddraw.md](../design/20261010-i015-ez2dj3rd-windowed-ddraw.md)

## 절차 / Steps

1. user32 `GetClientRect`·`ClientToScreen` 구현, resolve-only 목록에서 제거, 단위 테스트.
   *Implement user32 `GetClientRect` and `ClientToScreen`, drop them from the resolve-only list, and test them.*
2. `ddraw_clipper.cpp`: `IDirectDrawClipper`, `CreateClipper`(DX7·DX6), 서피스 `SetClipper`·`GetClipper`, 단위 테스트.
   *`ddraw_clipper.cpp`: `IDirectDrawClipper`, `CreateClipper` (DX7 and DX6), surface `SetClipper` and `GetClipper`, with unit tests.*
3. 3rd를 다시 실행해 다음 멈춤이 있으면 같은 방식으로 채운다.
   *Run 3rd again and fill any next stop the same way.*
4. 검증과 문서(README 실행 가능 타깃, IMPLEMENTED).
   *Verification and documents (the README's runnable targets, IMPLEMENTED).*

## 완료 조건 / Done when

3rd가 창을 닫을 때까지 실행되고, 단위 테스트와 모든 타깃 빌드가 통과한다.

*3rd runs until its window is closed, and the unit tests and every target's build pass.*
