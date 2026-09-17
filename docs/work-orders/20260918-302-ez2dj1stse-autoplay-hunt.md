# 작업 302 작업 지시 — ez2dj1stse autoplay 탐색 / Task 302 work order — ez2dj1stse autoplay hunt

절차: [`game-state-hunt` 스킬](../../.agents/skills/game-state-hunt/SKILL.md)
선행: [작업 301 ez2dj5th](20260918-301-ez2dj5th-autoplay-hunt.md)
상태: **완료.** [작업 로그](../work-logs/20260918-302-ez2dj1stse-autoplay-hunt.md)

## 한국어

`ez2dj1stse`(CHD `.protect` 빌드)에서 autoplay에 해당하는 상태를 찾는다. 1st 계열은 섹션이 있는 Win32 profile INI를 쓰고 데모 단서가 `DemoGame` 같은 장면 이름이라 3rd~5th와 구조가 다를 것으로 본다. 스킬 절차를 따르되 도구가 맞지 않는 단계는 그 사실을 기록하고 방법을 조정한다. 확인되면 빌드 한정 `game_controls`로 연결하고, 구조가 달라 현재 `GameControls` 형태(32비트 플래그 하나)로 표현할 수 없으면 연결하지 않고 설계 필요성을 보고한다. 결과는 `docs/analysis/ez2dj1stse-demo-play.md`에 기록한다.

## English

Find the state that amounts to autoplay in `ez2dj1stse` (the CHD `.protect` build). The 1st line uses a sectioned Win32 profile INI and its demo clues are scene names such as `DemoGame`, so its structure is expected to differ from 3rd-5th. Follow the skill, recording any step whose tools do not fit and adjusting the method. Once confirmed, wire it in as a build-bound `game_controls` entry; if the structure cannot be expressed in today's `GameControls` shape (a single 32-bit flag), do not wire it and report the need for a design instead. Record results in `docs/analysis/ez2dj1stse-demo-play.md`.
