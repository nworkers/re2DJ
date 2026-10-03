# EZ2DJ 6th와 Remember 1st의 데모 플레이와 autoplay / Demo play and autoplay in EZ2DJ 6th and Remember 1st

주제: 6th CHD가 실행하는 두 게임 실행 파일에서 켜고 끌 수 있는 autoplay 상태를 찾는다. [5th](ez2dj5th-demo-play.md)와 [1st Tracks](ez2dj1st-demo-play.md) 구조와 대조한다.

*Topic: a switchable autoplay state in the two game executables the 6th CHD runs, checked against the [5th](ez2dj5th-demo-play.md) and [1st Tracks](ez2dj1st-demo-play.md) structures.*

| 실행 파일 / Executable | TimeDateStamp | `size_of_image` | 결론 / Verdict |
| --- | --- | --- | --- |
| `EZ2DJ/EZ2DJ6th.EXE` | `0x411f6d44` | `0x00e34000` | autoplay `[0x008896ac]` — **확인됨** / *confirmed* |
| `EZ2DJ/EZ2DJ1ST/Ez2DJ.exe` (Remember 1st) | `0x411bbf5c` | `0x01914000` | 전환할 변수 없음 — **추정** / *no switchable variable — inferred* |

두 파일 모두 image base `0x00400000`이고 보호 섹션이 없다(`.text`·`.rdata`·`.data`만 있다). 그래서 실행 중 덤프 대신 CHD에서 꺼낸 파일을 섹션 배치대로 펼친 이미지로 정적 분석했다. 모든 주소는 **해당 빌드에만** 해당한다.

*Both have image base `0x00400000` and no protection section (only `.text`, `.rdata`, `.data`), so instead of a run-time dump the files taken from the CHD were laid out by their sections and analysed statically. Every address belongs to **that build only**.*

근거 작업: [작업 436](../work-logs/20261003-436-ez2dj6th-autoplay.md)
절차: [`game-state-hunt` 스킬](../../.agents/skills/game-state-hunt/SKILL.md)

---

## 1. 6th

### 1.1 확인됨: 데모 플래그 `0x008895f8` / Confirmed: the demo flag at `0x008895f8`

`..\..\Common\DEMOPLAY.bmp`와 `DEMOPLAY_mask.bmp` 로드(`0x00415378`, `0x00415390`)가 `[0x008895f8] != 0`일 때만 일어난다. 절대 참조는 **쓰기 2곳, 읽기 15곳**이고, 쓰기 두 곳(1과 0)이 모두 데모 시작 루틴 `0x0044c020`에 있다. 5th(쓰기 2곳, 읽기 14곳)와 같은 모양이다.

*Loading `..\..\Common\DEMOPLAY.bmp` and `DEMOPLAY_mask.bmp` (`0x00415378`, `0x00415390`) happens only when `[0x008895f8] != 0`. The global has **two writes and fifteen reads**, both writes (1 and 0) in the demo start routine `0x0044c020`, the shape 5th has (two writes, fourteen reads).*

### 1.2 확인됨: autoplay 플래그 `0x008896ac` / Confirmed: the autoplay flag at `0x008896ac`

**정적 근거.** 6th 빌드는 최적화되어 5th의 setter·getter가 인라인됐다.

- 데모 시작 루틴 `0x0044c020`의 첫 부분이 `eax = 1` 하나로 `[0x008895f8]`과 `[0x008896ac]`를 **함께** 쓴다(`0x0044c02c`, `0x0044c031`). 루틴 끝(`0x0044c1d8`, `0x0044c1de`)에서 둘을 함께 0으로 되돌린다.
- `[0x008896ac]`의 절대 참조는 쓰기 3곳, 읽기 17곳이다. 쓰기 하나는 위의 짝이 아닌 `0x00413a2b`로, 입력 슬롯 `0x1b`가 눌리면(`0x0040a870` 호출의 반환값 2) `1 - 값`을 쓴다. 5th의 `0x00436609`와 같다.
- 판정 계열의 읽기(예: `0x004225a6`)는 키별 노트 데이터를 `[esi + ebp*8 + 0x1f0]`에 저장한 직후 이 값이 0이 아니면 `[esi + 0x1d8]` 객체로 `0x00421260`을 부른다. 5th의 "노트 데이터 저장 직후" 호출처에 대응한다. 그 호출을 키 누름 연출로 보는 것은 **추정**이다.

*Static evidence. The 6th build is optimised, and 5th's setter and getter are inlined. The demo start routine `0x0044c020` writes `[0x008895f8]` and `[0x008896ac]` **together** from one `eax = 1` (`0x0044c02c`, `0x0044c031`) and returns both to 0 at its end (`0x0044c1d8`, `0x0044c1de`). `[0x008896ac]` has three writes and seventeen reads; the write outside that pair, `0x00413a2b`, stores `1 - value` when input slot `0x1b` is pressed (`0x0040a870` returning 2), as 5th's `0x00436609` does. A judgement-path read such as `0x004225a6` follows the store of per-key note data into `[esi + ebp*8 + 0x1f0]` and, when the value is non-zero, calls `0x00421260` on the `[esi + 0x1d8]` object, matching 5th's "right after storing note data" sites; reading that call as the key-press effect is **inferred**.*

**런타임 근거.** Linux x64에서 어트랙트를 외부 읽기 전용으로 180초 관찰했다(`guest_memory.py poll --launch`).

*Run-time evidence: 180 seconds of attract watched read-only from outside on Linux x64 (`guest_memory.py poll --launch`).*

| 시각 / time | `[0x008895f8]` demo | `[0x008896ac]` autoplay |
| --- | --- | --- |
| 0.00 s | 0 | 0 |
| 64.78 s | **1** | **1** |
| 108.55 s | 0 | 0 |

**제품 경로.** 6th 프로필에 RVA `0x004896ac`와 timestamp `0x411f6d44`를 선언했다. Linux 실행에서 6th 자식만 무장되고(`game controls : autoplay armed`), launcher(`0x411646a8`)와 Remember 1st는 무장되지 않는다. 사용자가 OSD 토글로 6th의 autoplay 동작을 확인했다.

*Product path: the 6th profile declares RVA `0x004896ac` with timestamp `0x411f6d44`. In a Linux run only the 6th child arms it (`game controls : autoplay armed`), never the launcher (`0x411646a8`) or Remember 1st. The user confirmed 6th's autoplay through the OSD toggle.*

### 1.3 관찰: 부분 자동 설정 / Observed: partial-auto settings

`AutoPedal`, `AutoScratchR`, `AutoScratchL` 키 문자열이 코드에서 참조되고(`0x0044ad0c`, `0x0044acf7`, `0x0044ace2`), 모드 선택에 `EFFECTOR_AUTO_PEDAL`·`EFFECTOR_AUTO_SCRATCH(_L/_R)` 그림이 있다. 5th의 `AutoScratch`가 좌우로 나뉜 것으로 **추정**한다. 노트 전체를 치는 autoplay와는 다른 기능이며 이 작업에서 다루지 않았다(**미확정**).

*The key strings `AutoPedal`, `AutoScratchR` and `AutoScratchL` are referenced from code (`0x0044ad0c`, `0x0044acf7`, `0x0044ace2`), and mode select has `EFFECTOR_AUTO_PEDAL` and `EFFECTOR_AUTO_SCRATCH(_L/_R)` pictures, **inferred** to be 5th's `AutoScratch` split into left and right. This is a different feature from autoplay hitting every note and was not examined here (**unresolved**).*

## 2. Remember 1st

### 2.1 확인됨: 1st Tracks와 같은 장면 구조 / Confirmed: the 1st Tracks scene structure

장면 이름 `game`, `player`, `DemoGame`, `DemoPlayer`, `ClubMixDemoGame`, `ShowDemoPlay`와 곡 재생 실패 문구 `EZModule(ezPlay)`가 있다. 데모 전용 플레이어 장면 `DemoPlayer`가 따로 있다.

*The scene names `game`, `player`, `DemoGame`, `DemoPlayer`, `ClubMixDemoGame`, `ShowDemoPlay` and the chart-player failure text `EZModule(ezPlay)` are present, with a dedicated demo player scene `DemoPlayer`.*

### 2.2 확인됨: 곡 재생기의 자동 인자는 상수다 / Confirmed: the chart player's auto argument is a constant

곡 재생 시작 함수 `0x00411a40`의 호출처 7곳 중 `push 1`은 `ClubMixDemoGame`·`DemoGame` 근처의 두 곳(`0x0040aa46`, `0x0040cfb7`)뿐이다. 나머지 다섯 곳은 `push 0`이거나, 함수 앞에서 0으로 만든 레지스터(`xor ebp, ebp`, `xor esi, esi`)를 넘긴다. 재생기는 인자를 `[0x0055795c]`에 저장하고, 0이 아니면 채널 구조체(`0x005586cc`부터 `0xbc` 간격)의 첫 필드를 2로 쓴다. 매 프레임 진행(`0x00411e15`)은 값이 0일 때만 `0x00411e70`을 부른다. 1st Tracks의 `[0x0055bc4c]`, `0x0041af60`, `0x0041b5a0`과 같은 형태다.

*Of the seven calls to the chart-player start `0x00411a40`, only two, near `ClubMixDemoGame` and `DemoGame` (`0x0040aa46`, `0x0040cfb7`), push 1; the other five push 0 or a register zeroed earlier in the function (`xor ebp, ebp`, `xor esi, esi`). The player stores the argument in `[0x0055795c]` and, when non-zero, writes 2 into the first field of the channel structures (from `0x005586cc`, `0xbc` apart). The per-frame progress (`0x00411e15`) calls `0x00411e70` only when the value is 0. This is 1st Tracks' shape with `[0x0055bc4c]`, `0x0041af60` and `0x0041b5a0`.*

### 2.3 추정: 전환할 autoplay 변수가 없다 / Inferred: no switchable autoplay variable

1st Tracks에서는 `[0x0055bc4c]`에 곡 도중 1을 써도 변화가 없었다(작업 304). Remember 1st는 같은 구조이므로 같은 결론으로 **추정**하고 `game_controls`에 선언하지 않는다. 이 빌드에서 쓰기 시험은 하지 않았다. 일반 곡을 자동 연주하려면 장면 교체나 코드 패치가 필요하며, 별도 설계가 필요하다.

*In 1st Tracks writing 1 into `[0x0055bc4c]` mid-song changed nothing (task 304). Remember 1st has the same structure, so the same verdict is **inferred** and nothing is declared in `game_controls`; no write test was done on this build. Automatic play of a normal song would need a scene swap or a code patch, which needs its own design.*

## 3. 미확정 / Unresolved

* 6th가 autoplay 값을 곡 시작 때 고정하는지.
* `AutoPedal`·`AutoScratchL`·`AutoScratchR`의 실제 동작.
* 입력 슬롯 `0x1b`의 물리 바인딩.
* Remember 1st의 `[0x0055795c]` 쓰기 시험 결과.

*Whether 6th latches the autoplay value at song start; what `AutoPedal`, `AutoScratchL` and `AutoScratchR` actually do; input slot `0x1b`'s physical binding; and the outcome of a write test of Remember 1st's `[0x0055795c]`.*
