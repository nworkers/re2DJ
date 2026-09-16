# 작업 295 작업 지시 — autoplay 변수 탐색 / Task 295 work order — Hunting the autoplay variable

선행: [작업 292 복호화된 주 이미지 덤프](20260916-292-decrypted-image-dump.md), [작업 294](20260917-294-port-helper-dump-crosscheck.md)

상태: **완료.** autoplay 플래그 `0x00a29508` 확인, 토글 입력의 물리 바인딩은 미확정. [작업 로그](../work-logs/20260917-295-autoplay-variable-hunt.md)

## 한국어

### 목적

어트랙트 데모는 게임이 스스로 노트를 친다. 사용자는 그 상태를 임의로 켜는 방법을 추후 고려하고 있다. 이 작업은 **그 상태가 어디에 있는지 찾는 조사**이며, 강제 toggle 구현은 결과를 본 뒤 따로 판단한다.

### 범위

1. 3rd `resumed` 덤프에서 INI 키 문자열을 참조하는 코드로 설정 구조를 찾는다.
2. 데모 플레이 경로와 그 상태 변수를 찾는다.
3. 찾은 변수를 게임 실행 중 **읽기 전용**으로 관찰해 확인한다.
4. 노트 자동 판정 분기를 추적한다.
5. 결과를 `docs/analysis/`에 확인됨/추정/미확정으로 기록한다.

### 제약

* **관찰만 한다.** 게스트 메모리에 쓰지 않는다.
* 분석 스크립트는 scratchpad에 두고 저장소에 넣지 않는다. 저장소에 디스어셈블러를 추가하지 않는다(작업 293의 범위 제외를 따름).
* 문서에는 주소, 구조, 명령 형태, 관찰된 동작만 적고 바이트 덤프를 적지 않는다.

### 범위에서 뺀 것

* 강제 toggle 구현.
* 3rd 외 타깃.

## English

Prerequisites: [Task 292, decrypted main-image dump](20260916-292-decrypted-image-dump.md), [Task 294](20260917-294-port-helper-dump-crosscheck.md)

Status: **complete.** Autoplay flag `0x00a29508` confirmed; the toggle input's physical binding is unresolved. [Work log](../work-logs/20260917-295-autoplay-variable-hunt.md)

### Purpose

In the attract demo the game hits notes by itself, and the user is considering a way to switch that state on at will. This task is **the investigation of where that state lives**; implementing a forced toggle is weighed separately once the results are in.

### Scope

1. Find the settings structure in 3rd's `resumed` dump through the code that references the INI key strings.
2. Find the demo-play path and its state variable.
3. Confirm that variable by observing it **read-only** while the game runs.
4. Trace the note auto-hit branch.
5. Record the results in `docs/analysis/` as confirmed, inferred or unresolved.

### Constraints

* **Observe only**: nothing is written to guest memory.
* Analysis scripts stay in the scratchpad, out of the repository, and no disassembler is added to the repository, following task 293's exclusion.
* Documents record addresses, structure, instruction shapes and observed behavior, never byte dumps.

### Out of scope

* Implementing a forced toggle.
* Targets other than 3rd.
