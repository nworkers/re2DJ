# 작업 289 작업 로그 — present 간격 히스토그램과 3rd 페이싱 원인 / Task 289 work log — Present interval histogram and the 3rd pacing cause

설계: [20260915-289-present-interval-histogram.md](../design/20260915-289-present-interval-histogram.md)
작업 지시: [20260915-289-present-interval-histogram.md](../work-orders/20260915-289-present-interval-histogram.md)
선행: [작업 288](20260915-288-present-sync-policy.md)

## 한국어

### 구현

| 계층 | 변경 |
| --- | --- |
| 공용 코어 | `PresentIntervalHistogram` — 0.25 ms 버킷 160개와 초과 버킷, `Add`/`Summarize`/`Reset` |
| facade | `RecordPresentedFrame`에서 간격 누적, FPS 창이 닫힐 때 요약 한 줄, 예산 120회 |
| facade | `PresentAndRecordCost`로 세 present 호출 지점을 감싸 backend `Present` 소요 시간도 누적 |
| 테스트 | `tests/unit/present_interval_histogram_test.cpp` |

작업 지시 이후 **present 비용 계측을 추가**했다. 간격만으로는 그 시간이 게스트 코드에서 갔는지 우리 HLE에서 갔는지 가를 수 없어, 원래 목적인 원인 규명에 답할 수 없었기 때문이다. 같은 요약 줄에 `cost_*` 항목으로 붙였다.

### 검증

* Windows x86 Release 전체 빌드: 오류 0건.
* 단위 테스트: `checks: 1752, failures: 0`.
* CTest 6개 중 5개 통과. 실패한 `re2dj_windows_vfs_runtime_probe`는 작업 288에서 baseline 대조로 확인한 **기존 실패**다.
* `ez2dj3rd`를 기본값과 `--vsync off`로, `ez2d2m`을 기본값으로 실행해 요약을 수집했다. `Blt` 경로(3rd)와 `Flip` 경로(`ez2d2m`) 양쪽에서 기록됨을 확인했다.

### 측정 결과

#### `ez2dj3rd` — vsync ON과 OFF가 구분되지 않는다

| 정책 | 프레임/창 | mean | p50 | 상위 버킷 |
| --- | --- | --- | --- | --- |
| 기본(interval=1) | 57 | 17.69~17.89 | 17.50~18.00 | 17.00~18.50에 넓게 분산 |
| `--vsync off`(interval=0) | 57 | 17.57~17.91 | 17.50~17.75 | 17.25~18.50에 넓게 분산 |

두 분포가 통계적으로 구분되지 않는다. 그리고 **어느 쪽도 16.667 ms의 정수배로 양자화되지 않는다.** 0.25 ms 버킷이 16.25, 17.00, 17.25, 17.50, 17.75, 18.00, 18.25, 18.50에 고르게 퍼진다.

#### `ez2dj3rd` — present 비용은 프레임의 1.3%다

| 항목 | 값 |
| --- | --- |
| 프레임 간격 mean | 17.69~17.89 ms |
| present 비용 mean | **0.22~0.24 ms** |
| present 비용 p50 | **0.00 ms** |
| present 비용 max | 0.28~0.77 ms |

`PollEvent` 드레인, `MakeCurrent`, clear, 전체 화면 blit, `SwapWindow`를 모두 합쳐 0.22 ms다. 프레임의 나머지 약 17.5 ms는 우리 코드 **밖**, 게스트 자신의 루프에서 간다.

#### `ez2d2m` — 같은 코드가 60 fps를 낸다 (대조군)

| 항목 | 값 |
| --- | --- |
| 프레임 간격 p50 | **16.50 ms** (61개 중 34개가 이 버킷) |
| 프레임 간격 mean | 16.08~16.67 ms |
| present 비용 p50 | **16.00 ms** |

같은 빌드, 같은 호스트, 같은 vsync 정책에서 `ez2d2m`은 present 안에서 16 ms를 블록하고 16.5 ms 간격으로 좁게 몰린다. vsync가 정상 동작하는 모습 그대로다.

### 원인 — 확정

세 측정이 함께 원인을 확정한다.

1. **vsync는 3rd의 상한이 아니다.** 세 정책의 분포가 같고, 어느 쪽도 refresh 배수로 양자화되지 않는다. present 비용 p50이 0.00 ms이므로 swap은 애초에 블록하지 않는다. 게스트가 이미 refresh보다 느려서 vsync가 개입할 일이 없기 때문이다.
2. **우리 그래픽 HLE도 상한이 아니다.** present 비용은 프레임의 1.3%다. `ez2d2m` 대조군이 같은 경로로 깨끗한 60 fps를 내므로 경로 자체도 건전하다.
3. **게스트 자신의 프레임 루프가 약 17.5 ms를 쓴다.** 그 분포는 16.25~18.5에 걸쳐 넓고, 정수 밀리초에 몰리지 않는다. 따라서 `timeGetTime` 정수 목표값 가설도 성립하지 않는다.

같은 실행에서 게스트 프로세스 CPU는 한 코어의 약 16%다. 17.5 ms 대부분을 **계산이 아니라 대기**로 보낸다는 뜻이다.

### 다음 경계 — 미확정

게스트가 무엇을 기다리는지는 아직 보이지 않는다. 3rd의 원본 `.idata`에는 `Sleep`, `WaitForSingleObject`, `CreateEventA`, `timeGetTime`이 모두 있으나 이 중 어느 것도 현재 HLE가 후킹하지 않으므로 호출 시점과 인자를 관찰할 수 없다. 리듬 게임이므로 DirectSound 재생 커서에 묶여 있을 가능성도 배제되지 않았다.

다음 작업은 이 대기 원시 함수를 관찰 가능하게 만드는 것이다. 이 작업에서는 후킹하지 않았다.

### 범위에서 뺀 것

* 프레임률 수정.
* `Sleep`·`timeGetTime`·`WaitForSingleObject` 후킹.
* 기존 `FrameDraws` 요약의 변경.
* Linux·Web 호스트 확인.

## English

### Implementation

| Layer | Change |
| --- | --- |
| Shared core | `PresentIntervalHistogram` — 160 buckets of 0.25 ms plus overflow, with `Add`/`Summarize`/`Reset` |
| Facade | Accumulates the interval in `RecordPresentedFrame` and emits one summary per closed FPS window, budgeted to 120 |
| Facade | `PresentAndRecordCost` wraps all three present call sites and accumulates the backend `Present` duration |
| Tests | `tests/unit/present_interval_histogram_test.cpp` |

**Present-cost measurement was added after the work order.** An interval alone cannot separate time spent in guest code from time spent in the HLE, so it could not answer the question the instrument exists for. It appears as the `cost_*` fields on the same summary line.

### Verification

* Windows x86 Release full build: no errors.
* Unit tests: `checks: 1752, failures: 0`.
* CTest: 5 of 6 pass. The failing `re2dj_windows_vfs_runtime_probe` is the **pre-existing failure** confirmed against the baseline in task 288.
* Summaries collected from `ez2dj3rd` with the default policy and `--vsync off`, and from `ez2d2m`, confirming the record appears on both the `Blt` path (3rd) and the `Flip` path (`ez2d2m`).

### Measurements

#### `ez2dj3rd` — vsync on and off are indistinguishable

| Policy | Frames/window | mean | p50 | Top buckets |
| --- | --- | --- | --- | --- |
| Default (interval=1) | 57 | 17.69-17.89 | 17.50-18.00 | spread across 17.00-18.50 |
| `--vsync off` (interval=0) | 57 | 17.57-17.91 | 17.50-17.75 | spread across 17.25-18.50 |

The two distributions are statistically indistinguishable, and **neither quantizes to multiples of 16.667 ms**: the 0.25 ms buckets populate evenly at 16.25, 17.00, 17.25, 17.50, 17.75, 18.00, 18.25 and 18.50.

#### `ez2dj3rd` — the present costs 1.3% of a frame

| Item | Value |
| --- | --- |
| Frame interval mean | 17.69-17.89 ms |
| Present cost mean | **0.22-0.24 ms** |
| Present cost p50 | **0.00 ms** |
| Present cost max | 0.28-0.77 ms |

The `PollEvent` drain, `MakeCurrent`, clear, full-screen blit and `SwapWindow` together take 0.22 ms. The remaining ~17.5 ms of the frame is spent **outside** our code, in the guest's own loop.

#### `ez2d2m` — the same code reaches 60 fps (control)

| Item | Value |
| --- | --- |
| Frame interval p50 | **16.50 ms** (34 of 61 samples in that bucket) |
| Frame interval mean | 16.08-16.67 ms |
| Present cost p50 | **16.00 ms** |

On the same build, the same host and the same vsync policy, `ez2d2m` blocks 16 ms inside the present and clusters tightly at 16.5 ms — vsync behaving exactly as it should.

### Cause — established

The three measurements together settle it.

1. **vsync does not cap 3rd.** The three policies produce the same distribution, and none quantizes to refresh multiples. A present cost p50 of 0.00 ms means the swap never blocks in the first place, because the guest is already slower than the refresh and vsync never has occasion to act.
2. **Our graphics HLE does not cap it either.** The present is 1.3% of the frame, and the `ez2d2m` control reaches a clean 60 fps through the same path, so the path itself is healthy.
3. **The guest's own frame loop spends about 17.5 ms.** Its distribution is broad across 16.25-18.5 rather than clustered on whole milliseconds, so an integer-millisecond `timeGetTime` target does not explain it either.

In the same runs the guest process uses about 16% of one core, so most of those 17.5 ms are spent **waiting rather than computing**.

### Next boundary — unresolved

What the guest waits on is still not visible. The 3rd original `.idata` carries `Sleep`, `WaitForSingleObject`, `CreateEventA` and `timeGetTime`, but the HLE hooks none of them, so their call sites and arguments cannot be observed. Being a rhythm game, a dependency on the DirectSound play cursor is not ruled out either.

The next task is to make that wait primitive observable. Nothing was hooked in this task.

### Excluded from scope

* Changing the frame rate.
* Hooking `Sleep`, `timeGetTime` or `WaitForSingleObject`.
* Changing the existing `FrameDraws` summary.
* Linux and Web host confirmation.
