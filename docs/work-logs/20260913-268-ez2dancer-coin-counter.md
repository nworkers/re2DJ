# 작업 로그: EZ2Dancer coin counter 입력 수정
# Work Log: EZ2Dancer Coin Counter Input Fix

## 한국어

### 결과

이전의 `0x304` bit 0 held-level mapping을 제거하고, `coin=F5` 입력을 rising-edge
counter로 변경했습니다. 이제 key down 시 counter가 1 증가하고, key를 계속 누르고
있거나 떼는 동안에는 중복 증가하지 않습니다. 다시 누르면 다음 counter 값이 됩니다.

### 원인과 변경

- 실제 실행에서 이전 `0x0001` held-level mapping이 credit으로 반영되지 않았습니다.
- `Ez2DancerIoBoard`가 `kCoin`의 released → pressed 전이를 감지해 16비트
  `coin_counter_`를 증가시키도록 변경했습니다.
- `0x304` read는 held bit가 아니라 현재 counter를 반환합니다.
- `--io-config`가 실제로 keyboard adapter까지 도달했는지 확인하도록 VFS 로그에
  `re2dj:vfs:io-config:profile=ez2dancer:status=initialized` 이벤트를 추가했습니다.
- 부팅 초반의 256건 I/O trace budget 이후에도 counter 변화는
  `re2dj:vfs:io-coin:previous=...:value=...`로 남깁니다.
- 공개 구현과 원본 실행에서 `0x304`를 읽는 사실은 참고했지만, 실제 cabinet
  register 의미는 여전히 **미확정**으로 유지했습니다.

### 검증

- Windows x86 Debug build: 성공, warnings 0, errors 0.
- `re2dj_unit_tests`: 1708 checks, failures 0.
- `re2dj_ez2dancer_keyboard_input_test`: 8 checks, failures 0.
- 관련 CTest (`-C Debug`): 2/2 passed.
- 실제 CHD 실행에서 설정 초기화 이벤트가 기록되는 것을 확인했습니다.
- 자동 입력 주입은 `GetAsyncKeyState`에 반영되지 않아, 이 환경에서 실제 키보드의
  F5 credit 전이까지는 자동 확정하지 못했습니다. 사용자가 새 DLL로 실행 후 F5를
  눌렀을 때 `io-coin` 이벤트와 credit 증가를 확인할 수 있습니다.

## English

### Result

Replaced the previous `0x304` bit-0 held-level mapping with a rising-edge counter for
the `coin=F5` input. A key-down transition increments the counter; holding or releasing
the key does not repeat the increment, and pressing it again produces the next value.

### Cause and changes

- The previous `0x0001` held-level mapping did not become a credit in the real run.
- `Ez2DancerIoBoard` now detects the released-to-pressed transition of `kCoin` and
  increments a 16-bit `coin_counter_`.
- Reads from `0x304` return the current counter rather than a held bit.
- The VFS log now records
  `re2dj:vfs:io-config:profile=ez2dancer:status=initialized`, proving that
  `--io-config` reached the keyboard adapter.
- Counter changes remain visible after the first 256 I/O trace records as
  `re2dj:vfs:io-coin:previous=...:value=...`.
- The public implementation and original execution confirm the `0x304` read as useful
  context, but the real cabinet register meaning remains **unresolved**.

### Verification

- Windows x86 Debug build: passed with 0 warnings and 0 errors.
- `re2dj_unit_tests`: 1708 checks, 0 failures.
- `re2dj_ez2dancer_keyboard_input_test`: 8 checks, 0 failures.
- Related CTest with `-C Debug`: 2/2 passed.
- A real CHD run recorded the configuration initialization event.
- Synthetic input injection was not reflected by `GetAsyncKeyState`, so the real F5-to-
  credit transition could not be automated in this environment. A user run with the new
  DLL can verify the `io-coin` event and the credit increase.
