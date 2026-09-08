# 작업 지시서: 플립 체인 버퍼 유지

## 한국어

### 관련 설계

[플립 체인 버퍼 유지 설계](../design/20260909-236-flip-chain-retention.md)

### 작업 항목

1. `Sdl3OpenGlWindowConfig`에 버퍼 개수를 추가하고 기본값을 2로 둡니다.
2. render target을 색 텍스처 여러 개로 만들고, 생성 시 각각 한 번 검게 지웁니다.
3. `Draw`가 현재 back 텍스처를 framebuffer에 붙이고, 프레임 시작의 암묵적 색 clear를 제거합니다. 깊이 clear는 유지합니다.
4. `Present`가 현재 텍스처를 표시한 뒤 색인을 회전시킵니다.
5. 백엔드에 render target을 지정한 색으로 지우는 공개 함수를 추가합니다.
6. facade가 주 표면의 back buffer 개수를 백엔드 설정으로 전달합니다.
7. `DDBLT_COLORFILL`이 render target 표면을 대상으로 하면 백엔드 clear를 호출합니다.
8. Windows x86 build와 시험을 검증합니다.
9. StreetMix 진입 실행으로 eyecatch가 로딩 동안 유지되는지 캡처로 확인합니다.
10. 3rd·4th 회귀를 확인합니다.
11. 작업 로그를 작성합니다.

### 제외 범위

- Warning 페이드 첫 프레임의 표시 시간
- 게임플레이 배경 렌더링 문제
- 표면 사이 `Blt` 경로

### 완료 조건

- 로딩 구간에서 eyecatch 아트워크가 `1player`와 함께 화면에 남습니다.
- unit test, product loader probe, VFS runtime probe가 통과하고 3rd·4th에 회귀가 없습니다.

## English

### Related design

[Flip Chain Buffer Retention Design](../design/20260909-236-flip-chain-retention.md)

### Work items

1. Add a buffer count to `Sdl3OpenGlWindowConfig`, defaulting to 2.
2. Build the render target from several colour textures and clear each once at creation.
3. Have `Draw` attach the current back texture and drop the implicit colour clear at frame start, keeping the depth clear.
4. Have `Present` show the current texture and then rotate the index.
5. Add a backend entry point that clears the render target to a given colour.
6. Pass the primary surface's back-buffer count from the facade into the backend configuration.
7. Route a `DDBLT_COLORFILL` targeting the render-target surface to that backend clear.
8. Verify the Windows x86 build and tests.
9. Confirm by a StreetMix run and captures that the eyecatch stays up through loading.
10. Confirm no regression for 3rd and 4th.
11. Write the work log.

### Out of scope

- The on-screen time of the Warning fade's first frame
- The gameplay background rendering question
- The surface-to-surface `Blt` path

### Completion criteria

- The eyecatch artwork stays on screen with `1player` through the loading period.
- The unit tests, product-loader probe, and VFS runtime probe pass with no 3rd or 4th regression.
