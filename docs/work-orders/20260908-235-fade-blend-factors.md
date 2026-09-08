# 작업 지시서: 페이드 블렌드 인자 보존

## 한국어

### 관련 설계

[페이드 블렌드 인자 보존 설계](../design/20260908-235-fade-blend-factors.md)

### 작업 항목

1. `DeviceDrawPrimitive`에서 게스트가 지정한 `SRCBLEND`·`DESTBLEND`를 먼저 해석하고, 둘 다 해석되는지를 판정합니다.
2. `IsFullScreenBlackFadeCandidate`에 그 판정을 넘겨, 게스트가 인자를 지정한 경우에만 alpha `0xff`인 사각형을 후보로 허용합니다.
3. 호환 경로가 게스트의 인자를 그대로 쓰고, 해석되지 않을 때만 `SRCALPHA / INVSRCALPHA`로 대체하게 합니다.
4. Windows x86 build와 시험을 검증합니다.
5. 1st SE 부팅 구간을 캡처해 페이드 순서와 검은 프레임 유무를 확인합니다.
6. 3rd·4th 회귀를 확인합니다.
7. 작업 로그를 작성합니다.

### 제외 범위

- 프레임 201의 전체 밝기 draw
- 게임플레이 배경 렌더링 문제
- `ALPHABLENDENABLE`을 켜지 않는 게스트를 위한 일반 정책

### 완료 조건

- Warning이 밝아졌다가 어두워지는 순서로 페이드합니다.
- 페이드 시작 지점에 검은 프레임이 없습니다.
- unit test, product loader probe, VFS runtime probe가 통과하고 3rd·4th에 회귀가 없습니다.

## English

### Related design

[Preserving the Guest's Fade Blend Factors](../design/20260908-235-fade-blend-factors.md)

### Work items

1. In `DeviceDrawPrimitive`, decode the guest's `SRCBLEND` and `DESTBLEND` first and decide whether both decode.
2. Pass that decision into `IsFullScreenBlackFadeCandidate` so a quad with an alpha of `0xff` qualifies only when the guest named the factors.
3. Make the compatibility path use the guest's factors, falling back to `SRCALPHA / INVSRCALPHA` only when they do not decode.
4. Verify the Windows x86 build and tests.
5. Capture the 1st SE boot sequence and confirm the fade order and the absence of a black frame.
6. Confirm no regression for 3rd and 4th.
7. Write the work log.

### Out of scope

- The full-brightness draw at frame 201
- The gameplay background rendering question
- A general policy for a guest that never enables `ALPHABLENDENABLE`

### Completion criteria

- The Warning fades in and then out, in that order.
- No black frame at the start of a fade.
- The unit tests, product-loader probe, and VFS runtime probe pass with no 3rd or 4th regression.
