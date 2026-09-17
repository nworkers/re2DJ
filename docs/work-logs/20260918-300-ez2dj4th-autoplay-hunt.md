# 작업 300 작업 로그 — ez2dj4th autoplay 탐색 / Task 300 work log — ez2dj4th autoplay hunt

작업 지시: [20260918-300-ez2dj4th-autoplay-hunt.md](../work-orders/20260918-300-ez2dj4th-autoplay-hunt.md)
분석: [4th 데모 플레이와 autoplay 플래그](../analysis/ez2dj4th-demo-play.md)
절차: [`game-state-hunt` 스킬](../../.agents/skills/game-state-hunt/SKILL.md)

## 한국어

### 스킬 절차 적용 결과

| 단계 | 결과 |
| --- | --- |
| 1. 덤프 | 작업 294의 `resumed` 덤프, timestamp `0x3d369bfd`, gaps 없음 |
| 2. 저장 설정 배제 | 설정 객체 `[0x005111e0]`에 24개 키, `AutoPlay` 없음. 덤프의 `AutoPlay` 문자열은 참조 없는 런타임 사본 |
| 3. 데모 단서 | `DEMOPLAY.bmp` 로드 조건 `[0x00ac290c]` |
| 4. 데모 시작 루틴 | 쓰기 2곳(1·0)이 모두 `0x004a4430`. 읽기 14곳 |
| 5. 짝 쓰기 | `PAIRED [0x00ac290c]`, `PAIRED [0x00ac29b0]`(setter `0x00437600`), `RESET` 3곳 |
| 6. 판정 경로 | getter `0x004375f0` 호출처 17곳이 3rd의 노트 도착·종료·UI 좌표·슬롯 `0x1b` 토글과 대응 |
| 7. 읽기 폴링 | 데모 구간 51.8~99.2초, 138.2초~에만 두 값이 함께 1 |
| 8·9. 쓰기 시험과 제품 연결 | 프로파일에 RVA `0x006c29b0`·timestamp `0x3d369bfd` 선언, 무장 확인, 사용자가 OSD로 autoplay 동작 확인 |

8단계와 9단계를 한 번의 플레이로 합쳤다. 프로파일 선언은 게스트에 아무것도 쓰지 않으므로 먼저 준비했고, 사용자가 곡 선택 화면에서 OSD 토글을 체크한 것이 쓰기 시험이 됐다. 실패하면 선언을 되돌리기로 했다.

### 스킬 개선

* **`settings_registry.py` 추가.** 2단계에서 설정 객체에 묶인 키 전체를 한 번에 나열한다. 버전마다 반복되는 수작업이었다.
* **`dumplib.instruction_covering` 수정.** 4th setter의 `mov dword ptr [0x00ac29b0], eax`(`A3` 1바이트 opcode)를 긴 물러남에서 앞 명령의 마지막 바이트를 삼켜 `or byte ptr [ebx + 0xac29b0], ah`로 복원했다. 레지스터 없는 절대 주소 피연산자로 복원되는 해석을 먼저 고르게 고쳤다. 3rd 데모 플래그(12/2/10)와 4th 데모 플래그(16/2/14) 결과가 그대로임을 확인했다.
* `SKILL.md`에 새 도구, 참조 없는 키 문자열의 함정, 확인된 결과 표를 추가했다.

### 이전 문서 정정 — 3rd 설정 키는 24개

`settings_registry.py`를 3rd 덤프로 검증하다 **3rd도 24개**임을 발견했다. 작업 295에서 "INI의 25개 키와 정확히 같은 집합"이라고 적은 것은 틀렸다. INI에만 있는 `UseIOCard`는 코드가 참조하지 않으며 덤프에는 참조 없는 런타임 사본만 있다. autoplay가 저장 설정이 아니라는 결론은 바뀌지 않는다.

작업 295 로그는 시간순 증거이므로 두고, 누적 문서인 3rd 분석 문서와 `EXE_DESIGN.ko.md`·`.en.md`를 정정했다.

### 검증

* Windows x86 Debug 빌드: 오류·경고 0건.
* 단위 테스트: `checks: 1806, failures: 0`. 4th 값, 3rd timestamp로 4th 주소가 무장되지 않음을 추가로 검사한다.
* 실행: `osd_controls`에서 `build_timestamp` = `executable_timestamp` = `0x3d369bfd`, `autoplay_armed: true`.
* 사용자 확인: OSD로 autoplay를 켜 곡이 자동 연주됨. 데모 현상에 대한 별도 보고는 받지 않았다.

### 남은 것

* 4th가 값을 곡 시작 때 고정하는지(곡 도중 끄기 시험 안 함).
* 판정 계열 두 벌의 의미.
* 다음 후보 `ez2dj5th`.

## English

### Applying the skill

1. Dump: task 294's `resumed` dump, timestamp `0x3d369bfd`, no gaps.
2. Stored setting ruled out: 24 keys on settings object `[0x005111e0]`, no `AutoPlay`; the dump's `AutoPlay` string is an unreferenced run-time copy.
3. Demo clue: the `DEMOPLAY.bmp` load guard `[0x00ac290c]`.
4. Demo start routine: both writes (1 and 0) in `0x004a4430`; 14 reads.
5. Paired writes: `PAIRED [0x00ac290c]`, `PAIRED [0x00ac29b0]` (setter `0x00437600`), three `RESET`.
6. Judgement path: the 17 sites of getter `0x004375f0` correspond to 3rd's note arrival, note end, UI coordinate and slot `0x1b` toggle.
7. Read-only poll: both values 1 together only during demos, 51.8-99.2 s and from 138.2 s.
8-9. Write test and wiring: RVA `0x006c29b0` and timestamp `0x3d369bfd` declared, the control armed, and the user confirmed autoplay through the OSD.

Steps 8 and 9 were merged into one play: declaring the profile writes nothing to the guest, so it was prepared first, and the user's tick of the OSD toggle at song select was the write test, with the declaration to be reverted on failure.

### Skill improvements

* **Added `settings_registry.py`**, listing every key bound to a settings object in one run for step 2, previously manual for each version.
* **Fixed `dumplib.instruction_covering`.** A long back-off swallowed the preceding instruction's last byte and recovered 4th's setter `mov dword ptr [0x00ac29b0], eax` (the one-byte `A3` opcode) as `or byte ptr [ebx + 0xac29b0], ah`. Decodings with a pure absolute operand are now preferred; the 3rd demo flag (12/2/10) and 4th demo flag (16/2/14) results are unchanged.
* `SKILL.md` gained the new tool, the unreferenced-key-string pitfall, and a table of confirmed results.

### Correcting an earlier document — 3rd has 24 setting keys

Validating `settings_registry.py` on the 3rd dump showed **3rd also has 24**. Task 295's "exactly the same set as the INI's 25 keys" was wrong: `UseIOCard`, present only in the INI, is not referenced by code, and the dump holds only an unreferenced run-time copy of it. The conclusion that autoplay is not a stored setting stands. Task 295's log is chronological evidence and is left as is; the cumulative 3rd analysis and `EXE_DESIGN.ko.md`/`.en.md` are corrected.

### Verification

* Windows x86 Debug build: no errors or warnings.
* Unit tests: `checks: 1806, failures: 0`, now also checking the 4th values and that the 3rd timestamp does not arm the 4th address.
* Run: `osd_controls` shows `build_timestamp` = `executable_timestamp` = `0x3d369bfd` and `autoplay_armed: true`.
* User check: turning autoplay on through the OSD made the song play itself. No separate report was received on demo effects.

### Remaining

* Whether 4th latches the value at song start (no mid-song switch-off was tested).
* What the two sets of judgement sites mean.
* The next candidate, `ez2dj5th`.
