# EZ2DJ 4th 데모 플레이와 autoplay 플래그 / EZ2DJ 4th demo play and the autoplay flag

주제: 4th의 설정 레지스트리, 데모 플래그, autoplay 플래그. [3rd 분석](ez2dj3rd-demo-play.md)과 같은 구조인지 대조한다.

*Topic: 4th's settings registry, demo flag and autoplay flag, checked against the structure found in [the 3rd analysis](ez2dj3rd-demo-play.md).*

측정 대상: 4th `EZ2DJ.EXE` CHD 빌드, TimeDateStamp `0x3d369bfd`, image base `0x00400000`, `size_of_image` `0x0071a000`. 모든 주소는 **이 빌드에만** 해당한다.

*Measured on the 4th `EZ2DJ.EXE` CHD build, TimeDateStamp `0x3d369bfd`, image base `0x00400000`, `size_of_image` `0x0071a000`. Every address belongs to **this build only**.*

근거 작업: [작업 300](../work-logs/20260918-300-ez2dj4th-autoplay-hunt.md)
절차: [`game-state-hunt` 스킬](../../.agents/skills/game-state-hunt/SKILL.md)

---

## 1. 확인됨: 설정 레지스트리에 autoplay 키가 없다 / Confirmed: no autoplay key in the settings registry

3rd와 같은 `push 주소; push "키"; mov ecx, [0x005111e0]; call 접근자` 모양이다. 설정 객체 `[0x005111e0]`에 묶인 키는 **24개**이며 `AutoPlay`는 없다.

*The same `push address; push "key"; mov ecx, [0x005111e0]; call accessor` shape as 3rd. **24 keys** are bound to the settings object `[0x005111e0]`, and `AutoPlay` is not among them.*

덤프에 `AutoPlay` 문자열이 한 곳 있으나 **코드 참조가 0건**이다. 게임이 overlay INI를 읽으며 만든 런타임 사본으로 **추정**한다. 이 overlay에는 `AutoPlay` 줄이 있으나 원본 INI에 있었는지는 이번에 확인하지 않았다(**미확정**). 어느 쪽이든 코드가 묶지 않으므로 저장 설정이 아니라는 결론은 같다.

*The dump holds one `AutoPlay` string with **no code reference**, **inferred** to be the game's run-time copy of what it read from the overlay INI. That overlay has an `AutoPlay` line; whether the original INI had one was not checked (**unresolved**). Either way the code does not bind it, so it is not a stored setting.*

## 2. 확인됨: 데모 플래그 `0x00ac290c` / Confirmed: the demo flag at `0x00ac290c`

`..\..\Common\DEMOPLAY.bmp`를 참조하는 함수 `0x0043b52f`가 `[0x00ac290c] != 0`일 때만 로드한다. 이 전역의 절대 참조는 **쓰기 2곳, 읽기 14곳**이며, 쓰기 두 곳(1과 0)이 모두 데모 시작 루틴 `0x004a4430`에 있다.

*Function `0x0043b52f`, which references `..\..\Common\DEMOPLAY.bmp`, loads it only when `[0x00ac290c] != 0`. The global has **two writes and fourteen reads**, and both writes (1 and 0) are in the demo start routine `0x004a4430`.*

데모 시작 루틴은 3rd와 같이 게임 장면을 동기 실행하는 래퍼이며, 믹스별 고정 데모곡을 고른다(`ClubMix`→`bow`, `SpaceMix`→`firestorm`, `RadioMix`→`complex`, `StreetMix`→`lovely`, `7StreetMix`→`delight` 등).

*Like 3rd's, the routine wraps the game scene synchronously and picks a fixed demo song per mix (`ClubMix` → `bow`, `SpaceMix` → `firestorm`, `RadioMix` → `complex`, `StreetMix` → `lovely`, `7StreetMix` → `delight`, and so on).*

## 3. 확인됨: autoplay 플래그 `0x00ac29b0` / Confirmed: the autoplay flag at `0x00ac29b0`

**정적 근거.** 데모 시작 루틴이 데모 플래그와 **짝으로** setter `0x00437600`을 통해 `[0x00ac29b0]`을 1로 세웠다가 0으로 되돌린다. getter는 `0x004375f0`(thunk `0x00402117`)이며 호출처는 11개 함수에 17곳이다. 3rd와 대응하는 곳은 다음과 같다.

*Static evidence. The demo start routine sets `[0x00ac29b0]` to 1 and back to 0 through setter `0x00437600`, **paired** with the demo flag. The getter is `0x004375f0` (thunk `0x00402117`), called from 17 sites in 11 functions, which correspond to 3rd's as follows.*

| 4th 호출처 / 4th sites | 3rd 대응 / 3rd counterpart | 동작 / behavior |
| --- | --- | --- |
| `0x00451439`, `0x004a0c94` | `0x0044e2c2` | 키별 노트 데이터(`+0x1cc`) 저장 직후 / right after storing per-key note data (`+0x1cc`) |
| `0x004515a4`, `0x004a0dc6` | `0x0044e3a1` | 노트 데이터를 `-1`로 지운 직후 / right after clearing the note data to `-1` |
| `0x004751e5` | `0x00479a95` | 같은 y 좌표 상수 70.0(`0x428c0000`) / the same y constant 70.0 (`0x428c0000`) |
| `0x00436479` | `0x0043450f` | 입력 슬롯 `0x1b`가 눌리면 `1 - 값` / `1 - value` when input slot `0x1b` is pressed |

노트 판정 계열 호출처가 두 벌(`0x004512e1`·`0x004a0afa` 함수)이다. 게임 모드가 둘인 것으로 **추정**하며, 어느 모드인지는 **미확정**이다.

*The note-judgement sites come in two sets (functions `0x004512e1` and `0x004a0afa`), **inferred** to be two game modes; which modes is **unresolved**.*

**런타임 근거.** 어트랙트에서 외부 읽기 전용으로 170초 관찰했다.

*Run-time evidence, read-only from outside, 170 seconds in attract:*

| 시각 / time | `[0x00ac290c]` demo | `[0x00ac29b0]` autoplay |
| --- | --- | --- |
| 0.00 s | 0 | 0 |
| 51.83 s | **1** | **1** |
| 99.16 s | 0 | 0 |
| 138.22 s | **1** | **1** |

4th의 데모 한 번은 약 47초로 3rd(약 22~25초)보다 길다.

*One 4th demo lasts about 47 seconds, longer than 3rd's 22-25.*

**제품 경로 확인.** 프로파일에 RVA `0x006c29b0`과 timestamp `0x3d369bfd`를 선언하자 런처가 무장했고(`autoplay_armed: true`), 사용자가 곡 선택 화면에서 OSD의 Autoplay를 체크하고 곡을 시작해 **autoplay 동작을 확인했다.** 데모 현상(오버레이·음소거·입력 시 종료)에 대한 별도 보고는 받지 않았다.

*Product path. Declaring RVA `0x006c29b0` and timestamp `0x3d369bfd` in the profile got the control armed (`autoplay_armed: true`), and the user ticked Autoplay in the OSD at song select, started a song, and **confirmed autoplay works**. No separate report was received on demo effects (overlay, muting, ending on input).*

## 4. 미확정 / Unresolved

* 4th가 3rd처럼 값을 곡 시작 때 고정하는지. 곡 도중 끄는 시험은 하지 않았다.
* 판정 계열이 두 벌인 이유와 각각의 게임 모드.
* 슬롯 `0x1b`의 물리 바인딩.
* 원본 4th INI에 `AutoPlay` 줄이 있었는지.

*Whether 4th, like 3rd, latches the value at song start (no mid-song switch-off was tested); why the judgement sites come in two sets and which game mode each serves; slot `0x1b`'s physical binding; and whether the original 4th INI had an `AutoPlay` line.*
