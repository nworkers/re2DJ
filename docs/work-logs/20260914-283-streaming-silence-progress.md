# 무음 스트리밍 진행 수정 / Streaming silence progress fix

## 한국어

새 로그 `011631-574`에서 track reset 적용과 무음 지속을 확인했습니다. 기존 원인
확정 주장을 정정했습니다. PCM 차이만 queue에 넣는 코드의 결함을 합성 테스트에서
재현했습니다. 수정 전 1초간 공급 요청에도 최초 8,820 frames에서 멈췄고 테스트가
실패했습니다. 소비량에 따라 원형 순서로 보충한 후 같은 테스트가 통과했으며 후속
tone이 cooked PCM 출력에 도달했습니다.

테스트는 실제 backend를 dummy audio device로 실행하며 원본 자산을 사용하지 않습니다.
queue 크기, 반복 Play continuation, 정지 cursor, 재시작 누적량도 검사합니다.
전체 VFS GUI probe의 기존 대기는 이번 회귀의 전제 조건으로 사용하지 않습니다.

Windows x86 Debug 빌드와 신규 오디오 테스트 및 기존 단위/입력/loader 4개 테스트를
수행했습니다. 실제 JAM 청취 성공은 아직 확인하지 않았습니다. cooked 로그는 초기
16회 이후 100 callback마다, 최대 36,000 callback까지 관측하도록 변경했습니다.

## English

Run `011631-574` confirms track reset is applied but silence persists; the prior confirmed
cause claim is corrected. A synthetic test reproduces the value-only queue producer defect:
before the fix, one second of commits stalls at the initial 8,820 frames and fails.
Consumption-based circular replenishment passes the same test and delivers a following
tone to cooked PCM output.

The test runs the actual backend using a dummy audio device without original assets.
It also checks queue bounds, repeated Play continuation, stopped cursor and restart
accounting. The existing full VFS GUI probe wait is not a prerequisite for this regression.

Built Windows x86 Debug and ran the new audio regression plus four existing unit/input/
loader tests. Real JAM listening success remains unverified. Cooked tracing now samples
every 100 callbacks after the first 16, up to 36,000 callbacks.
