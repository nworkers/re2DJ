# 작업 300 작업 지시 — ez2dj4th autoplay 탐색 / Task 300 work order — ez2dj4th autoplay hunt

절차: [`game-state-hunt` 스킬](../../.agents/skills/game-state-hunt/SKILL.md)
선행: [작업 295](20260917-295-autoplay-variable-hunt.md), [작업 299](20260918-299-game-state-hunt-skill.md)
상태: **완료.** [작업 로그](../work-logs/20260918-300-ez2dj4th-autoplay-hunt.md)

## 한국어

### 목적

`ez2dj4th`에서 autoplay 플래그를 찾고, 확인되면 빌드 한정 `game_controls`로 연결해 OSD 토글이 나타나게 한다.

### 범위

스킬의 10단계를 따른다. 정적 분석과 읽기 폴링은 에이전트가 하고, 쓰기 시험과 OSD 확인은 사용자가 게임을 조작하는 동안 사용자 동의 아래 한다. 결과는 `docs/analysis/ez2dj4th-demo-play.md`에 확인됨/추정/미확정으로 기록한다.

### 제약

* 주소는 이번에 덤프한 4th 빌드의 timestamp에만 묶는다.
* 데모 플래그는 쓰지 않는다.
* 스킬의 도구로 부족한 점이 드러나면 이 작업 로그에 기록하고, 스킬 개선은 필요한 만큼만 함께 한다.

## English

### Purpose

Find `ez2dj4th`'s autoplay flag and, once confirmed, wire it in as a build-bound `game_controls` entry so the OSD toggle appears.

### Scope

Follow the skill's ten steps. The agent does static analysis and read-only polling; the write test and OSD check happen with the user's agreement while the user drives the game. Results go into `docs/analysis/ez2dj4th-demo-play.md` as confirmed, inferred or unresolved.

### Constraints

* The address is bound only to the timestamp of the 4th build dumped here.
* The demo flag is never written.
* Any shortfall of the skill's tools is recorded in this task's log, with only as much skill improvement as the task needs.
