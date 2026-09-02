# 복호화 영역 판별기 작업 지시

관련 설계: [복호화 영역 판별기](../design/20260902-137-decrypted-region-judge.md)

*Related design: [decrypted region judge](../design/20260902-137-decrypted-region-judge.md).*

## 범위

1. 플랫폼 중립 `re2dj::analysis::ScoreCodeRegion`을 추가합니다. 바이트 span 하나를 받아 Shannon 엔트로피, `55 8b ec` prologue 수, 길이 4 이상 `cc` run 수, zero byte 비율, 3상태 판정을 계산합니다.
2. 청크 분할 점수 함수를 추가합니다. 기본 청크는 `0x8000`입니다.
3. 오프라인 CLI `re2dj_code_score`를 추가합니다. 입력은 파일 경로, HDD 디렉터리 상대 경로, CHD 안의 게스트 경로 세 가지이며, PE로 읽히면 섹션 단위로, 아니면 바이트 범위로 점수를 냅니다.
4. 후보 판별 loop에서 쓸 수 있도록 `--require-code` 종료 코드 규약을 넣습니다.
5. unit test로 지표 정의와 판정 경계를 고정합니다. 합성 데이터만 사용하고 원본 자산에 의존하지 않습니다.
6. 원본 4th `EZ2DJ.EXE` 섹션 점수가 분석 문서 기록값을 재현하는지 확인합니다.
7. 워크스루 Stage 5·Stage 7을 실제 command로 갱신하고, 자동 판별기가 없다는 서술을 정정합니다.
8. `ARCHITECTURE.md`와 문서 색인을 갱신하고 작업 로그를 남깁니다.

*Add the platform-neutral `re2dj::analysis::ScoreCodeRegion` taking one byte span and computing Shannon entropy, `55 8b ec` prologue count, `cc` run count for runs of four or more, zero-byte share, and a three-state verdict; add a chunked scoring function defaulting to `0x8000`; add the offline `re2dj_code_score` CLI accepting a file path, an HDD-relative guest path, or a guest path inside a CHD, scoring by PE section when the bytes parse as a PE and by byte range otherwise; add a `--require-code` exit-code convention for the candidate judgement loop; pin the metric definitions and verdict boundaries with unit tests using synthetic data only, with no dependency on original assets; confirm the original 4th `EZ2DJ.EXE` section scores reproduce the analysis document's figures; update Stages 5 and 7 of the walkthrough with real commands and correct the statement that no automated judge exists; and update `ARCHITECTURE.md`, the document indexes, and the work log.*

## 비범위

- launcher probe 실행 중 게스트 메모리 판정. 다음 작업입니다.
- Function `0x0e` 변환 구현, seed 탐색, 응답 계산.
- 역어셈블 기반 명령어 유효성 검사.

*Out of scope: judging guest memory during a launcher probe run, which is the next task; implementing the Function `0x0e` transform, searching for seeds, or computing responses; and disassembly-based instruction validation.*

## 완료 조건

- 원본 자산 없이 저장소가 빌드되고 unit test가 통과합니다.
- 4th `.text` 점수가 엔트로피 `7.9967`, prologue 0회, `cc` run 0회를 재현합니다.
- `.rdata` `7.896`, `.data` `7.998`, `.protect` `7.969`를 재현합니다.
- 합성 x86 코드 구간이 `code-like`, 합성 난수 구간이 `ciphertext-like`로 판정됩니다.
- 판정 임계값이 확인된 사실이 아니라 휴리스틱임이 코드 주석과 문서에 모두 표기됩니다.
- 원본 바이트 열이 저장소에 들어가지 않습니다.

*Completion requires the repository to build and pass unit tests without original assets; 4th `.text` reproducing entropy `7.9967` with zero prologues and zero `cc` runs; `.rdata`, `.data`, and `.protect` reproducing `7.896`, `7.998`, and `7.969`; a synthetic x86 code region judged `code-like` and a synthetic random region judged `ciphertext-like`; the heuristic nature of the thresholds marked in both code comments and documents; and no original byte sequences entering the repository.*
