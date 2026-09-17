# EZ2DJ 5th 데모 플레이와 autoplay 플래그 / EZ2DJ 5th demo play and the autoplay flag

주제: 5th의 설정 레지스트리, 데모 플래그, autoplay 플래그. [3rd](ez2dj3rd-demo-play.md)·[4th](ez2dj4th-demo-play.md) 구조와 대조한다.

*Topic: 5th's settings registry, demo flag and autoplay flag, checked against the [3rd](ez2dj3rd-demo-play.md) and [4th](ez2dj4th-demo-play.md) structure.*

측정 대상: 5th `EZ2DJ.EXE` CHD 빌드, TimeDateStamp `0x3f53377b`, image base `0x00400000`, `size_of_image` `0x00746000`. 모든 주소는 **이 빌드에만** 해당한다.

*Measured on the 5th `EZ2DJ.EXE` CHD build, TimeDateStamp `0x3f53377b`, image base `0x00400000`, `size_of_image` `0x00746000`. Every address belongs to **this build only**.*

근거 작업: [작업 301](../work-logs/20260918-301-ez2dj5th-autoplay-hunt.md)
절차: [`game-state-hunt` 스킬](../../.agents/skills/game-state-hunt/SKILL.md)

---

## 1. 확인됨: 설정 레지스트리 — autoplay 키 없음, 보조 자동 키 두 개 / Confirmed: settings registry — no autoplay key, two partial-auto keys

3rd·4th와 같은 `push 주소; push "키"; mov ecx, [0x00516350]; call 접근자` 모양이다. 설정 객체 `[0x00516350]`에 묶인 키는 **26개**이며 `AutoPlay`는 없다.

*The same registry shape as 3rd and 4th, on settings object `[0x00516350]`: **26 keys**, with no `AutoPlay`.*

4th의 24개에 **`AutoScratch`와 `AutoPedal`** 이 더해졌다. 이름으로 보아 턴테이블 스크래치와 페달만 자동 처리하는 저장 설정으로 **추정**하며, 노트 전체를 치는 autoplay와는 다른 기능이다. 실제 동작은 **미확정**이다.

*Compared with 4th's 24, **`AutoScratch` and `AutoPedal`** are added. By name they are **inferred** to be stored settings that automate only the turntable scratch and the pedal, a different feature from autoplay hitting every note; their actual behavior is **unresolved**.*

## 2. 확인됨: 데모 플래그 `0x00aee194` / Confirmed: the demo flag at `0x00aee194`

`..\..\Common\DEMOPLAY.bmp` 로드가 `[0x00aee194] != 0`일 때만 일어난다. 이 전역의 절대 참조는 **쓰기 2곳, 읽기 14곳**이며, 쓰기 두 곳(1과 0)이 모두 데모 시작 루틴 `0x004aabf7`에 있다.

*Loading `..\..\Common\DEMOPLAY.bmp` happens only when `[0x00aee194] != 0`. The global has **two writes and fourteen reads**, and both writes (1 and 0) are in the demo start routine `0x004aabf7`.*

## 3. 확인됨: autoplay 플래그 `0x00aee238` / Confirmed: the autoplay flag at `0x00aee238`

**정적 근거.** 데모 시작 루틴이 데모 플래그와 **짝으로** setter `0x00437790`을 통해 `[0x00aee238]`을 1로 세웠다가 0으로 되돌린다. getter는 `0x00437780`(thunk `0x00402121`)이며 호출처는 11개 함수에 17곳으로 4th와 같다.

*Static evidence. The demo start routine sets `[0x00aee238]` to 1 and back to 0 through setter `0x00437790`, **paired** with the demo flag. The getter is `0x00437780` (thunk `0x00402121`), called from 17 sites in 11 functions, as in 4th.*

| 5th 호출처 / 5th sites | 4th 대응 / 4th counterpart | 동작 / behavior |
| --- | --- | --- |
| `0x004524ea`, `0x004a7234` | `0x00451439`, `0x004a0c94` | 키별 노트 데이터 저장 직후 / right after storing per-key note data |
| `0x00452655`, `0x004a7366` | `0x004515a4`, `0x004a0dc6` | 노트 데이터를 `-1`로 지운 직후 / right after clearing the note data to `-1` |
| `0x0047b325` | `0x004751e5` | y 좌표 상수 70.0(`0x428c0000`) / y constant 70.0 (`0x428c0000`) |
| `0x00436609` | `0x00436479` | 입력 슬롯 `0x1b`가 눌리면 `1 - 값` / `1 - value` when input slot `0x1b` is pressed |

노트 데이터 필드는 두 판정 계열 중 `0x00452392` 함수에서 `+0x1d8`, `0x004a709a` 함수에서 `+0x1cc`다. 4th의 첫 계열(`+0x1cc`)보다 12바이트 뒤로 밀렸는데, 1절의 보조 자동 설정이 더해지며 구조체가 커진 것으로 **추정**한다.

*The note-data field is at `+0x1d8` in function `0x00452392` and `+0x1cc` in function `0x004a709a`. The first set moved 12 bytes past 4th's `+0x1cc`, **inferred** to be the structure growing with section 1's partial-auto settings.*

**런타임 근거.** 어트랙트에서 외부 읽기 전용으로 180초 관찰했다.

*Run-time evidence, read-only from outside, 180 seconds in attract:*

| 시각 / time | `[0x00aee194]` demo | `[0x00aee238]` autoplay |
| --- | --- | --- |
| 0.00 s | 0 | 0 |
| 68.60 s | **1** | **1** |
| 114.92 s | 0 | 0 |
| 171.25 s | **1** | **1** |

**제품 경로 확인.** 프로파일에 RVA `0x006ee238`과 timestamp `0x3f53377b`를 선언하자 런처가 무장했고(`autoplay_armed: true`), 사용자가 곡 선택 화면에서 OSD의 Autoplay를 체크해 **autoplay 동작을 확인했다.** 데모 현상(오버레이·음소거·입력 시 종료)에 대해서는 별도 언급을 받지 않았다.

*Product path. Declaring RVA `0x006ee238` and timestamp `0x3f53377b` got the control armed (`autoplay_armed: true`), and the user ticked Autoplay in the OSD at song select and **confirmed autoplay works**. Demo effects (overlay, muting, ending on input) were not separately mentioned.*

## 4. 미확정 / Unresolved

* `AutoScratch`·`AutoPedal`의 실제 동작과 autoplay 플래그와의 관계.
* 5th가 값을 곡 시작 때 고정하는지.
* 판정 계열 두 벌의 의미와 슬롯 `0x1b`의 물리 바인딩.

*The actual behavior of `AutoScratch` and `AutoPedal` and how they relate to the autoplay flag; whether 5th latches the value at song start; what the two judgement sets mean; and slot `0x1b`'s physical binding.*
