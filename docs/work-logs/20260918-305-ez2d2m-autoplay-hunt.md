# 작업 305 작업 로그 — ez2d2m autoplay 탐색 / Task 305 work log — ez2d2m autoplay hunt

작업 지시: [20260918-305-ez2d2m-autoplay-hunt.md](../work-orders/20260918-305-ez2d2m-autoplay-hunt.md)
분석: [EZ2Dancer 2nd MOVE 데모와 autoplay 플래그](../analysis/ez2d2m-demo-play.md)
절차: [`game-state-hunt` 스킬](../../.agents/skills/game-state-hunt/SKILL.md)

## 한국어

### 스킬이 맞은 곳과 맞지 않은 곳

| 단계 | 결과 |
| --- | --- |
| 1. 덤프 | 작업 294의 `resumed` 덤프, timestamp `0x3a5f074c`, gaps 없음 |
| 3. 데모 단서 | `DemoGame::OnCreateGame`·`OnDestroyGame`, `DemoGameDirector::Create`·`Delete`, `system\common\demoplay.bmp` |
| 3-1. 장면 엔진 | **맞지 않음.** 등록 함수가 없고 클래스마다 초기화·종료와 Director를 둔다 |
| 대체 경로 | `DemoGame`과 `NormalGame`의 `OnCreateGame`을 나란히 비교. 전역 참조와 상수 `push`만 추려 비교하니 일반 쪽에만 있는 `cmp [0x007fa424], 1` 분기가 드러남 |
| 5. 짝 쓰기 | 해당 없음. 데모는 플래그를 쓰지 않고 채널을 상수로 설정 |
| 6. 판정 경로 | 읽기 8곳 — 곡 시작 판정과 판정·입력 이벤트 여섯 곳. 값이 0일 때만 이벤트가 채널 모드를 바꾼다 |
| 7. 읽기 폴링 | **쓸 수 없음.** 데모가 플래그를 쓰지 않아 어트랙트에서 값이 움직이지 않는다 |
| 8. 쓰기 시험 | 사용자 동의 후 1을 씀. 해당 프로세스가 곧 종료되어 **효과를 귀속하지 못함** |
| 9. 제품 연결 | RVA `0x003fa424`·timestamp `0x3a5f074c` 선언, `autoplay_armed: true`, 사용자가 OSD 토글로 자동 연주 확인 |

### 잘못 짚었다가 고친 것

* 처음에는 `DemoGame::OnCreateGame`이 1을 쓰는 객체 필드 `+0x1015d8`을 후보로 봤으나, `NormalGame`도 같은 값을 써서 기각했다.
* 장면 전환 전역 `[0x007fa120]`·`[0x007fa128]`·`[0x007fa12c]`도 여러 클래스가 쓰므로 기각했다.
* 화면 파라미터 setter `0x0041c530`은 타이틀 화면도 데모와 같은 인자를 넘겨 기각했다.
* 쓰기 시험 도중 사용자가 "안 된다"고 해 후보를 접으려 했으나, 이어서 "다시 하니 된다"는 확인이 왔다. 그런데 **폴링 기록상 값을 쓴 프로세스는 그 전에 종료**되어 있었다. 이 확인은 귀속이 불가능하다고 보고, 프로파일에 선언한 뒤 OSD로 다시 확인했다.

### 스킬 개선

* `SKILL.md`에 **3-2 클래스 쌍 경로**를 추가했다. 데모·일반 클래스의 초기화를 비교하는 방법, 레지스터 배정 차이 때문에 전역 참조와 상수만 추려야 한다는 점, 데모가 플래그를 쓰지 않으면 7단계를 건너뛴다는 점을 적었다.
* 결과 표에 `ez2d2m` 행을 넣었다.

### 검증

* Windows x86 Debug 빌드: 오류·경고 0건.
* 단위 테스트: `checks: 1821, failures: 0`. `ez2d2m` 값을 검사하고 다른 timestamp로는 무장하지 않음을 확인하며, 무선언 목록에서 뺐다.
* 실행: `osd_controls`에서 `build_timestamp` = `executable_timestamp` = `0x3a5f074c`, `autoplay_armed: true`.
* 사용자 확인: OSD 토글로 자동 연주됨.

### 남은 것

* 입력 슬롯 `0xc`의 물리 바인딩.
* 채널 값 1의 정확한 의미.
* 남은 타깃은 `ez2dj2nd`(덤프 미수집)와 `ez2dj6th`(자식 프로세스 구조).

## English

### Where the skill fit and where it did not

The dump was task 294's `resumed` dump (timestamp `0x3a5f074c`, no gaps). The clues were class method names (`DemoGame::OnCreateGame`, `DemoGameDirector::Create`) and `system\common\demoplay.bmp`. Path 3-1 **did not fit**: there is no registration function, each class carrying its own init, destroy and Director. The substitute path compared `DemoGame`'s and `NormalGame`'s `OnCreateGame` side by side; reducing both to global references and constant pushes exposed a `cmp [0x007fa424], 1` branch present only in the normal one. Step 5 did not apply, since the demo sets the channels with constants instead of the flag. The flag's eight reads are the song-start test and six judgement and input event handlers, which touch channel modes only when it is 0. Step 7 **could not be used** at all, as the value does not move during attract. A write test with the user's consent was inconclusive, because the process written to had exited before the user's confirmation. Declaring RVA `0x003fa424` with timestamp `0x3a5f074c` armed the control, and the user confirmed automatic play through the OSD toggle.

### Wrong turns

The object field `+0x1015d8` set to 1 by `DemoGame::OnCreateGame` was rejected once `NormalGame` proved to write it too; the scene-transition globals `[0x007fa120]`, `[0x007fa128]` and `[0x007fa12c]` were rejected as shared by many classes; and the screen parameter setter `0x0041c530` was rejected because the title screen passes the demo's arguments. The write test first looked like a failure and then like a success, but the poll log showed the written process had already exited, so that confirmation was treated as unattributable and the OSD check was used instead.

### Skill improvements

* `SKILL.md` gained a **paired-class path (3-2)**: how to compare the demo and normal inits, why the comparison must be reduced to global references and constants (register allocation differs), and that step 7 is skipped when the demo does not use the flag.
* The results table gained an `ez2d2m` row.

### Verification

* Windows x86 Debug build: no errors or warnings.
* Unit tests: `checks: 1821, failures: 0`, checking the `ez2d2m` values, that a different timestamp arms nothing, and removing it from the undeclared list.
* Run: `osd_controls` reports `build_timestamp` = `executable_timestamp` = `0x3a5f074c` and `autoplay_armed: true`.
* User confirmation: automatic play through the OSD toggle.

### Remaining

* The physical binding of input slot `0xc`, the exact meaning of a channel value of 1, and the remaining targets `ez2dj2nd` (no dump) and `ez2dj6th` (child-process structure).
