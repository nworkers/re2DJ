# 작업 299 작업 지시 — 게임 상태 변수 탐색 스킬 / Task 299 work order — Game-state hunt skill

선행: [작업 295](20260917-295-autoplay-variable-hunt.md), [작업 296](20260917-296-autoplay-write-test.md), [작업 297](20260917-297-imgui-osd-autoplay.md)
상태: **완료.** [작업 로그](../work-logs/20260918-299-game-state-hunt-skill.md)

## 한국어

### 목적

`ez2dj3rd`에서 autoplay 플래그를 찾아 제품에 연결한 방식을 모든 버전에 반복할 수 있도록 여러 에이전트가 함께 쓰는 저장소 스킬로 만든다. 위치는 특정 도구 전용 경로가 아닌 `.agents/skills/`이며, `AGENTS.md`에서 안내한다.

### 범위

* `.agents/skills/game-state-hunt/SKILL.md` — 덤프, 저장 설정 배제, 데모 단서, 데모 시작 루틴, 짝 쓰기 수집, 판정 경로 확인, 읽기 폴링, 쓰기 시험, 프로파일 연결, 기록의 10단계와 함정.
* `.agents/skills/game-state-hunt/scripts/` — scratchpad에서 3rd 전용으로 쓰던 분석 스크립트를 빌드 무관하게 일반화한다. 덤프 sidecar의 image base를 읽고, 주소는 VA·RVA를 모두 받는다. 수작업이던 "짝으로 켜고 끄는 전역" 수집을 `paired_writes.py`로 자동화한다. 게스트 쓰기는 `--yes` 없이는 거부한다.
* 검증: 3rd 덤프로 작업 295의 결과를 각 스크립트가 재현하는지 확인하고, 나머지 보호 빌드 덤프에서 오류 없이 실행되는지 확인한다.

### 제약

* 제품 빌드에 의존성을 추가하지 않는다. Capstone(BSD-3-Clause)은 스크립트 실행에만 필요하다.
* 다른 버전의 실제 탐색은 이 작업에 포함하지 않는다.

## English

### Purpose

Turn the method that found `ez2dj3rd`'s autoplay flag and wired it into the product into a repository skill shared by every agent, so it can be repeated for every version. It lives in `.agents/skills/` rather than a tool-specific path and is pointed to from `AGENTS.md`.

### Scope

* `.agents/skills/game-state-hunt/SKILL.md` — ten steps with their pitfalls: dump, rule out a stored setting, follow the demo clue, find the demo start routine, collect paired writes, confirm the judgement path, poll read-only, test a write, wire the profile, and record.
* `.agents/skills/game-state-hunt/scripts/` — the analysis scripts used ad hoc for 3rd in the scratchpad, generalized to any build: they read the image base from the dump sidecar and accept VAs or RVAs. The manual step of collecting globals turned on and off in pairs is automated as `paired_writes.py`. Guest writes are refused without `--yes`.
* Verification: each script reproduces task 295's results on the 3rd dump, and runs without error on the other protected builds' dumps.

### Constraints

* No dependency is added to the product build; Capstone (BSD-3-Clause) is needed only to run the scripts.
* Actually hunting other versions is not part of this task.
