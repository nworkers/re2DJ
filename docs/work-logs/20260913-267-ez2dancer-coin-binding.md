# 작업 로그: EZ2Dancer coin 입력 바인딩
# Work Log: EZ2Dancer Coin Input Binding

## 한국어

### 결과

`ez2d2m`의 EZ2Dancer 예제 I/O 설정에 coin 키를 추가하고, 설정이 실제 word-wide
입력 board까지 전달되도록 연결했습니다. `coin=F5`를 누르고 있는 동안
`0x304` read가 `0x0001`을 반환하고, 떼면 관찰된 idle 값 `0x0000`으로 돌아갑니다.

### 변경 사항

- `Ez2DancerButton::kCoin`과 `Ez2DancerKeyboardInput`의 `coin` binding을 추가했습니다.
- `Ez2DancerIoBoard`에서 `0x304` bit 0을 coin 호환 입력으로 모델링했습니다.
- `config/ez2dancer-io.example.ini`에 `coin=F5`를 추가했습니다.
- idle/pressed/released board 회귀 테스트와 최소 keyboard INI를 갱신했습니다.
- README, ARCHITECTURE, EZ2Dancer I/O 분석 문서에 확인 상태를 반영했습니다.

원본 실행에서 `0x304` read는 확인되었지만 coin의 실제 port·bit·극성은 여전히
미확정입니다. 따라서 bit 0 active-high 동작은 원본 복원이 아니라 명시적인 추정
호환 mapping으로 기록했습니다.

### 검증

- 관련 실행 파일 테스트: `re2dj_unit_tests` — 1700 checks, failures 0
- 관련 실행 파일 테스트: `re2dj_ez2dancer_keyboard_input_test` — 8 checks, failures 0
- 전체 Windows x86 Debug build: 성공, warnings 0, errors 0
- 관련 CTest:
  `re2dj_ez2dancer_keyboard_input_test`, `re2dj_unit_tests` — 2/2 passed
- 전체 CTest는 첫 번째 `re2dj_windows_vfs_runtime_probe`가 실행 인자 없이 장시간
  대기하여 중단했습니다. 이는 이번 변경의 빌드·단위 테스트 실패가 아닙니다.

### 다음 확인

실제 `ez2d2m` 실행에서 F5를 눌러 credit이 증가하는지 확인해야 합니다. 증가하지
않으면 원본 input helper의 port read 관측을 새로 확보해 추정 mapping을 교체합니다.

## English

### Result

Added a coin key to the EZ2Dancer example I/O configuration for `ez2d2m` and connected
it through the word-wide input board. While `coin=F5` is held, a `0x304` read returns
`0x0001`; releasing it restores the observed idle value `0x0000`.

### Changes

- Added `Ez2DancerButton::kCoin` and the `coin` binding in `Ez2DancerKeyboardInput`.
- Modelled bit 0 of `0x304` as the coin compatibility input in `Ez2DancerIoBoard`.
- Added `coin=F5` to `config/ez2dancer-io.example.ini`.
- Updated board pressed/released regression coverage and the minimal keyboard INI.
- Updated README, ARCHITECTURE, and the EZ2Dancer I/O analysis with confirmation status.

The original run confirms reads from `0x304`, but the real coin port, bit, and polarity
remain unresolved. The active-high bit-0 behaviour is therefore documented as an
explicit inferred compatibility mapping, not as original executable recovery.

### Verification

- Related executable test: `re2dj_unit_tests` — 1700 checks, 0 failures
- Related executable test: `re2dj_ez2dancer_keyboard_input_test` — 8 checks, 0 failures
- Full Windows x86 Debug build: passed with 0 warnings and 0 errors
- Related CTest:
  `re2dj_ez2dancer_keyboard_input_test`, `re2dj_unit_tests` — 2/2 passed
- The full CTest run was stopped because the first `re2dj_windows_vfs_runtime_probe`
  waits for a long-running no-argument probe. This is not a build or unit-test failure
  for this change.

### Next check

Run `ez2d2m` and press F5 to verify that credit increases. If it does not, capture the
original input helper's port reads and replace the inferred mapping.
