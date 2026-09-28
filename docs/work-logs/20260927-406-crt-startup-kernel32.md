# 작업 406 작업 로그 — 1st CRT 시작의 kernel32 함수 / Task 406 work log — kernel32 functions of 1st's CRT startup

설계: [20260927-406-crt-startup-kernel32.md](../design/20260927-406-crt-startup-kernel32.md) · 지시서: [20260927-406-crt-startup-kernel32.md](../work-orders/20260927-406-crt-startup-kernel32.md)

## 2026-09-27

- 측정: 설계의 표와 같다. 첫 측정은 MSVC가 `printf` 인자를 오른쪽부터 평가해 `GetLastError()`가 호출보다 먼저 불렸다. 결과를 먼저 변수에 받도록 고쳐 다시 쟀다.
  *Measured as in the design's table. The first attempt read `GetLastError()` before the call, since MSVC evaluates `printf` arguments right to left; the probe was fixed to store each result first.*
- 결과: Windows x86 CTest 6개 통과(단위 4588 checks), Linux x64·x86 CTest 4개 통과(단위 4585 checks), 실패 0. 4th는 두 폭 모두 창을 닫을 때까지 돌았다.
  *Windows x86: all 6 CTest tests pass (4588 unit checks); Linux x64 and x86: all 4 CTest tests pass (4585 unit checks); no failures. 4th runs until the window closes on both widths.*
- Linux 1st(두 폭): CRT 시작을 지나 WinMain에서 창을 만들고, 호출 897번째 `user32!ShowWindow`에서 멈춘다.
  *Linux 1st (both widths) gets past its CRT startup, creates its window in WinMain, and stops at `user32!ShowWindow` (call 897).*
- 사용자 지시(2026-09-27): 오래 걸리는 실제 실행 회귀는 필요할 때만 하고, 일반적인 수정은 테스트 케이스로 확인한다. 이번 작업의 4th 회귀는 지시 전에 이미 돌렸다.
  *User direction: long real-run regressions only when needed, test cases otherwise. This task's 4th regression had already run before the direction.*
