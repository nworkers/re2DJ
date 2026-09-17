# 작업 301 작업 지시 — ez2dj5th autoplay 탐색 / Task 301 work order — ez2dj5th autoplay hunt

절차: [`game-state-hunt` 스킬](../../.agents/skills/game-state-hunt/SKILL.md)
선행: [작업 300 ez2dj4th](20260918-300-ez2dj4th-autoplay-hunt.md)
상태: **완료.** [작업 로그](../work-logs/20260918-301-ez2dj5th-autoplay-hunt.md)

## 한국어

`ez2dj5th`에서 autoplay 플래그를 찾고, 확인되면 빌드 한정 `game_controls`로 연결한다. 스킬의 10단계를 따르며 작업 300처럼 8·9단계는 프로파일 선언 뒤 사용자의 OSD 확인 한 번으로 합친다. 결과는 `docs/analysis/ez2dj5th-demo-play.md`에 기록한다. 주소는 덤프한 빌드의 timestamp에만 묶고 데모 플래그는 쓰지 않는다.

## English

Find `ez2dj5th`'s autoplay flag and, once confirmed, wire it in as a build-bound `game_controls` entry. Follow the skill's ten steps, merging steps 8 and 9 into a single OSD check by the user after the profile declaration as in task 300. Record results in `docs/analysis/ez2dj5th-demo-play.md`. The address is bound only to the dumped build's timestamp, and the demo flag is never written.
