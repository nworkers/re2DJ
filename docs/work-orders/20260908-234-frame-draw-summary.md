# 작업 지시서: 프레임별 draw 요약 진단

## 한국어

### 관련 설계

[프레임별 draw 요약 진단 설계](../design/20260908-234-frame-draw-summary.md)

### 작업 항목

1. `RootFacade`에 프레임당 draw 호출 수, 정점 수, 텍스처가 묶인 draw 수를 누적할 항목을 둡니다.
2. 프레임의 첫 정점 색과 마지막 정점 색을 기록합니다. 전체 화면 페이드는 이 값을 조절해 이루어지므로, 프레임별 값의 나열이 게스트가 요청한 페이드 곡선입니다.
3. 모든 draw 진입점이 도달하는 지점에서 한 번만 세어 중복 계수를 피합니다.
4. `Flip`에서 요약 한 줄을 기록하고 카운터를 초기화합니다. 부팅 구간을 담을 자체 예산을 둡니다.
5. Windows x86 build와 시험을 검증합니다.
6. 3rd·4th 회귀를 확인합니다.

### 제외 범위

- 깜빡임의 수정
- draw별 정점 좌표나 텍스처 내용 기록
- 게임플레이 배경 렌더링 문제

### 완료 조건

- 부팅 구간의 프레임마다 `FrameDraws` 한 줄이 남습니다.
- 페이드 구간에서 draw 호출 유무와 색 변화가 판별됩니다.
- unit test, product loader probe, VFS runtime probe가 통과하고 3rd·4th에 회귀가 없습니다.

## English

### Related design

[Per-frame Draw Summary Diagnostic Design](../design/20260908-234-frame-draw-summary.md)

### Work items

1. Add per-frame accumulators to `RootFacade` for the draw-call count, the vertex count, and how many draws had a texture bound.
2. Record the frame's first and last vertex colour. A full-screen fade is performed by modulating this, so the sequence of these values is the fade curve the guest asked for.
3. Count at the single point every draw entry reaches, so no draw is counted twice.
4. Write one summary line at `Flip` and reset the counters, under a budget of its own sized for the boot sequence.
5. Verify the Windows x86 build and tests.
6. Confirm no regression for 3rd and 4th.

### Out of scope

- Fixing the flicker
- Recording per-draw vertex coordinates or texture content
- The gameplay background rendering question

### Completion criteria

- One `FrameDraws` line per frame across the boot sequence.
- The presence of draw calls and the colour change through the fade are decided.
- The unit tests, product-loader probe, and VFS runtime probe pass with no 3rd or 4th regression.
