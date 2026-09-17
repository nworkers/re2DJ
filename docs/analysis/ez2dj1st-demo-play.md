# EZ2DJ 1st Tracks 자동 플레이 장면 — autoplay 변수 없음 / EZ2DJ 1st Tracks self-playing scenes — no autoplay variable

주제: 1st Tracks에 3rd~5th·1st SE처럼 켜고 끌 수 있는 autoplay 상태가 있는가. 결론은 **없다**(추정, 쓰기 시험으로 뒷받침).

*Topic: whether 1st Tracks has an autoplay state that can be switched like 3rd-5th and 1st SE. The conclusion is that **it does not** (inferred, backed by a write test).*

측정 대상: 1st Tracks `Ez2DJ.exe`, TimeDateStamp `0x3862fd9d`, image base `0x00400000`, `size_of_image` `0x019b6000`. 작업 294의 `resumed` 덤프(gaps 없음)를 썼다. 모든 주소는 **이 빌드에만** 해당한다.

*Measured on the 1st Tracks `Ez2DJ.exe`, TimeDateStamp `0x3862fd9d`, image base `0x00400000`, `size_of_image` `0x019b6000`, using task 294's `resumed` dump (no gaps). Every address belongs to **this build only**.*

근거 작업: [작업 304](../work-logs/20260918-304-ez2dj1st-autoplay-hunt.md)
절차: [`game-state-hunt` 스킬](../../.agents/skills/game-state-hunt/SKILL.md)
비교: [1st SE](ez2dj1stse-demo-play.md)

---

## 1. 확인됨: 장면 엔진과 데모 전용 플레이어 장면 / Confirmed: a scene engine with dedicated demo player scenes

1st SE와 같은 방식으로 등록 함수 `0x00424280` 하나가 장면 53개를 등록한다. 인자 순서(콜백 네 개, 크기, 플래그, 이름, 핸들 전역)도 같다.

*As in 1st SE, one registration function, `0x00424280`, registers 53 scenes with the same argument order (four callbacks, size, flag, name, handle global).*

| 장면 / scene | 콜백 1 | 콜백 2 | 콜백 3 | 콜백 4 | 핸들 / handle |
| --- | --- | --- | --- | --- | --- |
| `game` | `0x004057c0` | `0x00405900` | `0x00405ac0` | `0x00405e50` | `[0x01d83004]` |
| `player` | `0x00405eb0` | `0x00407db0` | `0x00407ec0` | `0x00408e90` | `[0x01d83000]` |
| `DemoGame` | `0x00413340` | `0x004134b0` | `0x00413530` | `0x004138b0` | `[0x01d82fac]` |
| `DemoPlayer` | `0x00413910` | `0x00415660` | `0x004156a0` | `0x00416320` | `[0x01d82fa8]` |
| `ClubMixDemoGame` | `0x0040f9a0` | `0x0040fb70` | `0x0040fbf0` | `0x0040ff60` | `[0x01d82fb8]` |
| `ClubMixDemoPlayer` | `0x00410690` | `0x004125f0` | `0x00412630` | `0x004132e0` | `[0x01d82fb0]` |
| `ShowDemoPlay` | `0x00416420` | `0x00416530` | `0x00416550` | `0x00416590` | `[0x01d82fa4]` |

**1st SE와 다른 점.** 1st SE는 데모도 일반 `player` 장면을 쓰고 전역 플래그로 자동·수동을 갈랐다. 1st Tracks는 **데모 전용 플레이어 장면**(`DemoPlayer`, `ClubMixDemoPlayer`)이 따로 있다. `HowToPlayGame` 장면은 없고 `ExerciseGame`·`ExercisePlayer`가 있다.

*How it differs from 1st SE: 1st SE ran its demo on the normal `player` scene and switched auto/manual with a global flag, whereas 1st Tracks has **dedicated demo player scenes** (`DemoPlayer`, `ClubMixDemoPlayer`). There is no `HowToPlayGame` scene; `ExerciseGame` and `ExercisePlayer` exist instead.*

## 2. 확인됨: 곡 재생기에 넘기는 자동 여부는 상수다 / Confirmed: the auto argument to the chart player is a constant

곡 재생 시작 함수 `0x0041af60`(실패 문구 `EZModule(ezPlay)`)의 호출처 7곳 중 **데모 장면 초기화 두 곳만 `push 1`**이다. 1st SE는 같은 자리에 autoplay 전역을 넘겼다.

*Of the seven calls to the chart player start `0x0041af60` (failure text `EZModule(ezPlay)`), **only the two demo scene inits push 1**; 1st SE passed its autoplay global here.*

| 호출 / call | 함수 / function | 인자 / argument |
| --- | --- | --- |
| `0x00413867` | `DemoGame` 콜백 3 `0x00413530` | `1` |
| `0x0040fedb` | `ClubMixDemoGame` 콜백 3 `0x0040fbf0` | `1` |
| `0x0040180e`, `0x00401d0a`, `0x00405e1b`, `0x00409d8e`, `0x0040d329` | ClubMix·일반·연습 게임 장면 / ClubMix, normal and exercise game scenes | `0` |

재생기는 인자를 `[0x0055bc4c]`에 저장하고, 0이 아니면 채널 3~16의 채널 구조체(`0x0055c788 + i * 0xbc`) 첫 필드를 2로 쓴다. `[0x0055bc4c]`의 참조는 5곳이며 모두 곡 진행 모듈 안이다. 쓰기는 재생기 저장과, 이미 재생 중일 때 재생기가 먼저 부르는 `0x0041ae20`의 0 초기화, 읽기는 재생기 안 두 곳과 매 프레임 진행 함수 `0x0041b4b0`이다. 후자는 값이 0일 때만 채널별 노트 처리 `0x0041b5a0`을 부른다. 이 처리를 놓친 노트 처리로 보는 것은 **추정**이다.

*The chart player stores its argument in `[0x0055bc4c]` and, when non-zero, writes 2 into the first field of the channel structs for channels 3-16 (`0x0055c788 + i * 0xbc`). `[0x0055bc4c]` has five references, all inside the chart module: the store and a zeroing in `0x0041ae20`, which the player start calls first when a song is already playing, two reads in the player start, and one in the per-frame advance `0x0041b4b0`, which calls the per-channel note handler `0x0041b5a0` only when the value is 0. Reading that handler as missed-note processing is **inferred**.*

**일반 `player` 장면은 `[0x0055bc4c]`를 읽지 않는다.**

***The normal `player` scene never reads `[0x0055bc4c]`.***

## 3. 확인됨: 런타임 관찰과 쓰기 시험 / Confirmed: run-time observation and the write test

**읽기 전용 관찰(어트랙트 200초).** `[0x0055bc4c]`는 89.39~119.68초와 132.45~162.75초에만 1이었다. 그 직전에 각각 `ClubMixDemoPlayer`와 `DemoPlayer` 핸들이 0이 아닌 값이 됐다. 핸들은 장면 종료 뒤에도 지워지지 않아 장면 활성 여부의 지표로는 쓸 수 없었다.

*Read-only, 200 seconds of attract: `[0x0055bc4c]` was 1 only from 89.39 to 119.68 s and from 132.45 to 162.75 s, just after the `ClubMixDemoPlayer` and `DemoPlayer` handles respectively became non-zero. The handles are not cleared when a scene ends, so they do not indicate whether a scene is active.*

**쓰기 시험(사용자 동의).** 일반 곡 진행 중 `[0x0055bc4c]`에 1을 썼다. 값은 20초 동안 1로 유지됐지만 사용자는 **아무 변화도 보지 못했다**. 노트는 자동으로 맞지 않았다.

*Write test, with the user's consent: 1 was written to `[0x0055bc4c]` during a normal song. The value stayed 1 for 20 seconds, but the user **saw no change**; notes were not hit automatically.*

## 4. 추정: 자동 연주는 데모 플레이어 장면의 코드에 있다 / Inferred: automatic play lives in the demo player scenes' code

`player`와 `DemoPlayer`의 콜백 1(매 프레임 처리로 추정)을 나란히 읽으면 흐름은 거의 같지만 부르는 멤버 함수가 전부 별도 사본이다(`0x00406090`↔`0x00413b00`, `0x00407a20`↔`0x00414ab0` 등). `DemoPlayer`에만 레인 7개의 타이머를 줄이는 `0x004155e0` 호출이 있고, `player`에만 있는 앞부분 블록이 있다. 곡 진행 모듈에는 일반 장면이 따를 자동 분기가 없으므로, 노트를 자동으로 치는 동작은 **데모 플레이어 장면 코드 자체**에 있다고 본다.

*Reading callback 1 (taken to be the per-frame update) of `player` and `DemoPlayer` side by side, the flow is nearly the same but every member function called is a separate copy (`0x00406090` vs `0x00413b00`, `0x00407a20` vs `0x00414ab0`, and so on). Only `DemoPlayer` calls `0x004155e0`, which counts down timers for seven lanes, and only `player` has an opening block. With no auto branch in the chart module for a normal scene to follow, automatic play is taken to live **in the demo player scenes' own code**.*

따라서 1st Tracks에는 3rd~5th·1st SE처럼 **주소 하나를 켜서 일반 곡을 자동 연주로 바꾸는 변수가 없다**고 결론짓고, `game_controls`에 선언하지 않는다. 일반 곡에서 자동 연주를 얻으려면 장면 교체나 코드 패치 같은 다른 개입이 필요하며, 별도 설계가 필요하다.

*1st Tracks is therefore concluded to have **no variable that turns a normal song into automatic play by switching one address**, as 3rd-5th and 1st SE have, and nothing is declared in `game_controls`. Automatic play in a normal song would need a different intervention, such as swapping scenes or patching code, and a design of its own.*

## 5. 미확정 / Unresolved

* `DemoPlayer`가 노트를 치는 정확한 지점.
* 채널 구조체 첫 필드 값 2와 `0x0041b5a0`의 정확한 의미.
* 1st SE가 같은 장면 엔진 계열에서 전역 플래그 방식으로 바뀐 경위.

*Exactly where `DemoPlayer` hits notes; the precise meaning of the value 2 in the channel struct's first field and of `0x0041b5a0`; and how 1st SE, from the same scene-engine line, came to use a global flag.*
