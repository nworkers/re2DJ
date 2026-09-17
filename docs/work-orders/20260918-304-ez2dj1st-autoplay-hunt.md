# 작업 304 작업 지시 — ez2dj1st autoplay 탐색 / Task 304 work order — ez2dj1st autoplay hunt

절차: [`game-state-hunt` 스킬](../../.agents/skills/game-state-hunt/SKILL.md)
선행: [작업 302 ez2dj1stse](20260918-302-ez2dj1stse-autoplay-hunt.md)
상태: **완료 — 변수 없음.** [작업 로그](../work-logs/20260918-304-ez2dj1st-autoplay-hunt.md)

## 한국어

`ez2dj1st`(1st Tracks)에서 autoplay에 해당하는 상태를 찾는다. 1st SE와 같은 장면 엔진 계열로 보고 스킬 3-1 경로부터 시작한다. 확인되면 빌드 한정 `game_controls`로 연결하고, 전환 가능한 변수가 없으면 연결하지 않고 그 근거를 분석 문서에 기록한다. 게스트 메모리 쓰기는 사용자 동의 후 곡 진행 중에만 한다.

## English

Find the state that amounts to autoplay in `ez2dj1st` (1st Tracks), starting from the skill's scene-engine path 3-1 on the expectation that it shares 1st SE's line. Once confirmed, wire it in as a build-bound `game_controls` entry; if there is no switchable variable, wire nothing and record the evidence in the analysis document. Guest memory is written only with the user's consent and only during a song.
