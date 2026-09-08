# 작업 로그: 페이드 블렌드 인자 보존

## 한국어

### 관련 문서

- 설계: [페이드 블렌드 인자 보존 설계](../design/20260908-235-fade-blend-factors.md)
- 작업 지시: [페이드 블렌드 인자 보존](../work-orders/20260908-235-fade-blend-factors.md)
- 선행 작업: [프레임별 draw 요약 진단](20260908-234-frame-draw-summary.md)

### 해결한 문제

사용자가 보고한 첫 번째 문제, Warning에서 로고로 넘어갈 때의 비정상적인 깜빡임입니다. 원인은 두 가지였고 둘 다 우리 쪽 호환 처리에 있었습니다.

### 진단 경로

[작업 234](20260908-234-frame-draw-summary.md)의 프레임별 기록에서 게스트가 페이드 구간에도 매 프레임 그린다는 것과, 오버레이 alpha가 `0xff`에서 `0x0c`씩 줄어 `0x0f`에 이른다는 것을 확인했습니다.

`RenderState` 기록은 상태별로 값이 바뀔 때만 남으므로, `state=27`(`ALPHABLENDENABLE`)이 프레임 201에 처음 나타난다는 것은 그 전까지 호출이 없었다는 뜻입니다. `state=19`·`state=20`은 프레임 0에 `ZERO`·`SRCALPHA`로 설정됩니다.

**확인됨.** 게스트가 요청한 연산은 `dst × srcAlpha`입니다.

### 결함과 수정

| 결함 | 증상 | 수정 |
| --- | --- | --- |
| 호환 경로가 게스트 인자를 `SRCALPHA / INVSRCALPHA`로 덮어씀 | `dst × (1 − a)`가 되어 페이드가 반전. Warning이 어두워지는 대신 밝아짐 | 게스트 인자가 해석되면 그대로 사용 |
| `IsFullScreenBlackFadeCandidate`가 alpha `0xff`를 배제 | 페이드 첫 단계(프레임 180)가 블렌딩 없이 불투명 검정으로 그려져 한 프레임이 완전히 검게 나옴 | 게스트가 인자를 지정한 경우에만 `0xff` 허용 |

두 번째 배제는 게스트가 인자를 지정하지 않은 경우에는 그대로 둡니다. 그때 불투명한 검은 사각형은 화면을 지우려는 정상적인 draw일 수 있고, 이를 페이드로 처리하면 정상 draw를 지우게 됩니다.

### 검증 — 결과

`re2dj ez2dj1stse` 부팅 구간을 burst 캡처해 프레임 평균 밝기를 측정했습니다.

수정 전:

```
b003: 0 ×7 → 2 5 6 8 10 12 14 → 14 유지
b006: 14 ×5 → 0 → 13 11 9 7 5 3 → 0 → 1 2 4 6 ... 42
```

`14` 유지 뒤에 검은 프레임 하나가 들어가고, 그 다음에야 페이드가 시작됩니다.

수정 후:

```
b005: 14 ×26 → 12 10 8 6 4 2 → 14 → 0 0 1 1 2 4 6
b007: 16 18 20 ... 51 → 45 52 55 58 ... 91
```

**페이드 시작 지점의 검은 프레임이 사라졌습니다.** `14`에서 `12 10 8 6 4 2`로 단조 감소합니다. 페이드 방향도 밝아졌다가 어두워지는 정상 순서입니다.

### 검증 — 시험과 회귀

- Windows x86 Release 전체 build 성공
- `re2dj_unit_tests.exe` → `checks: 1421, failures: 0`
- `re2dj_windows_product_loader_probe.exe` → 4개 항목 ok
- `re2dj_windows_vfs_runtime_probe.exe --vfs-enumeration-only` → exit 0
- 3rd·4th를 실행해 attract 화면을 캡처했습니다. 두 제품 모두 정상이며 회귀가 없습니다.

### 남은 과제 — 프레임 201

수정 후 캡처의 `... 4 2 → 14 → 0 0 ...`에서 `14`가 프레임 201입니다. 페이드가 `2`까지 내려간 직후 한 프레임만 전체 밝기로 돌아갔다가 로고 장면이 검정에서 시작합니다.

기록상 프레임 201은 `draws=1 textured=1 diffuse=0xffffffff`로, 프레임 150–179의 유지 구간과 같은 draw입니다. 즉 게스트가 페이드를 끝낸 뒤 Warning을 전체 밝기로 한 번 더 그립니다. 이 시점에는 게스트가 아직 `ALPHABLENDENABLE`을 켜지 않았고, 12개의 `SetRenderState` 호출은 이 프레임을 present한 뒤에 일어납니다.

**미확정.** 게스트 자신의 draw이므로 원본 하드웨어에서도 같은 한 프레임이 보였는지 확인하지 못했습니다. 원본과 비교할 근거를 확보하기 전에는 이 draw를 숨기지 않습니다. 숨기면 게스트 로직을 임의로 바꾸는 것이 됩니다.

### 남은 과제 — 그 밖

- 게임플레이 화면 배경이 단색 위 실루엣으로 보입니다.
- 프로파일 쓰기(`WritePrivateProfileStringA`)의 overlay 정책.
- 정적 IAT 패치가 packed build에서 무효가 되는 다른 경계 점검.

## English

### Related documents

- Design: [Preserving the Guest's Fade Blend Factors](../design/20260908-235-fade-blend-factors.md)
- Work order: [Preserving the Guest's Fade Blend Factors](../work-orders/20260908-235-fade-blend-factors.md)
- Preceding task: [Per-frame Draw Summary Diagnostic](20260908-234-frame-draw-summary.md)

### Problem solved

The user's first reported problem, the abnormal flicker on the Warning-to-logo transition. It had two causes, both in our compatibility handling.

### Diagnostic path

The per-frame records from [task 234](20260908-234-frame-draw-summary.md) showed the guest draws every frame through the fade and that the overlay alpha steps down from `0xff` to `0x0f` in decrements of `0x0c`.

`RenderState` records are written only when a state's value changes, so `state=27` (`ALPHABLENDENABLE`) first appearing at frame 201 means it was never called before then. `state=19` and `state=20` are set at frame 0 to `ZERO` and `SRCALPHA`.

**Confirmed.** The operation the guest asked for is `dst × srcAlpha`.

### Defects and fixes

The compatibility path overrode the guest's factors with `SRCALPHA / INVSRCALPHA`, giving `dst × (1 − a)` and running the fade backwards — the Warning brightened where it should have darkened. It now uses the guest's factors whenever they decode.

`IsFullScreenBlackFadeCandidate` rejected an alpha of `0xff`, so the fade's first step at frame 180 was drawn unblended as opaque black and one frame came out fully black. An alpha of `0xff` is now accepted, but only when the guest named the factors itself. The exclusion stays for a guest that named none: there an opaque black quad may be a legitimate draw meant to clear the screen, and treating it as a fade would erase that draw.

### Verification — result

Burst captures of the `re2dj ez2dj1stse` boot sequence measured mean frame brightness. Before the fix, the sequence held at `14` and then dropped to a single black frame before the fade began. After the fix it steps `14` down through `12 10 8 6 4 2` with no black frame at the start of the fade, and the fade runs in the correct order — in, then out.

### Verification — tests and regression

The full Windows x86 Release build succeeded, `re2dj_unit_tests.exe` reported `checks: 1421, failures: 0`, the product-loader probe reported all four items ok, and `re2dj_windows_vfs_runtime_probe.exe --vfs-enumeration-only` exited 0. 3rd and 4th were run and their attract screens captured; both are correct, with no regression.

### Remaining work — frame 201

In the post-fix capture, the `14` in `... 4 2 → 14 → 0 0 ...` is frame 201. Immediately after the fade reaches `2`, one frame returns to full brightness before the logo scene starts from black.

The record for frame 201 is `draws=1 textured=1 diffuse=0xffffffff`, the same draw as the held Warning in frames 150 to 179 — the guest draws the Warning at full brightness once more after finishing the fade. At that point it has still not enabled `ALPHABLENDENABLE`, and its twelve `SetRenderState` calls happen after this frame is presented.

**Unresolved.** It is the guest's own draw, and whether the same single frame appeared on original hardware has not been established. The draw is not suppressed without evidence to compare against, because suppressing it would be changing guest logic on a guess.

### Remaining work — other

- The gameplay background renders as silhouettes over a flat colour.
- The overlay policy for profile writes such as `WritePrivateProfileStringA`.
- Auditing other boundaries whose static IAT patch is overwritten on a packed build.
