# 작업 302 작업 로그 — ez2dj1stse autoplay 탐색 / Task 302 work log — ez2dj1stse autoplay hunt

작업 지시: [20260918-302-ez2dj1stse-autoplay-hunt.md](../work-orders/20260918-302-ez2dj1stse-autoplay-hunt.md)
분석: [1st SE 자동 플레이 장면과 autoplay 플래그](../analysis/ez2dj1stse-demo-play.md)
절차: [`game-state-hunt` 스킬](../../.agents/skills/game-state-hunt/SKILL.md)

## 한국어

### 스킬이 맞은 곳과 맞지 않은 곳

| 단계 | 결과 |
| --- | --- |
| 1. 덤프 | 작업 294의 `resumed` 덤프, timestamp `0x3862df27`, gaps 없음 |
| 3. 데모 단서 | `DemoGame`, `ShowDemoPlay`, `ClubMixDemoGame` 문자열. 참조 코드가 조건 전역이 아니라 **장면 등록 함수 `0x00423670`의 인자** |
| 4·5. 데모 루틴과 짝 쓰기 | **맞지 않음.** 켜고 끄는 짝이 한 함수가 아니라 장면의 초기화·종료 콜백 두 함수에 나뉜다 |
| 대체 경로 | 장면 표를 뽑고(60개), `DemoGame` 초기화 콜백을 읽어 상수를 쓰는 전역 두 개(`[0x01c3f3a0] = 2`, `[0x01c3f3a4] = 1`)를 찾음. 전자는 쓰기 31곳의 모드 값, 후자는 자동 플레이 세 장면의 초기화에서만 1이고 종료에서 0 |
| 6. 판정 경로 | 읽기 13곳 — 곡 재생기 시작 인자, 플레이어 장면 자동·수동 분기, 입력 처리의 키음 재생·정지 |
| 7. 읽기 폴링 | 자동 플레이 구간 60.6~96.4초, 103.1~136.7초에만 1. 두 번째 구간에서 mode도 2 |
| 8·9. 쓰기 시험과 제품 연결 | RVA `0x0183f3a4`·timestamp `0x3862df27` 선언, 무장 확인, 사용자가 OSD로 autoplay 동작 확인 |

### 스킬 개선

* `register_calls.py` 추가 — 등록 함수 호출마다 인자를 호출 순서로 나열하고 문자열을 풀어 준다. 1st SE에서 60개 장면과 각 콜백·핸들을 한 번에 얻었고, 세 번째·네 번째 콜백이 autoplay 쓰기 함수와 정확히 일치함을 확인했다.
* `SKILL.md`에 **3-1 장면 엔진 경로**를 추가했다. 단서가 장면 이름일 때의 절차, 판정 기준, `paired_writes.py`가 잡지 못하는 이유를 적었다.
* 처음에는 인자 순서를 거꾸로 풀어 표가 어긋났다. 마지막 `push`가 첫 인자라는 점을 도구에 반영했다.

### 별도 문제 — 효과음 무한 반복

사용자 확인 중 동전 효과음 같은 소리가 끝없이 반복됐다. 처음에는 "autoplay가 입력 처리의 소리 정지 호출을 건너뛰기 때문"이라고 **추정**했으나, 사용자가 **autoplay와 관계없이 재현된다**고 확인해 기각했다.

`--audio-volume-trace`로 다시 실행해 확인한 사실은 다음과 같다.

* 1st SE는 일회성 효과음 버퍼를 `0x000140e2`(`DSBCAPS_STATIC | GETCURRENTPOSITION2` 등)로 만든다.
* DirectSound HLE의 `is_streaming()`은 `LOCHARDWARE` 또는 `GETCURRENTPOSITION2`가 있거나 크기가 360,448이면 스트리밍으로 보므로, 이 효과음들이 모두 `is_streaming=1`이다.
* 로그 말미에 약 126초 동안 이어진 스트리밍 트랙(`sequence=6300`)이 있다.

일회성 버퍼가 스트리밍 링 경로에서 끝나지 않고 다시 도는 것이 원인으로 **추정**되며, 로그가 4,096줄에서 잘려 반복 중인 버퍼를 직접 특정하지는 못했다. 판정 규칙은 4th·`ez2d2m` JAM 스트리밍을 위해 들어간 것이므로, 사용자와 합의해 **별도 작업으로 설계부터** 다루기로 했다. TODO에 올렸다.

### 검증

* Windows x86 Debug 빌드: 오류·경고 0건.
* 단위 테스트: `checks: 1809, failures: 0`. 1st SE 값을 검사하고 다른 프로파일 무선언 목록에서 제외했다.
* 실행: `osd_controls`에서 `build_timestamp` = `executable_timestamp` = `0x3862df27`, `autoplay_armed: true`.
* 사용자 확인: OSD로 autoplay를 켜 곡이 자동 연주됨.

### 남은 것

* 1st SE가 곡 시작 때 값을 고정하는지, 모드 값의 의미.
* `ez2dj1st`가 같은 구조인지.
* 효과음 반복 문제(별도 작업).

## English

### Where the skill fit and where it did not

The dump was task 294's `resumed` dump (timestamp `0x3862df27`, no gaps). The demo clues `DemoGame`, `ShowDemoPlay` and `ClubMixDemoGame` were referenced not by a guard global but as **arguments to the scene registration function `0x00423670`**. Steps 4 and 5 **did not fit**: the on/off pair is split across a scene's init and destroy callbacks rather than one function. The substitute path listed the 60 scenes and read `DemoGame`'s init callback, which writes two constants: `[0x01c3f3a0] = 2`, a mode value with 31 writes, and `[0x01c3f3a4] = 1`, set to 1 only in the init of the three self-playing scenes and to 0 in their destroy. Its 13 reads are the chart player's start argument, the player scenes' auto/manual branch, and key-sound play and stop in input handling. A read-only poll showed it at 1 only during self-play (60.6-96.4 s and 103.1-136.7 s, mode also 2 in the second), and after declaring RVA `0x0183f3a4` with timestamp `0x3862df27` the control armed and the user confirmed autoplay through the OSD.

### Skill improvements

* Added `register_calls.py`, listing each registration call's arguments in call order with strings resolved. On 1st SE it produced all 60 scenes with their callbacks and handles, and its third and fourth callbacks match the autoplay write functions exactly.
* `SKILL.md` gained **path 3-1, the scene engine**: the procedure when the clue is a scene name, the criteria, and why `paired_writes.py` cannot see the pair.
* The arguments were first unpacked in reverse, misaligning the table; the tool now accounts for the last `push` being the first argument.

### Separate issue — endlessly repeating sound effects

During the user's check a coin-like sound repeated endlessly. The first **inference** — autoplay skipping the sound-stop call in input handling — was rejected when the user confirmed it **reproduces regardless of autoplay**. A rerun with `--audio-volume-trace` established that 1st SE creates one-shot effect buffers with `0x000140e2` (`DSBCAPS_STATIC | GETCURRENTPOSITION2` and others); that the DirectSound HLE's `is_streaming()` treats any buffer with `LOCHARDWARE` or `GETCURRENTPOSITION2`, or of 360,448 bytes, as streaming, so every one of them is `is_streaming=1`; and that a streaming track ran unbroken for about 126 seconds (`sequence=6300`) near the end of the log. A one-shot buffer wrapping in the streaming ring path is **inferred** to be the cause; the log stopped at 4,096 lines, so the repeating buffer was not identified directly. The rule exists for 4th and `ez2d2m` JAM streaming, so by agreement with the user it becomes **a separate task starting from design**, now in the TODO.

### Verification

* Windows x86 Debug build: no errors or warnings.
* Unit tests: `checks: 1809, failures: 0`, checking the 1st SE values and removing it from the no-declaration list.
* Run: `osd_controls` shows `build_timestamp` = `executable_timestamp` = `0x3862df27` and `autoplay_armed: true`.
* User check: turning autoplay on through the OSD made the song play itself.

### Remaining

* Whether 1st SE latches the value at song start, and the meaning of the mode value.
* Whether `ez2dj1st` shares the structure.
* The repeating sound effects (separate task).
