# EZ2DJ 3rd 데모 플레이와 설정 레지스트리 / EZ2DJ 3rd demo play and the settings registry

주제: 3rd가 어트랙트 데모를 어떻게 켜고 끄는가, 그리고 autoplay에 해당하는 상태가 어디에 있는가.

*Topic: how 3rd switches its attract demo on and off, and where the state that amounts to autoplay lives.*

측정 대상: 3rd `EZ2DJ.EXE` CHD 빌드, TimeDateStamp `0x3bca98a3`, image base `0x00400000`. 모든 주소는 **이 빌드에만** 해당한다.

*Measured on the 3rd `EZ2DJ.EXE` CHD build, TimeDateStamp `0x3bca98a3`, image base `0x00400000`. Every address here belongs to **this build only**.*

근거 작업: [작업 295](../work-logs/20260917-295-autoplay-variable-hunt.md)
자료: [실행 중 주 이미지 덤프](../guides/decrypted-image-dump.md)의 `resumed` 덤프, 외부 읽기 전용 메모리 폴링

---

## 1. 확인됨: 빌드 성격 / Confirmed: build character

이 실행 파일은 MSVC **디버그 구성**이다. 함수마다 지역 변수를 `0xcccccccc`로 채우고 반환 전에 스택 포인터 검사를 부르며, 호출이 incremental-link thunk(`e9 rel32`)를 거친다. 그래서 함수 경계와 호출 대상이 정적으로 잘 드러난다.

*This executable is an MSVC **debug configuration**: each function fills its locals with `0xcccccccc`, calls a stack-pointer check before returning, and routes calls through incremental-link thunks (`e9 rel32`). Function boundaries and call targets therefore read out cleanly.*

## 2. 확인됨: INI 설정은 키-주소 레지스트리다 / Confirmed: INI settings are a key-to-address registry

코드는 `push <주소 또는 값>; push "<키>"; mov ecx, [0x004edbc8]; call <접근자>` 모양으로 INI 키를 전역 설정 객체에 묶는다. 접근자 호출을 바이트 패턴으로 전수 수집했다.

*Code binds INI keys to a global settings object as `push <address or value>; push "<key>"; mov ecx, [0x004edbc8]; call <accessor>`. Every accessor call was collected by byte pattern.*

| 접근자 / accessor | 호출 수 / calls | 성격 (추정) / role (inferred) |
| --- | --- | --- |
| `0x00431a12` | 26 | 정수 읽기, 변수 주소를 받음 / integer read into an address |
| `0x00431830` | 15 | 정수 쓰기 / integer write |
| `0x00431adc` | 2 | 문자열 읽기 / string read |
| `0x00431914` | 2 | 문자열 쓰기 / string write |
| `0x00431b47` | 2 | 문자열 계열 / string family |

묶인 키는 INI 파일의 25개 키 중 **24개**이며, autoplay·demo에 해당하는 키는 없다. 따라서 **autoplay는 저장되는 설정이 아니다.** 나머지 하나인 `UseIOCard`는 코드가 참조하지 않는다. 덤프에 있는 `UseIOCard` 문자열은 참조 없는 런타임 사본뿐이다.

**정정 (작업 300).** 처음에는 "INI의 25개 키와 정확히 같은 집합"이라고 적었다. 작업 300에서 `settings_registry.py`로 전수 수집하자 24개였고 `UseIOCard`가 빠져 있었다. autoplay에 대한 결론은 바뀌지 않는다. [보호 빌드의 런타임 복호화](protected-build-runtime-decryption.md) 4절의 "`AutoPlay`는 INI 키가 아니다"를 레지스트리 수준에서 다시 확인한 것이다.

*The bound keys are **24** of the INI file's 25, with nothing for autoplay or demo; the remaining `UseIOCard` is not referenced by code, and the only `UseIOCard` string in the dump is an unreferenced run-time copy. Correction (task 300): this first read "exactly the same set as the INI's 25 keys"; collecting them exhaustively with `settings_registry.py` in task 300 gave 24 with `UseIOCard` missing, which leaves the autoplay conclusion unchanged. **Autoplay is not a stored setting**, which reconfirms at the registry level section 4 of [runtime decryption in protected builds](protected-build-runtime-decryption.md).*

부수적으로 확인된 변수 주소다. / *Variable addresses confirmed along the way:*

| 키 / key | 주소 / address |
| --- | --- |
| `GameLevel` | `0x004dadf8` |
| `AdvSound` | `0x004dadfc` |
| `EventMode` | `0x00a31dfc` |
| `EventModeMixName` | `0x00a31e00` |
| `EventModeSongNameIdx` | `0x00a31e04` |
| `EventModeSongName` | `0x00a31d70` |
| `TotalCoin` | `0x00a30678` |
| `ServiceCoin` | `0x00a31b90` |
| `TotalPlay` | `0x00a31d6c` |
| `TotalPlayTime` | `0x00a3067c` |

## 3. 확인됨: 데모 플레이 플래그 `0x00a2946c` / Confirmed: the demo-play flag at `0x00a2946c`

**정적 근거.** 문자열 `..\..\Common\DEMOPLAY.bmp`를 참조하는 곳은 한 곳뿐이며, vtable `0x004c2984` 클래스의 생성자가 `[0x00a2946c] != 0`일 때만 그 이미지를 로드한다.

*Static evidence. `..\..\Common\DEMOPLAY.bmp` is referenced exactly once, by the constructor of the class with vtable `0x004c2984`, which loads the image only when `[0x00a2946c] != 0`.*

이 전역에 대한 절대 주소 참조는 모두 12곳이다. **쓰기 2곳, 읽기 10곳**이다.

*There are 12 absolute references to the global: **two writes and ten reads**.*

| RVA | 명령 / instruction | 역할 / role |
| --- | --- | --- |
| `0x0008aa55` | `mov [..], 1` | 데모 시작 루틴 (아래) / demo start routine (below) — **확인됨** |
| `0x0008ac14` | `mov [..], 0` | 데모 해제 / demo cleared — 호출 문맥 **미확정** |
| `0x0003822a` | `cmp [..], 0` | `DEMOPLAY.bmp` 로드 / loads the overlay — **확인됨** |
| `0x00039917` | `cmp [..], 0` | 데모이고 `AdvSound == 0`이면 소리 끄기 / mutes the demo unless `AdvSound` — **추정** |
| `0x00039978`·`0x0003999e`·`0x000399c4` | `sete` | 입력 매핑 해제, 공통·1P·2P / unbinds input for shared, 1P, 2P — **확인됨** |
| `0x00039dc1` | `cmp [..], 0` | 데모 중 입력이 오면 장면 상태를 4로 / an input during demo sets scene state 4 — **추정: 데모 종료** |
| `0x00038ed3` | `cmp [..], 1` | 데모면 목록 루프를 건너뜀 / skips a list loop in demo — 의미 **미확정** |
| `0x0003f9ca` | `cmp [..], 0` | 데모면 곡 선택 휠 조작을 무시 / ignores song-wheel scrolling in demo — **추정** |
| `0x00038e35`·`0x00039736` | `cmp [..], 0` | 데모 전용 분기 / demo-only branches — 의미 **미확정** |

**데모 시작 루틴 `0x0048aa31`** — 확인됨. 플래그를 1로 세우고, 데모 믹스 인덱스 `[0x00a31e08]`을 `(idx + 1) % 5`로 돌린다. 믹스 이름 표 `0x004dae00`은 `RubyMix`, `StreetMix`, `RadioMix`, `ClubMix`, `SpaceMix`이며, 믹스마다 고정 데모곡을 고른다(`ClubMix`→`rfc`, `SpaceMix`→`sandstorm` 등).

*Demo start routine `0x0048aa31` — confirmed. It sets the flag to 1 and advances the demo mix index `[0x00a31e08]` as `(idx + 1) % 5` over the mix table at `0x004dae00` (`RubyMix`, `StreetMix`, `RadioMix`, `ClubMix`, `SpaceMix`), picking a fixed demo song per mix (`ClubMix` → `rfc`, `SpaceMix` → `sandstorm`, and so on).*

**런타임 근거.** 게임을 어트랙트 상태로 150초 두고 두 전역을 외부에서 0.25초 간격으로 **읽기만** 했다.

*Run-time evidence. The game idled in attract for 150 seconds while both globals were **read only**, from outside, every 0.25 seconds.*

| 시각 / time | `[0x00a2946c]` | `[0x00a31e08]` |
| --- | --- | --- |
| 0.00 s | 0 | 0 |
| 50.32 s | **1** | 1 |
| 72.85 s | 0 | 1 |
| 112.16 s | **1** | 2 |
| 137.69 s | 0 | 2 |

데모가 돌 때마다 플래그가 약 22~25초간 1이 되고, 데모가 시작될 때마다 믹스 인덱스가 1씩 오른다. 정적 해석과 일치한다.

*Each demo holds the flag at 1 for about 22-25 seconds and each demo start advances the mix index by one, matching the static reading.*

## 4. 확인됨: 입력 매니저의 구조 / Confirmed: the input manager's layout

전역 `[0x00a28ce8]`의 객체는 논리 키 슬롯마다 `0x14`바이트 레코드를 갖는다.

*The object at global `[0x00a28ce8]` keeps one `0x14`-byte record per logical key slot.*

| 메서드 / method | 동작 / behavior |
| --- | --- |
| `0x00420b09` (`SetBinding` 추정) | `[this + slot*0x14 + 0x18] = value` |
| `0x00420a53` (`GetState` 추정) | `return [this + slot*0x14 + 0x10]` |

데모에서는 슬롯 `6`·`7`(공통), `0xa`·`0xb`(1P), `0x12`·`0x13`(2P)의 바인딩이 `-1`이 된다. 평상시 값은 각각 `10`·`11`, `16`·`17`, `24`·`25`다.

*In demo the bindings of slots `6` and `7` (shared), `0xa` and `0xb` (1P), and `0x12` and `0x13` (2P) become `-1`; otherwise they are `10`/`11`, `16`/`17` and `24`/`25`.*

## 5. 확인됨: autoplay 플래그 `0x00a29508` / Confirmed: the autoplay flag at `0x00a29508`

데모 시작 루틴 `0x0048aa31`은 데모 플래그와 **짝으로** `0x00401a19(1)`을 부르고, 게임 장면(`0x00489468`)을 동기 실행한 뒤 `0x00401a19(0)`을 부르고 데모 플래그를 0으로 되돌린다. `0x00401a19`는 `[0x00a29508] = 인자`만 하는 setter이고, getter는 `0x004353e0`(thunk `0x004021a8`)이다.

*The demo start routine `0x0048aa31` calls `0x00401a19(1)` **paired** with the demo flag, runs the game scene (`0x00489468`) synchronously, then calls `0x00401a19(0)` and clears the demo flag. `0x00401a19` is a setter that only does `[0x00a29508] = argument`, and the getter is `0x004353e0` (thunk `0x004021a8`).*

같은 루틴이 0으로만 초기화하는 전역은 `[0x00a29534]`, `[0x00a29f3c]`, `[0x00a29474]`이며 짝을 이루지 않는다.

*The same routine only zero-initializes `[0x00a29534]`, `[0x00a29f3c]` and `[0x00a29474]`, which are not paired.*

**getter 호출처 7곳의 역할.** / *What the seven getter call sites do:*

| RVA | 동작 / behavior | 판정 / status |
| --- | --- | --- |
| `0x0004e2c2` | 노트 도착 시 키 누름 연출(인자 2)과 판정 연출(인자 `0xa`)을 **게임이 직접** 호출 / on note arrival the game itself calls the key-press effect (2) and the judgement effect (`0xa`) | **확인됨 / confirmed** (코드) |
| `0x0004e3a1` | 노트 종료 시 키 뗌 연출(인자 1) / key-release effect (1) at note end | **확인됨 / confirmed** (코드) |
| `0x0004d5bb`·`0x0004d602` | 키별 가상 호출에 `autoplay == 0` 여부를 인자로 전달 / passes `autoplay == 0` into a per-key virtual call | 의미 **미확정** |
| `0x00039da7` | 게임 장면 갱신 중 `autoplay == 1`이면 메서드 `0x00402356` 호출 / calls `0x00402356` when `autoplay == 1` | 의미 **미확정** |
| `0x00079a95` | autoplay면 UI 요소 y 좌표 70.0 → 430.0 / moves a UI element's y from 70.0 to 430.0 | **확인됨 / confirmed** (코드), 대상 요소 미확정 |
| `0x0003450f` | 토글 (아래) / the toggle (below) | **확인됨 / confirmed** (코드) |

**런타임 근거.** 어트랙트에서 외부 읽기 전용으로 관찰했다. / *Run-time evidence, read-only from outside, in attract:*

| 시각 / time | `[0x00a2946c]` demo | `[0x00a29508]` autoplay |
| --- | --- | --- |
| 0.00 s | 0 | 0 |
| 38.56 s | **1** | **1** |
| 60.59 s | 0 | 0 |

**쓰기 시험 — 확인됨** ([작업 296](../work-logs/20260917-296-autoplay-write-test.md)). 실제 플레이에서 이 값만 곡 선택 화면에서 1로 쓰자 다음 곡의 노트가 입력 없이 맞았다. 곡 도중 0으로 쓰면 **그 곡은 끝까지 자동으로 진행되고 다음 곡부터** 수동이 됐다. 값은 곡 시작 때 고정된다.

*Write test — confirmed ([task 296](../work-logs/20260917-296-autoplay-write-test.md)). Writing only this value to 1 at song select in a real play made the next song's notes get hit without input. Writing 0 mid-song left **that song automatic to the end, with manual play from the next song**: the value is latched at song start.*

**정정.** 위 표에서 `0x0004e2c2`·`0x0004e3a1`의 매 노트 호출이 판정을 켜고 끈다고 본 해석은 이 결과와 맞지 않는다. 매 노트 getter 호출은 누름·뗌 **연출**만 좌우하고 실제 판정은 곡 시작 때 고정된 값을 쓰거나, 곡 시작 경로의 `0x00039da7`·`0x0004d5bb`·`0x0004d602`가 판정 모드를 설정해 두는 것으로 **추정**한다. 어느 쪽인지는 미확정이다.

*Correction. Reading the per-note calls at `0x0004e2c2` and `0x0004e3a1` as what switches judgement on and off does not fit this result. It is **inferred** that either those per-note getter calls govern only the press and release **effects** while judgement uses a value latched at song start, or the song-start call sites `0x00039da7`, `0x0004d5bb` and `0x0004d602` set a judgement mode up front; which one is unresolved.*

**결론.** `[0x00a29508]`이 노트를 자동으로 치게 하는 플래그다. 데모 플래그 `[0x00a2946c]`는 "데모 장면이다"를, `[0x00a29508]`은 "게임이 노트를 친다"를 뜻하며 데모는 둘을 함께 켤 뿐이다.

***Conclusion.** `[0x00a29508]` is the flag that makes notes get hit automatically. The demo flag `[0x00a2946c]` means "this is the demo scene" and `[0x00a29508]` means "the game hits the notes"; the demo simply turns both on.*

## 6. 확인됨: 게임 안에 autoplay 토글 입력이 있다 / Confirmed: the game has a built-in autoplay toggle input

입력 매니저의 매 프레임 갱신 `0x004344c4`는 슬롯 3(코인, `TotalCoin` 증가), 슬롯 2(서비스 코인, `ServiceCoin` 증가)와 함께 **슬롯 `0x1b`가 눌리면(`== 2`) `[0x00a29508] = 1 - [0x00a29508]`** 로 뒤집는다. 조건이 없으므로 장면과 무관하게 동작한다.

*The input manager's per-frame update `0x004344c4` handles slot 3 (coin, increments `TotalCoin`) and slot 2 (service coin, increments `ServiceCoin`), and **when slot `0x1b` is pressed (`== 2`) flips `[0x00a29508] = 1 - [0x00a29508]`**. It is unconditional, so it works in any scene.*

**미확정.** 슬롯 `0x1b`가 어느 물리 입력(I/O 보드 비트 또는 키보드)에 묶이는지는 확인하지 못했다. 실행 중 입력 매니저 객체 `[0x00a28ce8]`(`0x00a2fad0`)를 `slot*0x14 + 0x10` 배치로 읽었을 때 모든 슬롯이 0이었으므로, 레코드 배치 가정이 그 시점에 맞지 않는다.

*Unresolved: which physical input (an I/O board bit or a keyboard key) slot `0x1b` is bound to. Reading the input manager object `[0x00a28ce8]` (`0x00a2fad0`) at run time with the `slot*0x14 + 0x10` layout gave zero for every slot, so that layout assumption did not hold at that point.*

## 7. 강제 toggle에 대한 함의 / What this means for a forced toggle

**제품 경로 (작업 297).** 이 절의 결론은 OSD의 autoplay 토글로 구현됐다. 백틱으로 OSD를 열어 체크한다. 주소는 `ez2dj3rd` 프로파일의 `game_controls`에 빌드 timestamp와 함께 저장되며, 실행 파일이 그 빌드일 때만 토글이 나타난다. [설계](../design/20260917-297-imgui-osd-autoplay.md)

*Product path (task 297). This section's conclusion is implemented as the OSD's autoplay toggle, opened with backtick. The address lives in the `ez2dj3rd` profile's `game_controls` together with the build timestamp, and the toggle appears only when the executable is that build. [Design](../design/20260917-297-imgui-osd-autoplay.md)*

* **데모 플래그 `[0x00a2946c]`를 쓰면 안 된다.** 입력 시 장면 종료, 오버레이, 음소거가 따라온다(3절). — 추정
* **autoplay 플래그 `[0x00a29508]`만 1로 쓰면** 게임이 노트를 친다. — **확인됨 (작업 296).** 단 **곡을 시작하기 전에** 써야 그 곡에 적용되며, 도중에 쓴 값은 다음 곡부터 반영된다. 데모 오버레이·음소거·입력 시 종료 같은 데모 부작용은 없었다(사용자 관찰).
* 코드 수정 없이 켤 수 있는 경로는 6절의 슬롯 `0x1b` 입력이다. 물리 바인딩을 찾으면 HLE 입력 설정만으로 켤 수 있다. — 추정

* *Do **not** write the demo flag `[0x00a2946c]`: ending on input, the overlay and muting come with it (section 3). — inferred*
* *Writing `1` to **only the autoplay flag `[0x00a29508]`** makes the game hit notes — **confirmed (task 296).** It must be written **before a song starts** to apply to that song; a mid-song write takes effect from the next song. No demo side effects — overlay, muting, ending on input — appeared (user observation).*
* *The code-free route is section 6's slot `0x1b` input: once its physical binding is found, it could be switched on through HLE input configuration alone. — inferred*
