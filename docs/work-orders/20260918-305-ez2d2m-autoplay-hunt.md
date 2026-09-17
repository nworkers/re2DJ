# 작업 305 작업 지시 — ez2d2m autoplay 탐색 / Task 305 work order — ez2d2m autoplay hunt

절차: [`game-state-hunt` 스킬](../../.agents/skills/game-state-hunt/SKILL.md)
선행: [작업 304 ez2dj1st](20260918-304-ez2dj1st-autoplay-hunt.md)
상태: **완료.** [작업 로그](../work-logs/20260918-305-ez2d2m-autoplay-hunt.md)

## 한국어

`ez2d2m`(EZ2Dancer 2nd MOVE)에서 autoplay에 해당하는 상태를 찾는다. 단서는 `DemoGame::OnCreateGame` 같은 클래스 메서드 이름이고 EZ2DJ 계열과 구조가 다르다. 확인되면 빌드 한정 `game_controls`로 연결하고, 표현할 수 없으면 연결하지 않고 근거를 기록한다. 게스트 메모리 쓰기는 사용자 동의 후에만 한다.

## English

Find the state that amounts to autoplay in `ez2d2m` (EZ2Dancer 2nd MOVE), whose clues are class method names such as `DemoGame::OnCreateGame` and whose structure differs from the EZ2DJ builds. Once confirmed, wire it in as a build-bound `game_controls` entry; if it cannot be expressed, wire nothing and record why. Guest memory is written only with the user's consent.
