# 작업 295 작업 로그 — autoplay 변수 탐색 / Task 295 work log — Hunting the autoplay variable

작업 지시: [20260917-295-autoplay-variable-hunt.md](../work-orders/20260917-295-autoplay-variable-hunt.md)
분석: [EZ2DJ 3rd 데모 플레이와 설정 레지스트리](../analysis/ez2dj3rd-demo-play.md)

## 한국어

### 방법

작업 292의 3rd `resumed` 덤프(virtual 레이아웃, 파일 오프셋 = RVA)를 대상으로 scratchpad의 Python 스크립트로 분석했다. 역어셈블은 사용자 환경에 이미 설치된 Capstone을 썼고 저장소에는 아무것도 추가하지 않았다. 런타임 확인은 외부 프로세스에서 `ReadProcessMemory`로 **읽기만** 했다.

### 1. 설정은 키-주소 레지스트리이고 autoplay 키가 없다 — 확인됨

INI 키 문자열의 참조를 따라가니 `push 주소; push "키"; mov ecx, [0x004edbc8]; call 접근자` 패턴이 나왔다. 이 패턴을 바이트로 전수 수집한 결과 접근자 5개에 묶인 키가 **INI의 25개 키와 정확히 같은 집합**이었다. autoplay는 저장 설정이 아니다.

실행 파일이 MSVC 디버그 구성(`0xcccccccc` 채움, 스택 검사, incremental-link thunk)이라 구조가 잘 드러났다.

### 2. 데모 플래그 `0x00a2946c` — 정적·런타임 모두 확인됨

유일한 데모 관련 문자열 `..\..\Common\DEMOPLAY.bmp`의 참조를 따라가니, 한 클래스 생성자가 `[0x00a2946c] != 0`일 때만 그 이미지를 로드했다. 이 전역의 참조는 쓰기 2곳·읽기 10곳이다.

쓰기 중 `0x0048aa31` 함수는 **데모 시작 루틴**이다. 플래그를 1로 세우고 데모 믹스 인덱스 `[0x00a31e08]`을 5개 믹스에 걸쳐 돌리며 믹스별 고정 데모곡을 고른다.

어트랙트에서 150초 동안 두 전역을 외부에서 읽었다. 플래그는 50.3~72.9초, 112.2~137.7초에 1이었고, 데모가 시작될 때마다 믹스 인덱스가 0→1→2로 올랐다.

### 3. 플래그를 읽는 곳의 역할

읽기 10곳은 데모 오버레이 로드, 입력 바인딩 해제(공통·1P·2P), 소리 끄기, 입력 시 데모 종료, 곡 선택 휠 무시, 기록 생략 계열이다. 확인됨/추정 구분은 분석 문서 3절 표에 있다.

중간에 잘못 짚은 곳이 두 번 있었다. `0x00434a23`·`0x00434ae9`를 처음엔 플레이어별 autoplay 스위치로 봤으나 입력 매니저의 **바인딩 해제**였고, `0x0043f9ae`는 판정이 아니라 **곡 선택 휠** 처리였다. 둘 다 역어셈블을 끝까지 읽고 정정했다.

### 4. 노트 자동 판정 지점 — 미확정 (6절에서 해소)

플래그를 직접 읽는 10곳 어디에도 노트 판정이 없다. 데모가 해제하는 바인딩도 플레이어당 두 슬롯뿐이다. 따라서 자동 판정은 (a) 데모 경로가 노트 키 레코드의 상태 필드를 직접 쓰거나 (b) 판정이 별도 플레이어 모드 값을 읽는 방식으로 **추정**한다.

다음 확인 방법은 데모 중 노트 키 상태 필드 쓰기를 `--field-write-watch`로 감시하는 것이다. 이는 실행 중 디버거 감시가 필요하므로 이번에 하지 않았다.

### 5. 강제 toggle에 대한 함의

`[0x00a2946c]`는 "자동으로 친다"가 아니라 "데모 장면이다"를 뜻한다. 실제 플레이 중 1로 쓰면 입력 시 장면이 종료되고 오버레이·음소거가 따라올 것으로 **추정**한다. 원하는 스위치는 4절의 자동 판정 지점을 찾은 뒤에 정해야 한다.

### 6. 후속 — autoplay 플래그 `0x00a29508`을 찾았다 — 확인됨

`--field-write-watch`는 진입 시점에 고정 주소로 무장하고 detached 실행과 함께 쓸 수 없어서, 힙에 있는 입력 레코드 감시에는 맞지 않았다. 대신 데모 시작 루틴을 끝까지 읽었다.

그 루틴은 게임 장면을 동기 실행하는 래퍼였고, 데모 플래그와 **짝으로** `[0x00a29508]`을 1로 세웠다가 0으로 되돌린다. 이 값의 getter 호출처 중 `0x0004e2c2`는 노트가 도착하면 **게임이 직접** 키 누름·판정 연출을 호출하고, `0x0004e3a1`은 노트 종료 시 키 뗌 연출을 호출한다. 사람 입력 없이 노트를 치는 동작이다.

런타임 읽기에서도 데모 구간(38.6~60.6초)에 데모 플래그와 같이 1이 됐다.

덤으로 **게임 안에 autoplay 토글 입력이 있다.** 입력 매니저의 매 프레임 갱신 `0x004344c4`가 슬롯 `0x1b`가 눌리면 이 값을 뒤집는다. 그 슬롯의 물리 바인딩은 미확정이다.

강제 toggle은 데모 플래그가 아니라 **이 값만** 대상으로 해야 할 것으로 추정한다. 실제 쓰기 시험은 작업 지시의 "관찰만" 제약에 따라 하지 않았다.

### 검증

* 코드 변경 없음. 빌드와 테스트는 작업 294 시점과 같다.
* 모든 주소는 TimeDateStamp `0x3bca98a3` 빌드에 한정된다.

## English

Work order: [20260917-295-autoplay-variable-hunt.md](../work-orders/20260917-295-autoplay-variable-hunt.md)
Analysis: [EZ2DJ 3rd demo play and the settings registry](../analysis/ez2dj3rd-demo-play.md)

### Method

Task 292's 3rd `resumed` dump (virtual layout, file offset = RVA) was analyzed with Python scripts in the scratchpad, disassembling with the Capstone already installed in the user's environment; nothing was added to the repository. Run-time confirmation only **read** memory, from an outside process, with `ReadProcessMemory`.

### 1. Settings are a key-to-address registry with no autoplay key — confirmed

Following references to the INI key strings led to the pattern `push address; push "key"; mov ecx, [0x004edbc8]; call accessor`. Collecting it exhaustively by bytes showed the keys bound through five accessors to be **exactly the INI's 25 keys**. Autoplay is not a stored setting. The executable is an MSVC debug configuration (`0xcccccccc` fill, stack checks, incremental-link thunks), which kept the structure readable.

### 2. The demo flag at `0x00a2946c` — confirmed statically and at run time

The only demo-related string, `..\..\Common\DEMOPLAY.bmp`, led to a class constructor that loads it only when `[0x00a2946c] != 0`. The global has two writes and ten reads. One writer, function `0x0048aa31`, is the **demo start routine**: it sets the flag to 1, rotates the demo mix index `[0x00a31e08]` across five mixes, and picks each mix's fixed demo song.

Reading both globals from outside for 150 seconds in attract showed the flag at 1 over 50.3-72.9 s and 112.2-137.7 s, with the mix index rising 0 → 1 → 2 at each demo start.

### 3. What the readers do

The ten reads cover loading the demo overlay, unbinding input (shared, 1P, 2P), muting, ending the demo on input, ignoring the song-select wheel, and skipping records; the confirmed/inferred split is in the analysis document's section 3 table.

Two wrong turns were corrected along the way by reading each disassembly through: `0x00434a23` and `0x00434ae9` first looked like per-player autoplay switches but **unbind input** in the input manager, and `0x0043f9ae` handles the **song-select wheel** rather than judgement.

### 4. The note auto-hit site — unresolved (resolved in section 6)

None of the ten direct reads performs note judgement, and demo unbinds only two slots per player. Auto-hit is therefore **inferred** to work either by (a) the demo path writing the note keys' state field directly or (b) judgement reading a separate per-player mode value. The next check is watching writes to a note key's state field during demo with `--field-write-watch`, which needs a debugger watch on a running game and was not done here.

### 5. What this means for a forced toggle

`[0x00a2946c]` means "this is the demo scene", not "hit notes automatically". Writing `1` during a real play is **inferred** to end the scene on the next input and bring the overlay and muting along. The switch actually wanted should be chosen after section 4's site is found.

### 6. Follow-up — found the autoplay flag at `0x00a29508` — confirmed

`--field-write-watch` arms a fixed address at entry and cannot run detached, so it did not suit a heap-allocated input record. The demo start routine was read through instead. It turned out to wrap the game scene synchronously and to set `[0x00a29508]` to 1 and back to 0 **paired** with the demo flag. Among that value's getter call sites, `0x0004e2c2` has **the game itself** call the key-press and judgement effects when a note arrives, and `0x0004e3a1` calls the key-release effect when the note ends: hitting notes with no human input. The run-time read showed it at 1 together with the demo flag over the demo span (38.6-60.6 s).

Incidentally, **the game has a built-in autoplay toggle input**: the input manager's per-frame update `0x004344c4` flips the value when slot `0x1b` is pressed. That slot's physical binding is unresolved.

A forced toggle is inferred to belong on **this value alone**, not the demo flag. No actual write was tested, per the work order's observe-only constraint.

### Verification

* No code change; build and test state are those of task 294.
* Every address is specific to the TimeDateStamp `0x3bca98a3` build.
