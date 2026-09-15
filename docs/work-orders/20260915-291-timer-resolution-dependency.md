# 작업 291 작업 지시 — 타이머 해상도 의존성 규명 / Task 291 work order — Establishing the timer-resolution dependency

선행: [작업 290 게스트 대기 계정](20260915-290-guest-wait-accounting.md)

상태: **완료.** [작업 로그](../work-logs/20260915-291-timer-resolution-dependency.md)와 [설계](../design/20260915-291-timer-resolution-dependency.md)를 참조한다.

## 한국어

### 배경

작업 290은 `ez2dj3rd` 프레임 시간 17.8 ms 중 16.8 ms가 게스트 `Sleep` 안이고, 요청값 평균이 16.3 ms임을 확인했다. 그리고 60 fps 미달의 원인으로 `Sleep` 초과 0.5 ms와 비-수면 구간 초과 0.6 ms를 남겼다.

이 지시서는 그중 `Sleep` 초과분이 **Windows 9x와 NT 계열의 타이머 해상도 차이**에서 오는지 판별한다.

### 지시서 작성 시점에 확인된 사실 — 확인됨 (2026-09-15)

3rd `EZ2DJ.EXE`(CHD 빌드, TimeDateStamp `0x3bca98a3`)의 원본 `.idata`를 해석한 결과다.

| 항목 | 값 |
| --- | --- |
| `WINMM.dll` import 8개 | `mixerOpen`, `mixerClose`, `mixerGetNumDevs`, `mixerGetLineInfoA`, `mixerGetLineControlsA`, `mixerGetControlDetailsA`, `mixerSetControlDetails`, `timeGetTime` |
| 타이밍 관련 `KERNEL32` import | `Sleep`, `GetThreadPriority`, `SetThreadPriority` |
| `timeBeginPeriod` | **없음.** `.idata`뿐 아니라 파일 전체 바이트 검색에서 0건이므로 보호 계층의 `GetProcAddress` 동적 해석 가능성도 없다 |
| 우리 소스의 `timeBeginPeriod`·`NtSetTimerResolution` 호출 | 없음 |

### 가설

**추정 1 — 게스트는 1 ms 기본 해상도를 암묵적으로 전제한다.** `timeGetTime`의 기본 해상도가 Windows 9x에서는 1 ms이고 NT 계열에서는 시스템 타이머 틱(기본 15.6 ms)을 따른다. 게스트는 프레임당 `timeGetTime`을 4번 불러 잘 시간을 계산하면서 `timeBeginPeriod`를 부르지 않는다. Win98에서는 부를 이유가 없었기 때문으로 본다.

**추정 2 — 현재 1 ms 해상도는 우리가 의도한 것이 아니다.** 만약 NT 기본 15.6 ms였다면 16.3 ms 요청은 다음 틱 경계인 약 31 ms로 올림되어 32 fps 근처가 나왔을 것이다. 실측 56 fps는 프로세스가 이미 ~1 ms로 동작 중임을 뜻한다. 우리가 부르지 않았으므로 정적 링크된 SDL3가 올렸을 가능성이 크다.

추정 2가 맞다면 3rd의 현재 타이밍은 **SDL의 부수효과에 얹혀 있는 것**이고, SDL 구성이 바뀌면 조용히 무너진다. 이것이 이 작업의 실질적 동기다.

### 실험 1 — 해상도 공급자 식별

목적: 프로세스의 타이머 해상도를 누가 올리는지 확정한다.

1. 현재 구성으로 `re2dj ez2dj3rd --guest-wait-trace`를 실행하고 `Sleep` 소요 p50과 히스토그램을 기준값으로 기록한다.
2. `NtQueryTimerResolution`으로 실제 현재 해상도를 읽어 진단 로그에 한 줄 남긴다. 게스트 동작은 바꾸지 않는다.
3. `RE2DJ_ENABLE_SDL3_AUDIO=OFF` 빌드로 같은 실행을 반복한다. SDL video까지 빠지는 구성이 가능하면 그것도 본다.
4. 해상도와 `Sleep` p50이 15.6 ms 계열로 튀는지 비교한다.

판정: 3에서 p50이 15.6 ms 배수로 이동하면 추정 2가 확인된다. 이동하지 않으면 다른 공급자를 찾는다.

### 실험 2 — 게스트의 주기 계산 방식 판별

목적: 요청값 16.3 ms가 고정 상수인지 `목표 - 경과` 피드백인지 가른다. 작업 290이 "원본 의도가 정말 60 fps인지"를 미확정으로 남긴 부분을 겨냥한다.

1. 작업 290의 히스토그램 인프라에 프레임별 `(Sleep 요청값, 직전 비-수면 구간 소요)` 쌍을 bounded 하게 기록한다.
2. 두 값의 상관을 본다.

판정: 반비례하면 피드백 루프이며, 이 경우 1 ms 양자화가 평균 16.3 ms를 만드는 경로까지 설명된다. 요청값이 작업 시간과 무관하게 일정하면 목표 주기가 16.67 ms가 아닐 수 있으므로, 그 상수를 원본 코드에서 찾는 것이 다음 단계다.

### 제약

* **관찰만 한다.** `Sleep` 인자도 반환값도 바꾸지 않고 타이머 해상도도 올리지 않는다. 프레임률 개입은 별도 작업이며 AGENTS.md의 원본 게임 로직 보존 원칙과 대조해 따로 판단한다.
* 진단은 작업 289·290과 같이 옵션으로 게이트한다. 옵션이 없으면 아무것도 기록하지 않는다.
* 호출 경로에서는 카운터 증가만 한다.
* `NtQueryTimerResolution`은 진단 전용이며 공용 코어가 아니라 `src/platform/windows/` 아래에 둔다.

### 완료 조건

* 실험 1의 판정과 근거 실행 로그.
* 실험 2의 판정과 근거.
* `docs/kb/` — `timeGetTime`·`Sleep` 해상도와 `timeBeginPeriod`의 일반 배경을 주제 문서로 남기고 Microsoft 공식 문서를 링크한다. Win9x와 NT 계열의 기본값 차이를 여기에 둔다.
* `docs/analysis/` — 3rd의 타이밍 import 표면과 판정 결과를 프로젝트 고유 사실로 남긴다. 확인됨 / 추정 / 미확정을 구분한다.
* 새 analysis·kb 파일을 추가하면 해당 디렉터리 `README.md` 색인을 같은 작업에서 갱신한다.
* 작업 로그를 남긴다.

### 범위에서 뺀 것

* 프레임률 수정과 `Sleep` 개입.
* 타이머 해상도를 우리가 명시적으로 올리는 것. 실험 1의 판정이 먼저다.
* 3rd 외 타깃. 다른 타깃도 같은 전제를 가질 수 있으나 이번에는 확인하지 않는다.
* 스케줄링·타이머 병합·`SetThreadPriority` 영향. 별도 후보로 남긴다.

## English

Prerequisite: [Task 290, guest wait accounting](20260915-290-guest-wait-accounting.md)

Status: **complete.** See the [work log](../work-logs/20260915-291-timer-resolution-dependency.md) and the [design](../design/20260915-291-timer-resolution-dependency.md).

### Background

Task 290 established that 16.8 ms of `ez2dj3rd`'s 17.8 ms frame sits inside the guest's `Sleep`, that the average request is 16.3 ms, and that the shortfall from 60 fps splits into 0.5 ms of sleep overshoot and 0.6 ms of non-sleeping time. This work order decides whether the overshoot comes from the **timer-resolution difference between Windows 9x and the NT family**.

### Facts confirmed when this order was written — confirmed (2026-09-15)

From parsing the original `.idata` of the 3rd `EZ2DJ.EXE` CHD build (TimeDateStamp `0x3bca98a3`).

| Item | Value |
| --- | --- |
| The 8 `WINMM.dll` imports | `mixerOpen`, `mixerClose`, `mixerGetNumDevs`, `mixerGetLineInfoA`, `mixerGetLineControlsA`, `mixerGetControlDetailsA`, `mixerSetControlDetails`, `timeGetTime` |
| Timing-related `KERNEL32` imports | `Sleep`, `GetThreadPriority`, `SetThreadPriority` |
| `timeBeginPeriod` | **Absent.** Zero hits across the whole file, not just `.idata`, so the protection layer cannot resolve it through `GetProcAddress` either |
| `timeBeginPeriod` or `NtSetTimerResolution` in our sources | None |

### Hypotheses

**Inferred 1 — the guest implicitly assumes a 1 ms default resolution.** The default resolution of `timeGetTime` is 1 ms on Windows 9x, while on the NT family it follows the system timer tick, 15.6 ms by default. The guest calls `timeGetTime` four times per frame to compute its sleep yet never calls `timeBeginPeriod`, which is what one would expect of code written for Win98.

**Inferred 2 — the 1 ms resolution in effect today is not ours by intent.** At the NT default of 15.6 ms a 16.3 ms request would round up to roughly 31 ms and yield about 32 fps. The measured 56 fps means the process already runs at about 1 ms. Since we never request it, statically linked SDL3 is the likely source.

If inference 2 holds, 3rd's current timing rides on an SDL side effect and would break quietly when the SDL configuration changes. That is the practical motivation for this task.

### Experiment 1 — identify who supplies the resolution

1. Run `re2dj ez2dj3rd --guest-wait-trace` on the current configuration and record the `Sleep` duration p50 and histogram as the baseline.
2. Read the actual current resolution with `NtQueryTimerResolution` and log one diagnostic line. Do not change guest behavior.
3. Repeat with a `RE2DJ_ENABLE_SDL3_AUDIO=OFF` build, and with SDL video excluded as well if such a configuration is possible.
4. Compare whether the resolution and the `Sleep` p50 move to the 15.6 ms family.

Decision: if step 3 moves p50 onto multiples of 15.6 ms, inference 2 is confirmed. If not, look for another supplier.

### Experiment 2 — determine how the guest computes its period

1. Extend task 290's histogram infrastructure to record bounded per-frame pairs of `(requested Sleep, preceding non-sleeping duration)`.
2. Examine the correlation.

Decision: an inverse correlation means a `target - elapsed` feedback loop, which would also explain how 1 ms quantization produces a 16.3 ms average. A request independent of work time means the target period may not be 16.67 ms, and the next step is to find that constant in the original code.

### Constraints

* **Observe only.** Do not change the `Sleep` argument or return value and do not raise the timer resolution. Intervening in the frame rate is a separate task to be weighed against the AGENTS.md principle of preserving original game logic.
* Gate the diagnostics behind an option as tasks 289 and 290 do; record nothing without it.
* Increment counters only on the call path.
* `NtQueryTimerResolution` is diagnostic-only and belongs under `src/platform/windows/`, not the shared core.

### Completion criteria

* The decision from experiment 1 with the run logs behind it.
* The decision from experiment 2 with its evidence.
* `docs/kb/` — a topic document on `timeGetTime` and `Sleep` resolution and `timeBeginPeriod`, linking official Microsoft documentation, carrying the Win9x versus NT default difference.
* `docs/analysis/` — 3rd's timing import surface and the decisions, as project-specific findings marked confirmed, inferred, or unresolved.
* Update the `README.md` index of any analysis or kb directory that gains a file, in the same task.
* Leave a work log.

### Out of scope

* Changing the frame rate or intervening in `Sleep`.
* Raising the timer resolution ourselves; experiment 1 decides first.
* Targets other than 3rd, which may share the assumption but are not checked here.
* Scheduling, timer coalescing, and `SetThreadPriority` effects, kept as separate candidates.
