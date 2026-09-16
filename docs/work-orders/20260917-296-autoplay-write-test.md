# 작업 296 작업 지시 — autoplay 플래그 쓰기 시험 / Task 296 work order — Autoplay flag write test

선행: [작업 295 autoplay 변수 탐색](20260917-295-autoplay-variable-hunt.md)
상태: **완료.** [작업 로그](../work-logs/20260917-296-autoplay-write-test.md)
분석: [EZ2DJ 3rd 데모 플레이와 설정 레지스트리](../analysis/ez2dj3rd-demo-play.md)

## 한국어

### 목적

작업 295는 `[0x00a29508]`이 노트를 자동으로 치게 하는 플래그라고 코드와 읽기 관찰로 결론 냈으나, 실제로 값을 써서 확인하지는 않았다. 이 작업은 **실제 플레이 중 그 값만 1로 써서** autoplay가 되는지, 데모 부작용이 없는지 시험한다.

### 방법

* scratchpad의 Python 스크립트가 실행 중인 `EZ2DJ.EXE`에 `WriteProcessMemory`로 쓴다. `status`(읽기), `on`(1 쓰고 재확인), `off`(0 쓰고 재확인)만 한다.
* 게임 입력(코인, 시작, 곡 선택, 플레이 관찰)은 사용자가 직접 한다.
* 쓰기 시점은 코인 투입 후 곡 선택 화면이다. 어트랙트 중에는 데모 종료가 값을 0으로 되돌리므로 쓰지 않는다.

### 관찰 항목

1. 쓴 값이 유지되는가.
2. 곡을 시작했을 때 노트가 입력 없이 맞는가.
3. 데모 오버레이, 음소거, 입력 시 장면 종료 같은 데모 부작용이 없는가.
4. 플레이 도중 `off`로 되돌리면 그 뒤로는 자동 판정이 멈추는가.
5. 게임이 종료되거나 보호 계층이 반응하지 않는가.

### 제약

* 저장소에 코드를 추가하지 않는다. 쓸모가 확인되어 제품 기능으로 만들 때는 별도 설계와 작업 지시를 거친다.
* 쓰기 대상은 `[0x00a29508]` 하나뿐이다. 데모 플래그는 쓰지 않는다.
* 플레이 기록은 overlay에만 쓰이며 원본 HDD는 바뀌지 않는다.

### 범위에서 뺀 것

* 슬롯 `0x1b`의 물리 바인딩 탐색.
* 3rd 외 타깃.

## English

Prerequisite: [Task 295, hunting the autoplay variable](20260917-295-autoplay-variable-hunt.md)
Status: **complete.** [Work log](../work-logs/20260917-296-autoplay-write-test.md)
Analysis: [EZ2DJ 3rd demo play and the settings registry](../analysis/ez2dj3rd-demo-play.md)

### Purpose

Task 295 concluded from code and read-only observation that `[0x00a29508]` makes notes get hit automatically, but never wrote the value. This task **writes that value alone to 1 during a real play** to test whether autoplay results and whether demo side effects stay away.

### Method

* A Python script in the scratchpad writes to the running `EZ2DJ.EXE` with `WriteProcessMemory`, doing only `status` (read), `on` (write 1 and read back) and `off` (write 0 and read back).
* The user drives the game input — coin, start, song selection, and watching the play.
* The write happens at song select after a coin is inserted. It is not done during attract, where the end of a demo resets the value to 0.

### What to observe

1. Whether the written value holds.
2. Whether notes are hit without input once a song starts.
3. Whether demo side effects — the overlay, muting, the scene ending on input — stay away.
4. Whether writing `off` mid-play stops the automatic hits from then on.
5. Whether the game exits or the protection reacts.

### Constraints

* No code is added to the repository. If the result proves useful as a product feature, that goes through its own design and work order.
* The only write target is `[0x00a29508]`; the demo flag is never written.
* Play records are written to the overlay only, leaving the original HDD unchanged.

### Out of scope

* Finding slot `0x1b`'s physical binding.
* Targets other than 3rd.
