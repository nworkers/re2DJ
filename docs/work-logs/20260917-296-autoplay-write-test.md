# 작업 296 작업 로그 — autoplay 플래그 쓰기 시험 / Task 296 work log — Autoplay flag write test

작업 지시: [20260917-296-autoplay-write-test.md](../work-orders/20260917-296-autoplay-write-test.md)
선행: [작업 295](20260917-295-autoplay-variable-hunt.md)
분석: [EZ2DJ 3rd 데모 플레이와 설정 레지스트리](../analysis/ez2dj3rd-demo-play.md)

## 한국어

### 방법

`re2dj ez2dj3rd --io-config .\config\ez2dj-io.example.ini`로 실행해 사용자가 키보드로 코인·시작·곡 선택을 조작했다. scratchpad 스크립트가 실행 중인 `EZ2DJ.EXE`의 `[0x00a29508]`만 `WriteProcessMemory`로 썼고, 쓸 때마다 다시 읽어 확인했다. 데모 플래그 `[0x00a2946c]`는 쓰지 않았다. 판정은 사용자가 화면을 보고 했다.

### 결과

| 시각 | 시점 | 동작 | 읽은 값 | 사용자 관찰 |
| --- | --- | --- | --- | --- |
| 01:26:10 | 곡 선택 화면 | `on` | autoplay 0 → 1, demo 0 | — |
| 01:26:12 | 곡 선택 화면 | `status` | autoplay 1, demo 0 | — |
| 01:27:23 | 곡 플레이 중 | `status` | autoplay 1, demo 0 | **노트가 자동으로 맞았다** |
| 01:28:08 | 곡 플레이 중 | `off` | autoplay 1 → 0, demo 0 | — |
| 01:28:10 | 곡 플레이 중 | `status` | autoplay 0, demo 0 | **그 곡은 끝까지 자동으로 진행됐고, 다음 곡부터 수동이 됐다** |

### 판정

1. **`[0x00a29508]`은 autoplay 스위치다 — 확인됨.** 곡 선택 화면에서 1로 쓰고 곡을 시작하자 입력 없이 노트가 맞았다. 쓴 값은 플레이 중에도 유지됐고 데모 플래그는 0 그대로였다.
2. **값은 곡 시작 시점에 고정된다 — 확인됨.** 플레이 도중 0으로 써도 그 곡의 자동 판정은 멈추지 않았고 다음 곡부터 반영됐다. 따라서 곡 진행 중의 판정은 이 전역을 매번 읽는 것이 아니라 곡 시작 때 읽어 둔 값을 쓴다.
3. **보호 계층의 반응은 없었다 — 확인됨.** `.data` 쓰기 뒤에도 게임은 종료되지 않았다.
4. **데모 부작용은 없었다 — 확인됨(사용자 관찰).** DEMO PLAY 표시, 음소거, 입력 시 장면 종료가 보이지 않았다. 데모 플래그를 쓰지 않았으므로 작업 295의 해석과 일치한다.

### 작업 295 해석의 정정

작업 295는 `0x0004e2c2`가 노트가 도착할 때마다 getter를 불러 "게임이 직접 판정한다"고 보고, 그 값 하나로 판정이 켜지고 꺼진다고 **추정**했다. 결과 2는 그 추정과 맞지 않는다. getter를 노트마다 부른다면 도중에 끈 값이 곧바로 반영돼야 한다.

가능한 설명은 둘이다. `0x0004e2c2`·`0x0004e3a1`의 매 노트 호출은 누름·뗌 **연출**만 좌우하고 실제 판정은 곡 시작 때 고정된 값을 쓰거나, 곡 시작 경로의 호출처(`0x00039da7`의 `0x00402356`, `0x0004d5bb`·`0x0004d602`의 키별 가상 호출)가 판정 모드를 설정해 둔다. 어느 쪽인지는 **미확정**이다. 분석 문서 5·7절을 이에 맞게 고쳤다.

### 실무상 결론

* 강제 toggle은 **곡을 시작하기 전에** 써야 그 곡에 적용된다.
* 곡 선택 화면에서 쓴 값은 이후 곡에도 계속 유지된다. 일반 게임 경로는 이 값을 건드리지 않기 때문이다(작업 295). 다만 어트랙트로 돌아가 데모가 한 번 돌면 데모 종료가 0으로 되돌린다.

### 검증

* 코드 변경 없음. 저장소에 추가한 파일도 없다.
* 모든 주소는 TimeDateStamp `0x3bca98a3` 빌드에 한정된다.

## English

Work order: [20260917-296-autoplay-write-test.md](../work-orders/20260917-296-autoplay-write-test.md)
Prerequisite: [Task 295](20260917-295-autoplay-variable-hunt.md)
Analysis: [EZ2DJ 3rd demo play and the settings registry](../analysis/ez2dj3rd-demo-play.md)

### Method

The game ran as `re2dj ez2dj3rd --io-config .\config\ez2dj-io.example.ini`, and the user drove coin, start and song selection from the keyboard. A scratchpad script wrote only `[0x00a29508]` in the running `EZ2DJ.EXE` with `WriteProcessMemory`, reading the value back after every write. The demo flag `[0x00a2946c]` was never written. The user judged the outcome by watching the screen.

### Results

| Time | Point | Action | Values read | User observation |
| --- | --- | --- | --- | --- |
| 01:26:10 | Song select | `on` | autoplay 0 → 1, demo 0 | — |
| 01:26:12 | Song select | `status` | autoplay 1, demo 0 | — |
| 01:27:23 | During play | `status` | autoplay 1, demo 0 | **Notes were hit automatically** |
| 01:28:08 | During play | `off` | autoplay 1 → 0, demo 0 | — |
| 01:28:10 | During play | `status` | autoplay 0, demo 0 | **That song stayed automatic to the end; the next song was manual** |

### Decisions

1. **`[0x00a29508]` is the autoplay switch — confirmed.** Written to 1 at song select, the song that followed had its notes hit without input; the value held during play and the demo flag stayed 0.
2. **The value is latched when a song starts — confirmed.** Writing 0 mid-song did not stop that song's automatic hits and took effect from the next song, so in-song judgement does not reread this global each time but uses the value read at song start.
3. **The protection did not react — confirmed.** The game kept running after the `.data` write.
4. **No demo side effects — confirmed by user observation.** The DEMO PLAY overlay, muting and the scene ending on input did not appear, consistent with task 295's reading since the demo flag was never written.

### Correcting task 295's reading

Task 295 read `0x0004e2c2` calling the getter on every note arrival as "the game judges by itself" and **inferred** that this one value switches judgement on and off. Result 2 contradicts that: if the getter were consulted per note, turning it off mid-song would have applied at once.

Two explanations remain: either the per-note calls at `0x0004e2c2` and `0x0004e3a1` govern only the press and release **effects** while actual judgement uses a value latched at song start, or a call site on the song-start path (`0x00402356` from `0x00039da7`, or the per-key virtual calls at `0x0004d5bb` and `0x0004d602`) sets a judgement mode up front. Which one is **unresolved**. Sections 5 and 7 of the analysis document are corrected accordingly.

### Practical conclusions

* A forced toggle must be written **before a song starts** to apply to that song.
* A value written at song select persists into later songs, because the ordinary game path never touches it (task 295). Returning to attract and letting one demo run resets it to 0 when the demo ends.

### Verification

* No code change and no file added to the repository.
* Every address is specific to the TimeDateStamp `0x3bca98a3` build.
