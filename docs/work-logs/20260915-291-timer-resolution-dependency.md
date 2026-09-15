# 작업 291 작업 로그 — 타이머 해상도 의존성 규명 / Task 291 work log — Establishing the timer-resolution dependency

설계: [20260915-291-timer-resolution-dependency.md](../design/20260915-291-timer-resolution-dependency.md)
작업 지시: [20260915-291-timer-resolution-dependency.md](../work-orders/20260915-291-timer-resolution-dependency.md)
선행: [작업 290](20260915-290-guest-wait-accounting.md)

## 한국어

### 구현

| 계층 | 변경 |
| --- | --- |
| Windows | `timer_resolution_probe.h/.cpp` 신규 — `ntdll`의 `NtQueryTimerResolution` 동적 해석, 변경 시에만 기록, 스위치 무장 전 표본은 보류 |
| Windows | `guest_wait_accounting.h/.cpp` — `(요청값, 직전 non-sleeping 구간)` 쌍의 누적합 다섯 개와 파생 통계 `GuestSleepModelSummary` |
| 주입 런타임 | `Re2djWaitSleep`이 직전 `Sleep` 종료 시각을 유지해 non-sleeping 구간을 함께 측정 |
| 주입 런타임 | `DllMain` attach에서 해상도 표본 1회 |
| facade | `pre-audio`·`post-audio`·`pre-backend`·`post-backend`·`frame-window` 다섯 지점 표본, `guest-sleep-model` 한 줄 |

새 기록은 모두 기존 `--guest-wait-trace` 스위치를 공유한다. 같은 질문에 대한 진단이므로 스위치를 새로 만들지 않았다.

### 실험 1 — 판정: 공급자는 정적 링크된 SDL3다 — 확인됨

**지시서의 방법으로는 답이 나오지 않았고, 그 사실 자체가 첫 번째 발견이다.**

`NtQueryTimerResolution`은 세 실행 모두에서 `current_ms=1.0000`을 돌려주었다. 프레임률이 절반으로 떨어진 실행에서도 값이 같았다. 이 API는 **시스템 전역** 값을 읽는데, Windows 10 2004부터 `timeBeginPeriod`는 호출한 프로세스에만 적용되기 때문이다. 이 호스트에서는 무관한 다른 프로세스가 전역 값을 1 ms로 유지하고 있었다.

대조 측정으로 확인했다. 아무것도 요청하지 않은 프로세스는 전역 값이 1.0 ms로 읽히는 동안에도 `Sleep(15)` p50이 29.96 ms였고, `PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION`을 적용해도 29.99 ms로 같았다.

그래서 **정적 증거와 제거 실험**으로 답을 냈다.

1. 우리 주입 런타임 DLL의 import table에 `timeBeginPeriod`와 `timeEndPeriod`가 있다. 우리 소스에는 호출이 없고 3rd 원본에도 없다.
2. SDL3 소스에서 경로를 확인했다. `SDL_InitSubSystem` → `SDL_InitMainThread` → `SDL_InitTicks`가 `SDL_HINT_TIMER_RESOLUTION` 콜백을 등록하고, 힌트가 비어 있으면 기본 period 1로 `timeBeginPeriod(1)`을 부른다. 서브시스템 종류와 무관하다.
3. 힌트 `SDL_TIMER_RESOLUTION=0`으로 그 요청만 제거하고 같은 실행을 반복했다.

| 측정 | 기본 | `SDL_TIMER_RESOLUTION=0` |
| --- | --- | --- |
| `Sleep` 소요 p50 | 16.00~16.25 ms | **27.50~27.75 ms** |
| 프레임 간격 mean | 17.56~17.59 ms | **30.29~30.63 ms** |
| 프레임률 | 약 56.9 fps | **약 32.7 fps** |

지시서가 예측한 "약 31 ms로 올림되어 32 fps"와 일치한다. **추정 2는 확인되었다. 3rd의 현재 프레임률은 SDL3의 기본 힌트에 얹혀 있다.**

지시서의 `RE2DJ_ENABLE_SDL3_AUDIO=OFF` 빌드는 수행하지 않았다. 힌트 제거 실험이 같은 질문에 더 좁게 답하고, 2번의 소스 경로가 audio·video 어느 서브시스템이든 같은 `SDL_InitTicks`를 지난다는 것을 보이므로 audio만 뺀 빌드는 새 정보를 주지 않는다.

### 실험 2 — 판정: 목표 주기를 유지하는 피드백 루프다 — 확인됨

프레임마다 `(Sleep 요청값 x, 직전 non-sleeping 구간 y)`를 누적합으로 모았다.

| 창 | `mean(x)` | `sd(x)` | `mean(y)` | `sd(y)` | `mean(x+y)` | `sd(x+y)` | `corr` |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 14.35 | 3.17 | 2.77 | 3.08 | 17.13 | 0.62 | -0.981 |
| 2 | 15.19 | 2.11 | 1.81 | 1.97 | 17.00 | 0.63 | -0.955 |
| 3 | 15.37 | 1.05 | 1.59 | 0.95 | 16.96 | 0.53 | -0.864 |
| 4 | 15.43 | 0.87 | 1.51 | 0.84 | 16.94 | 0.57 | -0.780 |

`corr`은 -0.78~-0.98, `sd(x)`와 `sd(y)`는 0.8~3.2 ms로 움직이는데 `sd(x+y)`만 0.53~0.68 ms로 일정하다. 설계의 판정표에서 **`목표 - 경과` 피드백**에 해당한다. 요청값은 고정 상수가 아니다.

목표 주기는 `mean(x+y)` = **16.94~17.35 ms**이며, 실험 1에서 호스트 해상도를 15.6 ms로 바꿔도 17.09~17.35 ms로 움직이지 않았다. 게스트 내부 상수라는 뜻이다.

약 17 ms는 58.8 Hz이고 60 Hz의 16.67 ms가 아니다. 다만 게스트의 경과 측정이 `timeGetTime`의 1 ms 양자화를 거치고 측정 구간 경계가 우리와 정확히 같지 않으므로 이는 추정치다. **원본 코드의 상수를 직접 읽지 않았으므로 16 ms인지 17 ms인지는 미확정으로 남긴다.** 작업 290이 "원본 의도가 정말 60 fps인지"를 미확정으로 남긴 부분은, 적어도 **60 Hz를 그대로 가정할 근거는 약해졌다**는 쪽으로 좁혀졌다.

### 프레임 예산의 갱신

게스트가 지키는 목표 주기는 17.0 ms인데 실제 프레임은 17.6 ms다. 차이 약 0.6 ms는 `Sleep` 초과분이며, 1 ms 해상도에서도 요청값이 틱 경계로 올림되기 때문이다. 작업 290이 "0.5 ms 초과 + 0.6 ms 비-수면 초과"로 나눠 둔 두 몫은, **목표 주기가 16.67 ms가 아니라 약 17 ms**라는 사실로 다시 읽어야 한다. 비-수면 시간이 게스트의 예상보다 길다는 두 번째 몫은 근거가 사라진다.

### 검증

* Windows x86 Debug 전체 빌드와 Release 전체 빌드: 오류 0건.
* 단위 테스트: `checks: 1752, failures: 0`.
* CTest 6개 중 5개 통과. 남은 `re2dj_windows_vfs_runtime_probe`는 **기존 실패**다. 이번에는 Debug에서 2초 실패가 아니라 멈춤으로 나타나 원인을 확인했다. 변경 전체를 stash 하고 clean tree에서 같은 Debug 구성으로 다시 빌드해 실행했을 때도 동일하게 멈췄으므로 이번 변경과 무관하다. 작업 288·290에서 기록한 Release 실행의 2초 실패와 같은 테스트다.
* 옵션 없는 실행에서 `timer-resolution` 0줄, `guest-sleep-model` 0줄, `guest-wait` 0줄, `present-interval` 11줄. 스위치가 실제로 게이트 역할을 한다.
* 실험 1의 대조 측정은 저장소 밖 임시 스크립트로 수행했고 저장소에 남기지 않았다. 재현 절차는 kb 문서의 표에 값과 함께 적어 두었다.

### 문서

* `docs/kb/windows-timer-resolution.md` 신규 — `Sleep`·`timeGetTime` 해상도, Win9x와 NT 계열 기본값 차이, Windows 10 2004의 프로세스 단위 규칙, SDL3의 기본 요청. Microsoft 공식 문서 링크 포함.
* `docs/analysis/ez2dj3rd-frame-pacing.md` 신규 — 3rd의 타이밍 import 표면, 피드백 루프 판정, SDL3 의존 판정, 갱신된 프레임 예산. 확인됨/추정/미확정 구분.
* 두 디렉터리의 `README.md` 색인 갱신.

### 다음 경계

프레임률 개입은 여전히 별도 판단이 필요하지만, 이번 작업으로 선택지의 성격이 바뀌었다.

* **의존을 명시화하는 것**과 **프레임률을 바꾸는 것**은 다른 일이다. 전자는 우리가 `SDL_TIMER_RESOLUTION` 힌트나 `timeBeginPeriod`를 직접 요청해 현재 동작을 SDL 기본값에서 우리 정책으로 옮기는 것이고, 게스트가 보는 시간 진행은 지금과 같다. 원본 게임 로직을 건드리지 않는다.
* 후자는 `Sleep` 초과분만큼 덜 자게 만드는 일이며 게스트의 시간 진행을 바꾼다. AGENTS.md의 원본 게임 로직 보존 원칙과 대조해 따로 판단해야 한다.

첫 번째는 이번 판정의 직접적인 귀결이므로 별도 작업으로 제안한다.

### 후속 확인 (2026-09-16) — `timeGetTime`은 함께 뭉개지지 않는다 — 확인됨

실험 1의 결론을 읽던 중 나온 질문이다. `timeBeginPeriod`가 `timeGetTime`과 `Sleep`의 해상도를 함께 바꾼다면, 힌트 제거 실행에서 게스트의 경과 측정도 15.6 ms로 뭉개졌어야 한다.

해상도를 요청하지 않은 대조 프로세스에서 두 값을 같은 프로세스 안에서 쟀다. `timeGetTime` 관측 step은 **1 ms**(200 표본 중 197)인데 같은 프로세스의 `Sleep(15)` p50은 **25.87 ms**였다. 시계와 수면이 분리되어 있다.

게스트 데이터도 같은 결론이다. 힌트 제거 실행의 `sd(x)`는 1.17~1.77 ms, `sd(x+y)`는 0.68 ms였다. 게스트 시계가 15.6 ms 단위였다면 경과값이 0 또는 16으로만 읽혀 두 값 모두 6 ms 근처가 되어야 하는데, 관측값은 그 1/5 이하다.

따라서 **SDL3의 요청을 잃으면 프레임률은 절반이 되지만 게스트의 내부 시간 진행은 유지된다.** 실험 1이 드러낸 위험은 생각보다 좁다. 반면 이 호스트의 `timeGetTime` 고해상도 자체가 무관한 외부 프로세스의 전역 틱 요청에 얹혀 있고, 전역 틱을 낮출 수단이 없어 그 조건에서의 동작은 확인하지 못했다. 분석 문서 8절에 미확정으로 남겼다.

이 확인으로 60 Hz 주제의 조사는 종료한다. 남은 항목은 분석 문서 8절의 미확정 목록과 아래 "다음 경계"이며, 착수 여부는 별도 판단이다.

### 범위에서 뺀 것

* `Sleep` 인자·반환값 개입과 프레임률 수정.
* 우리가 타이머 해상도를 명시적으로 요청하는 것. 판정이 나왔으므로 이제 별도 작업의 대상이다.
* 3rd 외 타깃.
* `SetThreadPriority`와 스케줄링 영향.

## English

Design: [20260915-291-timer-resolution-dependency.md](../design/20260915-291-timer-resolution-dependency.md)
Work order: [20260915-291-timer-resolution-dependency.md](../work-orders/20260915-291-timer-resolution-dependency.md)
Prerequisite: [Task 290](20260915-290-guest-wait-accounting.md)

### Implementation

| Layer | Change |
| --- | --- |
| Windows | New `timer_resolution_probe.h/.cpp` — dynamic `ntdll` resolution of `NtQueryTimerResolution`, recording on change only, holding samples taken before the switch is armed |
| Windows | `guest_wait_accounting.h/.cpp` — five running sums over (request, preceding non-sleeping segment) pairs and the derived `GuestSleepModelSummary` |
| Injected runtime | `Re2djWaitSleep` keeps the previous sleep's return time and measures the non-sleeping segment alongside |
| Injected runtime | One resolution sample at `DllMain` attach |
| Facade | Samples at `pre-audio`, `post-audio`, `pre-backend`, `post-backend` and `frame-window`, plus one `guest-sleep-model` line |

Every new record shares the existing `--guest-wait-trace` switch: it is a diagnostic for the same question, so no second switch was introduced.

### Experiment 1 — decision: the supplier is statically linked SDL3 — confirmed

**The work order's method could not answer this, and that is the first finding.**

`NtQueryTimerResolution` returned `current_ms=1.0000` in all three runs, including the one whose frame rate halved. The API reads the **system-wide** value, and since Windows 10 2004 `timeBeginPeriod` applies only to the calling process. On this host an unrelated process was holding the global value at 1 ms.

A control measurement confirmed it: a process that requested nothing measured a `Sleep(15)` p50 of 29.96 ms while the global value still read 1.0 ms, and applying `PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION` gave the same 29.99 ms.

The answer came from **static evidence plus an ablation** instead.

1. Our injected runtime DLL's import table contains `timeBeginPeriod` and `timeEndPeriod`. Neither our sources nor the original 3rd binary call them.
2. The path is visible in SDL3's sources: `SDL_InitSubSystem` → `SDL_InitMainThread` → `SDL_InitTicks` registers the `SDL_HINT_TIMER_RESOLUTION` callback and, with the hint unset, calls `timeBeginPeriod(1)` at a default period of 1, regardless of which subsystem was requested.
3. The same run was repeated with only that request removed, through `SDL_TIMER_RESOLUTION=0`.

| Measurement | Default | `SDL_TIMER_RESOLUTION=0` |
| --- | --- | --- |
| `Sleep` duration p50 | 16.00-16.25 ms | **27.50-27.75 ms** |
| Frame interval mean | 17.56-17.59 ms | **30.29-30.63 ms** |
| Frame rate | About 56.9 fps | **About 32.7 fps** |

This matches the work order's prediction of "roughly 31 ms and about 32 fps". **Inference 2 is confirmed: 3rd's current frame rate rides on SDL3's default hint.**

The work order's `RE2DJ_ENABLE_SDL3_AUDIO=OFF` build was not run. Removing the hint answers the same question more narrowly, and the source path in step 2 shows that audio and video both reach the same `SDL_InitTicks`, so an audio-only ablation would add nothing.

### Experiment 2 — decision: a feedback loop holding a target period — confirmed

Per-frame pairs of (requested sleep `x`, preceding non-sleeping segment `y`) were accumulated as running sums.

| Window | `mean(x)` | `sd(x)` | `mean(y)` | `sd(y)` | `mean(x+y)` | `sd(x+y)` | `corr` |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 14.35 | 3.17 | 2.77 | 3.08 | 17.13 | 0.62 | -0.981 |
| 2 | 15.19 | 2.11 | 1.81 | 1.97 | 17.00 | 0.63 | -0.955 |
| 3 | 15.37 | 1.05 | 1.59 | 0.95 | 16.96 | 0.53 | -0.864 |
| 4 | 15.43 | 0.87 | 1.51 | 0.84 | 16.94 | 0.57 | -0.780 |

`corr` runs -0.78 to -0.98, `sd(x)` and `sd(y)` move over 0.8-3.2 ms, and only `sd(x+y)` stays flat at 0.53-0.68 ms. In the design's decision table that is **`target - elapsed` feedback**: the request is not a fixed constant.

The target period is `mean(x+y)` = **16.94-17.35 ms**, and it did not move when experiment 1 changed the host resolution to 15.6 ms, staying at 17.09-17.35 ms. It is a guest-internal constant.

About 17 ms is 58.8 Hz, not the 16.67 ms of 60 Hz. The guest measures its elapsed time through `timeGetTime`'s 1 ms quantization and its measurement boundaries are not exactly ours, so this is an estimate. **Whether the constant is 16 or 17 is left unresolved,** since it has not been read out of the original code. What task 290 left open — whether the original intent really is 60 fps — narrows at least to this: **the evidence for simply assuming 60 Hz is now weaker.**

### The frame budget, revised

The guest holds a 17.0 ms target while the actual frame is 17.6 ms. The roughly 0.6 ms difference is the sleep overshoot, because even at 1 ms resolution a request rounds up to a tick boundary. Task 290 split the shortfall into "0.5 ms of sleep overshoot plus 0.6 ms of non-sleeping overshoot"; with the target at about 17 ms rather than 16.67 ms, the second term loses its basis.

### Verification

* Windows x86 Debug and Release full builds: no errors.
* Unit tests: `checks: 1752, failures: 0`.
* CTest: 5 of 6 pass. The remaining `re2dj_windows_vfs_runtime_probe` is the **pre-existing failure**. It appeared this time as a hang rather than the 2-second failure seen before, so it was checked: stashing the whole change and rebuilding the same Debug configuration on a clean tree hung identically, so it is unrelated to this work. It is the same test recorded as failing in tasks 288 and 290 under Release.
* A run without the option recorded zero `timer-resolution`, zero `guest-sleep-model` and zero `guest-wait` lines against 11 `present-interval` lines, so the switch does gate them.
* Experiment 1's control measurement used a throwaway script outside the repository and was not committed. Its values and procedure are written into the kb document's table.

### Documentation

* New `docs/kb/windows-timer-resolution.md` — `Sleep` and `timeGetTime` resolution, the Win9x versus NT defaults, the Windows 10 2004 per-process rule, and SDL3's default request, with links to official Microsoft documentation.
* New `docs/analysis/ez2dj3rd-frame-pacing.md` — 3rd's timing import surface, the feedback-loop decision, the SDL3 dependency decision, and the revised frame budget, marked confirmed, inferred, or unresolved.
* Index updates in both directories' `README.md`.

### Next boundary

Intervening in the frame rate still needs its own judgment, but this task changed the shape of the options.

* **Making the dependency explicit** and **changing the frame rate** are different things. The first means requesting the resolution ourselves, through the `SDL_TIMER_RESOLUTION` hint or `timeBeginPeriod`, moving today's behavior from an SDL default to our own policy. Time advances for the guest exactly as it does now, and original game logic is untouched.
* The second means sleeping short by the overshoot, which changes how time advances for the guest and has to be weighed against the AGENTS.md principle of preserving original game logic.

The first follows directly from this decision and is proposed as its own task.

### Follow-up (2026-09-16) — `timeGetTime` does not degrade with it — confirmed

A question raised while reading experiment 1's conclusion: if `timeBeginPeriod` changes the resolution of `timeGetTime` and `Sleep` together, the guest's elapsed measurement should have coarsened to 15.6 ms in the hint-removed run too.

Both values were measured inside one control process that requested no resolution. `timeGetTime` stepped at **1 ms** (197 of 200 samples) while the same process's `Sleep(15)` p50 was **25.87 ms**. The clock and the sleep are decoupled.

The guest data agrees. In the hint-removed run `sd(x)` was 1.17-1.77 ms and `sd(x+y)` 0.68 ms, where a 15.6 ms guest clock would read elapsed as only 0 or 16 and push both near 6 ms — five times what was observed.

So **losing SDL3's request halves the frame rate but leaves the guest's internal advance of time intact.** The risk experiment 1 exposed is narrower than it first appeared. Conversely, this host's fine `timeGetTime` itself rides on an unrelated process's global-tick request, and with no way to lower the global tick its behavior under that condition was not verified; it is recorded as unresolved in section 8 of the analysis.

This closes the 60 Hz investigation. What remains is the unresolved list in the analysis document's section 8 and the next boundary below, and whether to take either up is a separate decision.

### Excluded from scope

* Intervening in the `Sleep` argument or return value, and changing the frame rate.
* Requesting the timer resolution ourselves. Now that the decision is in, it belongs to a separate task.
* Targets other than 3rd.
* `SetThreadPriority` and scheduling effects.
