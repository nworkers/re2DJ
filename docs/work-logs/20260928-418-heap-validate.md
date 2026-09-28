# 작업 418 작업 로그 — HeapValidate / Task 418 work log — HeapValidate

설계: [20260928-418-heap-validate.md](../design/20260928-418-heap-validate.md) · 지시서: [20260928-418-heap-validate.md](../work-orders/20260928-418-heap-validate.md)

## 2026-09-28

- 측정 결과는 설계에 적었다.
  *The measurements are in the design.*
- 결과: 실패 0.
  - Windows x86: CTest 6개 통과, 단위 4935 checks.
  - Linux x64·x86: CTest 4개 통과, 단위 4932 checks.

  *Results, no failures:*
  - *Windows x86: all 6 CTest tests pass, 4935 unit checks.*
  - *Linux x64 and x86: all 4 CTest tests pass, 4932 unit checks.*
- Linux 1st(x64·x86 모두):
  - 디버그 CRT의 `HeapValidate` 두 번이 1을 받는다.
  - 소리 스레드와 함께 `IDirectSoundBuffer::SetCurrentPosition`까지 간다.
  - 1171번째 호출 `user32!wsprintfA`에서 멈춘다.

  *Linux 1st (both x64 and x86):*
  - *The debug CRT's two `HeapValidate` calls get 1.*
  - *It runs on with the sound thread through `IDirectSoundBuffer::SetCurrentPosition`.*
  - *It stops at `user32!wsprintfA` (call 1171).*
- Windows 제품 코드는 바뀌지 않아 Windows 실행은 생략했다. 긴 회귀도 필요 없는 변경이라 생략했다.
  *No Windows product code changed, so no Windows run was made; the change needs no long regression either.*
