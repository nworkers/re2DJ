# 작업 441 설계 — 턴테이블 step 기본값 2 / Task 441 design — a default turntable step of 2

선행: [작업 306 설계](20260918-306-builtin-io-mapping.md)

## 배경 / Background

사용자가 EZ2DJ 턴테이블 `step`의 기본값을 2로 바꿔 달라고 요청했다. 내장 기본값 `kEz2DjDefaultTurntableStep`은 4였다. 그런데 "내장 기본값과 같다"고 적힌 `config/ez2dj-io.example.ini`는 이전 커밋 `eab1c37`부터 `step=2`였다. 그래서 두 값이 어긋나 있었고, 이를 대조하는 Windows 단위 테스트(`ez2dj_keyboard_input_test.cpp`)도 실패할 상태였다. Linux host는 `--io-config`의 `step`을 읽지 않고 기본값만 쓴다.

*The user asked for a default EZ2DJ turntable `step` of 2. The built-in `kEz2DjDefaultTurntableStep` was 4, while `config/ez2dj-io.example.ini`, which says it lists the built-in defaults, has read `step=2` since commit `eab1c37`; the two disagreed, and the Windows unit test comparing them (`ez2dj_keyboard_input_test.cpp`) would fail. The Linux host takes no `step` from `--io-config` and uses the default alone.*

## 결정 / Decisions

- `kEz2DjDefaultTurntableStep`을 2로 바꾼다. 두 host가 같은 상수를 쓰므로 Windows의 INI 없는 실행과 Linux 실행 모두 2가 된다. 턴테이블 위치는 키를 누른 동안 8 ms마다 2씩 움직인다.
  *`kEz2DjDefaultTurntableStep` becomes 2. Both hosts use the constant, so a Windows run without an INI and every Linux run get 2; the turntable position moves by 2 every 8 ms while a key is held.*
- 공용 단위 테스트(`ez2dj_keyboard_map_test.cpp`)가 예제 INI의 `step`도 기본값과 대조한다. 그래서 Linux에서도 둘이 어긋나면 드러난다.
  *The shared unit test (`ez2dj_keyboard_map_test.cpp`) also compares the example INI's `step` with the default, so a mismatch shows on Linux too.*

## 검증 / Verification

Linux x64 build와 CTest(경고를 오류로). Windows build는 이 환경에 MSVC가 없어 하지 못한다.

*The Linux x64 build and CTest with warnings as errors; the Windows build cannot run here without MSVC.*
