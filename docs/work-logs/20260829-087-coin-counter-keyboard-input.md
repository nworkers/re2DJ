# 키보드 코인 누적 카운터 수정 작업 로그

관련 설계: [키보드 코인 누적 카운터 수정](../design/20260829-087-coin-counter-keyboard-input.md)

관련 작업 지시: [키보드 코인 누적 카운터 수정 작업 지시](../work-orders/20260829-087-coin-counter-keyboard-input.md)

관련 분석: [EZ2DJ I/O 포트 맵](../analysis/ez2dj-io-map.md)

## 한국어

### 원인

사용자 실제 실행에서 F3 한 번이 credit 99를 만들었다. `Ez2DjIoBoard::SetButton`은 false→true edge를 정상 보존했지만, port `0x105`가 한 read 동안 `0xfe`를 반환하고 다음 read에 `0xff`로 복귀했다.

대응 unprotected binary를 읽기 전용으로 재분석했다. VA `0x004175c5`부터 `0x105`를 읽고, VA `0x0041764c`에서 `current - previous`를 계산하며 음수이면 256을 더한다. VA `0x00417675`부터 이 delta를 credit 관련 전역에 누적한다. 따라서 되돌아가는 pulse가 추가 delta를 만든 것이 직접 원인이다.

### 구현

- consume-on-read `coin_pending_`을 stable `std::uint8_t coin_counter_`로 교체했다.
- coin false→true 전이마다 counter를 modulo-256으로 1 증가시킨다.
- port `0x105` read는 현재 counter를 반환하며 상태를 변경하지 않는다.
- 초기 idle counter는 `0x00`이고 전체 idle bytes는 `ff ff 80 80 00 ff`다.
- unit test는 초기값, hold 중 반복 read, release, 다음 press와 255→0 wrap을 검사한다.
- 작업 085 설계·로그, architecture와 누적 I/O 분석의 one-read pulse 설명을 정정했다.

### 검증

- 표준 `build/windows-x86` 전체 warnings-as-errors build 통과
- CTest 3/3 통과
  - `re2dj_windows_vfs_runtime_probe`
  - `re2dj_windows_product_loader_probe`
  - `re2dj_unit_tests`
- 잠금 해제 뒤 표준 runtime DLL link가 정상 동작함을 확인했다.
- 임시 `build/windows-x86-task086`, `build/windows-x86-task087` 산출물은 사용자 요청에 따라 삭제했다. 재생성 가능한 build output이며 복구가 필요하지 않다.

실제 F3 press/release 한 번당 credit 1 증가 여부는 사용자의 다음 실행 확인으로 남긴다.

## English

### Cause

In the user's real run, one F3 press produced 99 credits. `Ez2DjIoBoard::SetButton` correctly retained the false-to-true edge, but port `0x105` returned `0xfe` for one read and then returned to `0xff`. Read-only reanalysis of the corresponding unprotected binary confirms a read starting at VA `0x004175c5`, `current - previous` at VA `0x0041764c`, addition of 256 when negative, and accumulation of that delta into credit-related globals from VA `0x00417675`. The returning pulse directly created another counter delta.

### Implementation

Replaced consume-on-read `coin_pending_` with stable `std::uint8_t coin_counter_`. Each coin false-to-true transition increments the counter once modulo 256, while reads return it without mutation. Initial counter is `0x00`, yielding idle bytes `ff ff 80 80 00 ff`. Unit coverage now checks initial state, repeated held reads, release, next press, and 255-to-zero wrap. Task 085 design/log, architecture, and cumulative I/O analysis were corrected.

### Verification

The standard `build/windows-x86` complete warnings-as-errors build passes, followed by CTest 3/3. The standard runtime DLL links normally after lock release. At the user's request, regenerable temporary outputs `build/windows-x86-task086` and `build/windows-x86-task087` were deleted. User verification that each F3 press/release cycle adds exactly one credit remains pending.
