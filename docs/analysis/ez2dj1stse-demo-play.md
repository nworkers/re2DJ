# EZ2DJ 1st SE 자동 플레이 장면과 autoplay 플래그 / EZ2DJ 1st SE self-playing scenes and the autoplay flag

주제: 1st SE CHD 빌드의 장면 엔진에서 autoplay 상태가 어디에 있는가. 3rd~5th와 구조가 다르다.

*Topic: where the autoplay state lives in the scene engine of the 1st SE CHD build, which is structured unlike 3rd-5th.*

측정 대상: 1st SE `Ez2DJ.exe` CHD `.protect` 빌드, TimeDateStamp `0x3862df27`, image base `0x00400000`, `size_of_image` `0x01aec000`. 모든 주소는 **이 빌드에만** 해당한다.

*Measured on the 1st SE `Ez2DJ.exe` CHD `.protect` build, TimeDateStamp `0x3862df27`, image base `0x00400000`, `size_of_image` `0x01aec000`. Every address belongs to **this build only**.*

근거 작업: [작업 302](../work-logs/20260918-302-ez2dj1stse-autoplay-hunt.md)
절차: [`game-state-hunt` 스킬](../../.agents/skills/game-state-hunt/SKILL.md)

---

## 1. 확인됨: 장면 엔진 / Confirmed: a scene engine

게임은 장면을 등록 함수 `0x00423670` 하나로 등록한다. 인자는 콜백 네 개, 객체 크기, 플래그, 이름, 핸들 전역 주소 순이다. 호출은 60곳이며 `game`, `player`, `MusicSelect`, `Title` 등 이름 붙은 장면이 나온다.

*The game registers scenes through one function, `0x00423670`, taking four callbacks, an object size, a flag, a name and the address of a handle global, in that order. There are 60 calls, naming scenes such as `game`, `player`, `MusicSelect` and `Title`.*

| 장면 / scene | 콜백 1 | 초기화 / init (콜백 3) | 종료 / destroy (콜백 4) | 핸들 / handle |
| --- | --- | --- | --- | --- |
| `game` | `0x00407d40` | `0x004080d0` | `0x00408540` | `[0x01eb7f38]` |
| `player` | `0x0040bf40` | `0x0040c9a0` | `0x0040cbe0` | `[0x01eb7f30]` |
| `ClubMixDemoGame` | `0x0040dfd0` | `0x0040e220` | `0x0040e660` | `[0x01eb7ef0]` |
| `DemoGame` | `0x0040e6e0` | `0x0040e920` | `0x0040eda0` | `[0x01eb7eec]` |
| `ShowDemoPlay` | `0x0040ee20` | `0x0040ef50` | `0x0040efa0` | `[0x01eb7ee8]` |
| `HowToPlayGame` | `0x004123c0` | `0x004125e0` | `0x004128c0` | `[0x01eb7ec0]` |

콜백 3·4를 초기화·종료로 부르는 것은 5절의 쓰기 위치가 정확히 일치한다는 근거에 따른 **추정**이다.

*Naming callbacks 3 and 4 init and destroy is **inferred** from section 3's write sites matching them exactly.*

데모는 3rd~5th처럼 "데모 플래그를 켠 일반 게임 장면"이 아니라 **데모 전용 장면**(`DemoGame`, `ClubMixDemoGame`)이고, DEMO PLAY 오버레이도 별도 장면 `ShowDemoPlay`다. 데모 전용 플레이어 장면은 없으므로 데모도 일반 `player` 장면을 쓴다.

*The demo is not a normal game scene with a demo flag as in 3rd-5th but **dedicated scenes** (`DemoGame`, `ClubMixDemoGame`), with the DEMO PLAY overlay its own scene, `ShowDemoPlay`. There is no demo-only player scene, so the demo uses the normal `player` scene.*

## 2. 확인됨: 모드 값 `0x01c3f3a0`은 autoplay가 아니다 / Confirmed: the mode value at `0x01c3f3a0` is not autoplay

`DemoGame` 초기화가 `[0x01c3f3a0] = 2`와 `[0x01c3f3a4] = 1`을 연달아 쓴다. 앞의 값은 쓰기 31곳·읽기 81곳에서 0·1·2로 바뀌는 게임 전역 모드 값이며 autoplay가 아니다. 의미(플레이 인원 또는 진행 상태)는 **미확정**이다.

*`DemoGame`'s init writes `[0x01c3f3a0] = 2` and `[0x01c3f3a4] = 1` in a row. The first is a game-wide mode value moved among 0, 1 and 2 by 31 writes and read at 81 sites, not autoplay; its meaning (player count or progress state) is **unresolved**.*

## 3. 확인됨: autoplay 플래그 `0x01c3f3a4` / Confirmed: the autoplay flag at `0x01c3f3a4`

**쓰기 7곳.** 1로 쓰는 곳은 **스스로 플레이하는 세 장면의 초기화뿐**이고, 0으로 쓰는 곳은 그 장면들의 종료와 전역 초기화 함수 `0x00436530`이다.

*Seven writes. It is set to 1 **only in the init of the three self-playing scenes**, and to 0 in their destroy callbacks and in the global reset `0x00436530`.*

| 장면 / scene | 1 | 0 |
| --- | --- | --- |
| `DemoGame` | `0x0040e945` | `0x0040edb7` |
| `ClubMixDemoGame` | `0x0040e23b` | `0x0040e677` |
| `HowToPlayGame` | `0x00412605` | `0x004128d7` |

**읽기 13곳.** 세 갈래다.

*Thirteen reads, in three groups.*

* **곡 재생 시작에 인자로 넘김** — `0x004019d4`, `0x00401fa2`, `0x00408500`, `0x00412efb`. 값이 곡 재생기 `0x0041a7a0`의 인자로 들어가므로 곡 시작 때 고정된다고 **추정**한다. / *Passed into the chart player `0x0041a7a0` when a song starts, so the value is **inferred** to be latched at song start.*
* **플레이어 매 프레임 처리의 자동·수동 분기** — `0x00406941`(`ClubMixPlayer`), `0x0040bf8f`(`player`), `0x00416572`(`DoublePlayer`). / *The auto-versus-manual branch of each player scene's per-frame update.*
* **입력 처리** — `0x004075fb`·`0x004076a3`, `0x0040c822`·`0x0040c8b9`, `0x00416ea9`·`0x00416f40`. 수동이면 키를 누를 때 키음을 재생하고 뗄 때 정지하며, autoplay면 둘 다 건너뛰고 키 빔 연출만 한다. / *Input handling: when manual a key press plays its key sound and release stops it; with autoplay both are skipped and only the key-beam effect remains.*

**런타임 근거.** 어트랙트에서 외부 읽기 전용으로 200초 관찰했다.

*Run-time evidence, read-only from outside, 200 seconds in attract:*

| 시각 / time | `[0x01c3f3a0]` mode | `[0x01c3f3a4]` autoplay |
| --- | --- | --- |
| 0.00 s | 0 | 0 |
| 60.59 s | 0 | **1** |
| 96.38 s | 0 | 0 |
| 103.14 s | **2** | **1** |
| 136.69 s | 0 | 0 |

두 번째 구간은 `DemoGame` 초기화가 두 값을 함께 쓰는 것과 일치한다. 첫 구간은 mode가 바뀌지 않으므로 `ClubMixDemoGame` 또는 `HowToPlayGame`으로 **추정**한다.

*The second span matches `DemoGame`'s init writing both values; the first leaves mode unchanged and is **inferred** to be `ClubMixDemoGame` or `HowToPlayGame`.*

**제품 경로 확인.** 프로파일에 RVA `0x0183f3a4`와 timestamp `0x3862df27`를 선언하자 런처가 무장했고(`autoplay_armed: true`), 사용자가 OSD로 autoplay를 켜 **곡이 자동 연주됨을 확인했다.**

*Product path. Declaring RVA `0x0183f3a4` and timestamp `0x3862df27` got the control armed (`autoplay_armed: true`), and the user turned autoplay on in the OSD and **confirmed the song plays itself**.*

## 4. 별도 문제: 효과음 무한 반복 — autoplay와 무관 / Separate issue: endlessly repeating sound effects — unrelated to autoplay

같은 확인 중 동전 효과음 같은 소리가 끝없이 반복됐다. 사용자는 **autoplay를 켜고 끄는 것과 관계없이 재현된다**고 확인했다.

*During the same check a coin-like sound effect repeated endlessly; the user confirmed it **reproduces regardless of toggling autoplay**.*

**확인됨 — 효과음 버퍼가 스트리밍으로 분류된다.** `--audio-volume-trace` 실행에서 1st SE는 일회성 효과음 버퍼를 `0x000140e2`(`DSBCAPS_STATIC | CTRLFREQUENCY | CTRLPAN | CTRLVOLUME | STICKYFOCUS | GETCURRENTPOSITION2`)로 만들고, DirectSound HLE는 `GETCURRENTPOSITION2`만 보고 이들을 모두 `is_streaming=1`로 분류한다. 배경음 링 버퍼는 `0x000140c6`, 360,448 바이트다.

*Confirmed — the sound-effect buffers are classified as streaming. In a `--audio-volume-trace` run 1st SE creates its one-shot effect buffers with `0x000140e2` (`DSBCAPS_STATIC | CTRLFREQUENCY | CTRLPAN | CTRLVOLUME | STICKYFOCUS | GETCURRENTPOSITION2`), and the DirectSound HLE, looking only at `GETCURRENTPOSITION2`, classifies every one as `is_streaming=1`. The background ring buffers are `0x000140c6`, 360,448 bytes.*

**확인됨 — 이것이 반복의 원인이었다.** 스트리밍 경로는 반복 플래그와 관계없이 링을 계속 순환했다. [작업 303](../work-logs/20260918-303-directsound-static-oneshot.md)에서 `STATIC` 효과음을 일반 경로로 돌리고 스트리밍 경로가 `DSBPLAY_LOOPING`을 따르게 하자 효과음이 모두 `is_streaming=0`으로 분류됐고, 사용자가 반복이 사라졌음을 청취로 확인했다. 같은 조합을 쓰는 1st Tracks도 함께 고쳐졌다.

*Confirmed — this was the cause. The streaming path kept cycling the ring whatever the loop flag said. In [task 303](../work-logs/20260918-303-directsound-static-oneshot.md) `STATIC` effects moved to the normal path and the streaming path began honoring `DSBPLAY_LOOPING`; every effect now classifies `is_streaming=0`, and the user confirmed by ear that the repeat is gone. 1st Tracks, which uses the same combination, was fixed with it.*

## 5. 미확정 / Unresolved

* 1st SE가 실제로 곡 시작 때 값을 고정하는지(곡 도중 끄기 시험 안 함).
* 모드 값 `[0x01c3f3a0]`의 의미.
* 1st Tracks(`ez2dj1st`)가 같은 장면 엔진과 같은 플래그 배치를 갖는지.

*Whether 1st SE really latches the value at song start (no mid-song switch-off was tested); the meaning of the mode value `[0x01c3f3a0]`; and whether 1st Tracks (`ez2dj1st`) shares the same scene engine and flag layout.*
