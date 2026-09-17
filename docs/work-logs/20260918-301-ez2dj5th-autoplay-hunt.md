# 작업 301 작업 로그 — ez2dj5th autoplay 탐색 / Task 301 work log — ez2dj5th autoplay hunt

작업 지시: [20260918-301-ez2dj5th-autoplay-hunt.md](../work-orders/20260918-301-ez2dj5th-autoplay-hunt.md)
분석: [5th 데모 플레이와 autoplay 플래그](../analysis/ez2dj5th-demo-play.md)
절차: [`game-state-hunt` 스킬](../../.agents/skills/game-state-hunt/SKILL.md)

## 한국어

### 스킬 절차 적용 결과

| 단계 | 결과 |
| --- | --- |
| 1. 덤프 | 작업 294의 `resumed` 덤프, timestamp `0x3f53377b`, gaps 없음 |
| 2. 저장 설정 배제 | 설정 객체 `[0x00516350]`에 26개 키, `AutoPlay` 없음. 4th보다 `AutoScratch`·`AutoPedal`이 많음 |
| 3. 데모 단서 | `DEMOPLAY.bmp` 로드 조건 `[0x00aee194]` |
| 4. 데모 시작 루틴 | 쓰기 2곳(1·0)이 모두 `0x004aabf7`, 읽기 14곳 |
| 5. 짝 쓰기 | `PAIRED [0x00aee194]`, `PAIRED [0x00aee238]`(setter `0x00437790`), `RESET` 3곳 |
| 6. 판정 경로 | getter `0x00437780` 호출처 17곳이 4th의 노트 도착·종료·UI 좌표·슬롯 `0x1b` 토글과 대응. 첫 계열의 노트 데이터 필드는 `+0x1d8` |
| 7. 읽기 폴링 | 데모 구간 68.6~114.9초, 171.3초~에만 두 값이 함께 1 |
| 8·9. 쓰기 시험과 제품 연결 | 프로파일에 RVA `0x006ee238`·timestamp `0x3f53377b` 선언, 무장 확인, 사용자가 OSD로 autoplay 동작 확인 |

스킬 도구는 수정 없이 그대로 동작했다. 단계마다 한두 번의 명령으로 끝났고, 변경이 필요한 곳은 스킬 문서의 결과 표와 저장 키 수에 관한 문장뿐이었다.

### 새로 관찰한 것

* **`AutoScratch`·`AutoPedal` 저장 키.** 부분 자동 설정으로 추정하며 동작은 확인하지 않았다. 스킬 2단계에 "`Auto`로 시작하는 저장 키가 있어도 autoplay인지는 따로 확인한다"를 추가했다.
* **노트 데이터 필드 이동.** 첫 판정 계열에서 4th의 `+0x1cc`가 `+0x1d8`로 바뀌었다. getter 호출처를 오프셋으로 찾으면 빌드마다 달라질 수 있어 스킬 결과 표 아래에 적었다.

### 검증

* Windows x86 Debug 빌드: 오류·경고 0건.
* 단위 테스트: `checks: 1808, failures: 0`. 5th 값과 4th timestamp로 5th 주소가 무장되지 않음을 검사한다.
* 실행: `osd_controls`에서 `build_timestamp` = `executable_timestamp` = `0x3f53377b`, `autoplay_armed: true`.
* 사용자 확인: OSD로 autoplay를 켜 곡이 자동 연주됨. 데모 현상은 확인을 요청했으나 별도 언급은 없었다.

### 남은 것

* 5th가 값을 곡 시작 때 고정하는지, `AutoScratch`·`AutoPedal`의 동작.
* 구조가 다른 계열 — `ez2dj1st`·`ez2dj1stse`, `ez2d2m` — 과 덤프가 없는 `ez2dj2nd`, 덤프가 불가한 `ez2dj6th`.

## English

### Applying the skill

1. Dump: task 294's `resumed` dump, timestamp `0x3f53377b`, no gaps.
2. Stored setting ruled out: 26 keys on settings object `[0x00516350]`, no `AutoPlay`; `AutoScratch` and `AutoPedal` are new relative to 4th.
3. Demo clue: the `DEMOPLAY.bmp` load guard `[0x00aee194]`.
4. Demo start routine: both writes (1 and 0) in `0x004aabf7`; 14 reads.
5. Paired writes: `PAIRED [0x00aee194]`, `PAIRED [0x00aee238]` (setter `0x00437790`), three `RESET`.
6. Judgement path: the 17 sites of getter `0x00437780` correspond to 4th's note arrival, note end, UI coordinate and slot `0x1b` toggle; the first set's note-data field is at `+0x1d8`.
7. Read-only poll: both values 1 together only during demos, 68.6-114.9 s and from 171.3 s.
8-9. Write test and wiring: RVA `0x006ee238` and timestamp `0x3f53377b` declared, the control armed, and the user confirmed autoplay through the OSD.

The skill's tools worked unchanged. Each step took one or two commands, and the only edits were the skill's results table and its sentence on stored key counts.

### Newly observed

* **`AutoScratch` and `AutoPedal` stored keys**, inferred to be partial-auto settings, not exercised. Step 2 of the skill now says to check separately whether any stored key starting with `Auto` really is autoplay.
* **The note-data field moved** from 4th's `+0x1cc` to `+0x1d8` in the first judgement set. Finding getter sites by that offset can differ per build, noted under the skill's results table.

### Verification

* Windows x86 Debug build: no errors or warnings.
* Unit tests: `checks: 1808, failures: 0`, checking the 5th values and that the 4th timestamp does not arm the 5th address.
* Run: `osd_controls` shows `build_timestamp` = `executable_timestamp` = `0x3f53377b` and `autoplay_armed: true`.
* User check: turning autoplay on through the OSD made the song play itself. Demo effects were asked about but not separately mentioned.

### Remaining

* Whether 5th latches the value at song start, and what `AutoScratch` and `AutoPedal` do.
* The structurally different lines — `ez2dj1st`/`ez2dj1stse` and `ez2d2m` — plus `ez2dj2nd`, not yet dumped, and `ez2dj6th`, which cannot currently be dumped.
