# EZ2Dancer 2nd MOVE 데모와 autoplay 플래그 / EZ2Dancer 2nd MOVE demo and the autoplay flag

주제: `ez2d2m`에서 autoplay에 해당하는 상태가 어디에 있는가. EZ2DJ 계열 셋과 모두 다른 네 번째 형태다.

*Topic: where the autoplay state lives in `ez2d2m`, a fourth shape unlike any of the three EZ2DJ ones.*

측정 대상: `EZ2Dancer.exe` CHD `.protect` 빌드, TimeDateStamp `0x3a5f074c`, image base `0x00400000`, `size_of_image` `0x0043b000`. 작업 294의 `resumed` 덤프(gaps 없음)를 썼다. 모든 주소는 **이 빌드에만** 해당한다.

*Measured on the `EZ2Dancer.exe` CHD `.protect` build, TimeDateStamp `0x3a5f074c`, image base `0x00400000`, `size_of_image` `0x0043b000`, using task 294's `resumed` dump (no gaps). Every address belongs to **this build only**.*

근거 작업: [작업 305](../work-logs/20260918-305-ez2d2m-autoplay-hunt.md)
절차: [`game-state-hunt` 스킬](../../.agents/skills/game-state-hunt/SKILL.md)
비교: [1st Tracks](ez2dj1st-demo-play.md), [1st SE](ez2dj1stse-demo-play.md), [3rd](ez2dj3rd-demo-play.md)

---

## 1. 확인됨: 클래스 쌍 구조 / Confirmed: paired classes

이 빌드는 장면을 등록 함수로 모으지 않고, `Title`·`SongSelect`·`NormalGame`·`DemoGame`처럼 클래스마다 `OnCreateGame`·`OnDestroyGame`과 `<이름>Director::Create`·`Delete`를 둔다. 각 함수는 자기 이름 문자열을 로그 함수 `0x004279a0`에 넘기므로 이름으로 식별된다.

*Rather than registering scenes through one function, this build gives each class its own `OnCreateGame`, `OnDestroyGame` and `<name>Director::Create`/`Delete` — `Title`, `SongSelect`, `NormalGame`, `DemoGame` and so on. Each passes its own name string to the logging function `0x004279a0`, which is how they are identified.*

| 클래스 / class | `OnCreateGame` | `Director::Create` | Director vtable |
| --- | --- | --- | --- |
| `NormalGame` | `0x00422510` | `0x00422cc0` 부근 | `0x0044e140` |
| `DemoGame` | `0x00424420` | `0x00424b40` | `0x0044e540` |

두 Director vtable은 앞 여섯 항목 중 네 개가 서로 다른 구현이다. 데모 전용 Director는 `system\common\demoplay.bmp`를 읽고 판정 메시지(`'JDGE'`)를 받는다.

*The two Director vtables differ in four of their first six entries. The demo Director loads `system\common\demoplay.bmp` and handles judgement messages (`'JDGE'`).*

## 2. 확인됨: 채널 모드 설정 함수 `0x0041dc80` / Confirmed: the channel mode setter `0x0041dc80`

`0x0041dc80(index, value)`는 곡 객체의 채널 배열에 값을 쓴다. 인덱스가 음수이거나 `[ecx + 0x321dc]` 이상이면 아무것도 하지 않는다.

*`0x0041dc80(index, value)` writes a value into the song object's channel array, doing nothing when the index is negative or at least `[ecx + 0x321dc]`.*

* `DemoGame::OnCreateGame`은 채널 3~`0x12`를 **조건 없이 1**로 쓴다(`0x004245dc`부터).
* `NormalGame::OnCreateGame`은 `[0x007fa424] == 1`일 때만 같은 값을 쓰고, 아니면 모두 0으로 쓴다(`0x004226c8`의 비교).

*`DemoGame::OnCreateGame` writes **1 unconditionally** to channels 3-`0x12` (from `0x004245dc`), while `NormalGame::OnCreateGame` writes the same values only when `[0x007fa424] == 1` and zeros otherwise (the comparison at `0x004226c8`).*

## 3. 확인됨: autoplay 플래그 `0x007fa424` / Confirmed: the autoplay flag at `0x007fa424`

**쓰기 2곳.** `0x00421fa0`(0)과 `0x00422080`(1)이며 같은 루틴의 두 갈래다. 이 루틴은 입력 매니저 `[0x007fa418]`에 슬롯 `0xc`의 상태를 물어 2이면 값을 뒤집고, 그 자리에서 채널 3~`0x12`를 새 값으로 다시 쓴다. 곧 **게임에 내장된 토글**이다. 슬롯 `0xc`의 물리 바인딩은 **미확정**이다. 3rd의 슬롯 `0x1b`와 같은 구조다.

*Two writes, `0x00421fa0` (0) and `0x00422080` (1), are the two branches of one routine that asks the input manager `[0x007fa418]` for slot `0xc`, flips the value when it reads 2, and rewrites channels 3-`0x12` on the spot — **a toggle built into the game**, like 3rd's slot `0x1b`. The physical binding of slot `0xc` is **unresolved**.*

**읽기 8곳.** `NormalGame::OnCreateGame`의 곡 시작 판정 하나와, 판정·입력 이벤트 처리 여섯 곳(`0x00423be6`, `0x00423d52`, `0x00423dea`, `0x004258c6`, `0x004259e5` 등)이다. 후자는 값이 0일 때만 눌린 채널을 0으로, 떼면 1로 바꾼다. 값이 1이면 이벤트가 채널 모드를 건드리지 않아 자동 상태가 유지된다.

*Eight reads: the song-start test in `NormalGame::OnCreateGame`, and six judgement and input event handlers (`0x00423be6`, `0x00423d52`, `0x00423dea`, `0x004258c6`, `0x004259e5` among them) which, only when the value is 0, set the pressed channel to 0 and back to 1 on release. With the value 1 the events leave the channel modes alone, so the automatic state holds.*

**EZ2DJ 계열과 다른 점.** 데모는 이 플래그를 쓰지 않고 상수를 쓴다. 그래서 3rd~5th·1st SE와 달리 **어트랙트를 지켜봐도 값이 변하지 않으며**, 읽기 폴링으로는 확인할 수 없다.

*How it differs from the EZ2DJ builds: the demo uses constants rather than this flag, so unlike 3rd-5th and 1st SE **the value does not move during attract** and read-only polling cannot confirm it.*

**제품 경로 확인.** 프로파일에 RVA `0x003fa424`와 timestamp `0x3a5f074c`를 선언하자 런처가 무장했고(`autoplay_armed: true`), 사용자가 OSD 토글로 **자동 연주됨을 확인했다.** 값은 `NormalGame::OnCreateGame`이 읽으므로 **곡을 시작할 때 반영된다**(구조에 따른 추정, 진행 중인 곡에서는 바뀌지 않았다).

*Product path. Declaring RVA `0x003fa424` with timestamp `0x3a5f074c` armed the control (`autoplay_armed: true`), and the user **confirmed automatic play through the OSD toggle**. Since `NormalGame::OnCreateGame` reads it, the value takes effect **when a song starts** (inferred from the structure; a song already running did not change).*

## 4. 미확정 / Unresolved

* 입력 슬롯 `0xc`의 물리 바인딩.
* 채널 값 1이 "노트를 자동으로 친다"인지 "키음을 자동으로 낸다"인지. 결과는 같아 보이지만 채널 배열의 의미 자체는 확인하지 못했다.
* 데모 Director가 판정 메시지를 만들어 내는 지점.

*The physical binding of input slot `0xc`; whether a channel value of 1 means hitting notes automatically or sounding them automatically, since the outcome looks the same but the array's meaning was not established; and where the demo Director's judgement messages originate.*
