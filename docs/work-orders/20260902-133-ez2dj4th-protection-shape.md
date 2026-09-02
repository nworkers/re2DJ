# ez2dj4th 보호 형태 조사 작업 지시

관련 분석: [ez2dj4th Hardlock runtime](../analysis/ez2dj4th-hardlock-runtime.md)

*Related analysis: [ez2dj4th Hardlock runtime](../analysis/ez2dj4th-hardlock-runtime.md).*

## 배경

사용자 질문: Hardlock 경계를 통째로 건너뛰고 게임 진입점으로 직접 갈 수 있는가.

*User question: can the Hardlock boundary be skipped entirely to jump straight to the game entry point?*

## 범위

1. `0x450` 응답 6바이트 중 원본이 실제로 검증하는 바이트를 대조 실행으로 특정합니다.
2. 검증되는 바이트가 구조 검사인지 정확 일치인지 확인합니다.
3. 원본 이미지의 각 섹션이 평문인지 암호문인지 측정합니다.
4. 위 결과로 "보호 우회 후 진입점 직행"의 가능 여부를 판정하고 분석 문서와 작업 로그에 남깁니다.

*Identify which of the six `0x450` response bytes the original actually validates through controlled runs, determine whether that check is structural or an exact match, measure whether each section of the original image is plaintext or ciphertext, and use those results to decide whether jumping straight to the game entry point is possible, recording the outcome in the analysis document and a work log.*

## 비범위

- 코드 변경. 기존 launcher 옵션만 사용합니다.
- Function `0x0e` 알고리즘 복원.

*Out of scope: code changes, since only existing launcher options are used, and any reconstruction of the Function `0x0e` algorithm.*

## 완료 조건

- 검증되는 바이트 위치와 검사 성격이 재현 가능한 실행으로 확인됩니다.
- 섹션별 측정값이 기록됩니다.
- 진입점 직행 가능 여부에 대한 판정과 근거가 남습니다.

*Completion requires reproducible runs identifying the validated byte positions and the nature of the check, recorded per-section measurements, and a documented verdict with its basis on jumping directly to the entry point.*
