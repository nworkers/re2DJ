# 작업 299 작업 로그 — 게임 상태 변수 탐색 스킬 / Task 299 work log — Game-state hunt skill

작업 지시: [20260918-299-game-state-hunt-skill.md](../work-orders/20260918-299-game-state-hunt-skill.md)

## 한국어

### 구현

| 파일 | 내용 |
| --- | --- |
| `SKILL.md` | 10단계 절차, 단계별 판정 기준과 함정, 빌드 성격에 따른 주의 |
| `scripts/dumplib.py` | 덤프·sidecar 읽기, 섹션, thunk 추적, 함수 시작 추정, 재동기화 역어셈블, 피연산자 기준 명령 복원 |
| `scripts/find_strings.py` | 문자열 검색과 참조 코드 |
| `scripts/xrefs.py` | 전역 참조의 write/read 분류 |
| `scripts/disasm.py` | 함수·구간 역어셈블 |
| `scripts/paired_writes.py` | 함수가 켰다 끄는 전역 수집(직접 쓰기와 one-line setter) |
| `scripts/callers.py` | thunk를 거친 호출까지 포함한 호출처와 인자 문맥 |
| `scripts/guest_memory.py` | 게스트 메모리 read/poll, `--yes`가 있어야 write |

작업 295에서 부딪힌 함정을 공용 모듈에 반영했다. Capstone은 해석할 수 없는 바이트에서 순회를 멈추므로 한 바이트씩 넘겨 재동기화한다. 피연산자 위치에서 짧게 물러나 해석하면 명령 중간을 다른 명령으로 읽으므로 긴 쪽부터 시도한다. 스크립트 이름은 표준 모듈(`dis`)과 겹치지 않게 했다.

### 검증 — 3rd 덤프에서 작업 295 결과 재현

| 단계 | 기대값 | 결과 |
| --- | --- | --- |
| `find_strings.py "demo\|autoplay\|attract" --xrefs` | `DEMOPLAY.bmp` 참조 1곳, 함수 `0x381ef` | 일치 |
| `xrefs.py 0x00a2946c` | 쓰기 2, 읽기 10 | 일치 (`total=12 write=2 read=10`) |
| `disasm.py 0x0048aa55 --function` | 함수 `0x8aa31`에서 플래그 1 쓰기 | 일치 |
| `paired_writes.py 0x0048aa55` | 데모 플래그와 autoplay 플래그가 짝 | 일치 — `PAIRED [0x00a2946c]`, `PAIRED [0x00a29508]` (setter `0x004353f0` 경유), `RESET` 세 곳 |
| `callers.py 0x004021a8` | getter 호출처 7곳 | 일치 — 5개 함수에 7곳 |
| `callers.py 0x00401a19 --context 3` | 데모 루틴의 1·0과 입력 매니저의 `1 - 값` | 일치 |

수작업으로 찾았던 autoplay 플래그가 `paired_writes.py` 한 번으로 나온다.

### 검증 — 다른 보호 빌드에서 실행

`ez2dj1st`, `ez2dj1stse`, `ez2dj4th`, `ez2dj5th`, `ez2d2m`의 `resumed` 덤프에 `find_strings.py "demo" --xrefs`를 돌려 오류 없이 실행됨을 확인했다. 관찰된 데모 단서는 3rd·4th·5th의 `DEMOPLAY.bmp`, 1st·1st SE의 `DemoGame` 계열 장면 이름, `ez2d2m`의 `DemoGame::OnCreateGame` 계열 클래스 메서드 이름이다. **탐색 출발점일 뿐 분석 결과가 아니며**, 스킬 3단계에 그렇게 표시했다.

`guest_memory.py write`는 대상 프로세스가 없을 때 종료 코드 1로 끝남을 확인했다. 실행 중 게임에 대한 read·poll·write는 작업 295·296에서 같은 방식으로 확인한 경로이며, 이번에는 다시 실행하지 않았다.

### 기타

* 사용자 요청에 따라 스킬을 `.claude/skills/`가 아니라 **`.agents/skills/`** 에 두었다. 특정 에이전트 전용 경로가 아니므로 여러 에이전트가 같은 스킬을 쓴다. 자동으로 발견되지 않는 에이전트도 있으므로 `AGENTS.md`에 스킬 위치와 목록을 적었다.
* 스크립트 실행 시 생기는 `__pycache__/`를 `.gitignore`에 추가했다.
* 코드·제품 빌드 변경 없음.

## English

### Implementation

`SKILL.md` holds the ten-step procedure with per-step criteria and pitfalls plus notes by build character. Under `scripts/`: `dumplib.py` (dump and sidecar reading, sections, thunk following, function-start heuristics, resynchronizing disassembly, operand-anchored instruction recovery), `find_strings.py` (strings and referencing code), `xrefs.py` (write/read classification of a global's references), `disasm.py` (function or window disassembly), `paired_writes.py` (globals a function turns on and off, through direct writes and one-line setters), `callers.py` (call sites including through thunks, with argument context), and `guest_memory.py` (guest memory read and poll, and write only with `--yes`).

The pitfalls from task 295 are built into the shared module: Capstone stops at undecodable bytes, so it resynchronizes one byte at a time; backing off a short distance from an operand decodes the middle of an instruction as something else, so the longest back-off is tried first; and no script name collides with a standard module such as `dis`.

### Verification — reproducing task 295 on the 3rd dump

`find_strings.py` found the single `DEMOPLAY.bmp` reference in function `0x381ef`; `xrefs.py 0x00a2946c` reported `total=12 write=2 read=10`; `disasm.py --function` showed function `0x8aa31` writing the flag to 1; `paired_writes.py` reported `PAIRED [0x00a2946c]` and `PAIRED [0x00a29508]` through setter `0x004353f0`, plus three `RESET` globals; `callers.py 0x004021a8` found the getter's 7 call sites in 5 functions; and `callers.py 0x00401a19 --context 3` showed the demo routine's 1 and 0 and the input manager's `1 - value`. All match. The autoplay flag found by hand comes out of a single `paired_writes.py` run.

### Verification — running on the other protected builds

`find_strings.py "demo" --xrefs` ran without error on the `resumed` dumps of `ez2dj1st`, `ez2dj1stse`, `ez2dj4th`, `ez2dj5th` and `ez2d2m`. The demo clues observed are `DEMOPLAY.bmp` in 3rd, 4th and 5th, `DemoGame`-family scene names in 1st and 1st SE, and `DemoGame::OnCreateGame`-family class method names in `ez2d2m`. **They are starting points, not analysis results**, and step 3 of the skill says so.

`guest_memory.py write` exits with code 1 when the target process is absent. Read, poll and write against a running game follow the path confirmed the same way in tasks 295 and 296 and were not rerun here.

### Other

* At the user's request the skill lives in **`.agents/skills/`** rather than `.claude/skills/`, a path not tied to one agent, so several agents share it. Not every agent discovers skills there automatically, so `AGENTS.md` now states the location and lists the skills.
* `__pycache__/`, written when the scripts run, was added to `.gitignore`.
* No code or product build change.
