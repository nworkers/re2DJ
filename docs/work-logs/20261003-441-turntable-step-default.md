# 작업 441 작업 로그 — 턴테이블 step 기본값 2 / Task 441 work log — a default turntable step of 2

설계: [20261003-441-turntable-step-default.md](../design/20261003-441-turntable-step-default.md) · 지시서: [20261003-441-turntable-step-default.md](../work-orders/20261003-441-turntable-step-default.md)

## 2026-10-03

- `include/re2dj/input/ez2dj_keyboard_map.h`: `kEz2DjDefaultTurntableStep` 4 → 2. 예제 INI(`step=2`, `eab1c37`부터)와 일치한다.
  *`kEz2DjDefaultTurntableStep` 4 → 2 in `include/re2dj/input/ez2dj_keyboard_map.h`, matching the example INI (`step=2` since `eab1c37`).*
- `tests/unit/ez2dj_keyboard_map_test.cpp`의 `CheckDefaults`가 예제 INI의 `turntables.step`을 기본값과 대조한다.
  *`CheckDefaults` in `tests/unit/ez2dj_keyboard_map_test.cpp` compares the example INI's `turntables.step` with the default.*
- 작업 085·306 설계에 바뀐 기본값을 적었다.
  *Tasks 085 and 306's designs note the new default.*
- **검증**: `scripts/test_all.sh linux-x64-debug`(경고를 오류로) build 성공, CTest 4개 통과. 이 환경에는 MSVC가 없어 Windows build와 `ez2dj_keyboard_input_test.cpp`를 돌리지 못했다. 그 테스트는 이제 기본값과 예제가 같아 통과할 것으로 **추정**한다.
  *Verification: `scripts/test_all.sh linux-x64-debug` (warnings as errors) builds and passes 4 CTest tests. With no MSVC here the Windows build and `ez2dj_keyboard_input_test.cpp` did not run; that test is **inferred** to pass now that the default and the example agree.*
