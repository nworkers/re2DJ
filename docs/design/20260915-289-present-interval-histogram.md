# 작업 289 설계 — present 간격 히스토그램 / Task 289 design — Present interval histogram

선행: [작업 288 present 동기화 정책](20260915-288-present-sync-policy.md)

## 한국어

### 배경

작업 288의 실행 검증에서 `ez2dj3rd`가 vsync를 꺼도(`applied_interval=0`) 56~57 fps 그대로임을 확인했다. present가 전혀 블록하지 않는데 속도가 같으므로 상한은 present 경계에 없고, 게스트가 약 17.6 ms 주기로 스스로 페이싱한다.

다음 질문은 **그 17.6 ms가 어떻게 만들어지는가**이다. 지금은 답할 계측이 없다.

* 프레임별 시간을 남기는 `FrameDraws` 요약은 `SurfaceFlip` 안에만 있다. 3rd는 `Flip`을 한 번도 부르지 않고 `Blt`로 present하므로 3rd 실행에는 프레임 시간이 한 줄도 남지 않는다.
* 타이틀바 FPS는 1초 평균이라 분포를 볼 수 없다.
* launcher의 `--api-trace`는 소프트웨어 브레이크포인트 기반이라 측정 대상인 타이밍을 교란한다.

### 목표

present 사이 간격의 **분포**를 프로세스 안에서, 프레임마다 I/O 없이 기록한다.

분포 모양이 원인을 가른다.

* 17 ms와 18 ms에 좁게 몰린 이봉 → 정수 밀리초 타이머 목표. `timeGetTime`은 1 ms 해상도이므로 목표가 17 ms면 관측 간격이 위상에 따라 17 또는 18로 갈린다.
* 16.67 ms의 정수배에 몰림 → 여전히 어떤 vsync 경계에 묶여 있다는 뜻.
* 넓게 퍼짐 → 타이머가 아니라 이벤트나 오디오 커서 같은 다른 신호를 기다린다.

### 설계

#### 계측 지점

`RecordPresentedFrame`은 present 세 경로(`Blt` primary 대상, `BltFast`, `Flip`)가 **모두** 지나는 단일 지점이다. 여기에 넣으면 present 방식과 무관하게 모든 제품에서 기록된다. `FrameDraws`를 `Blt` 경로에 복제하지 않는 이유이기도 하다.

#### 누적 방식

프레임마다 파일을 쓰지 않는다. 그러면 계측이 측정 대상을 바꾼다. 대신 고정 크기 버킷 배열에 카운트만 올리고, 기존 FPS 창(약 1초)이 닫힐 때 요약 한 줄을 남긴다.

* 버킷 폭 `0.25 ms`, 범위 `0`~`40 ms`, 160개와 초과 버킷 하나.
* 요약에는 프레임 수, 평균, 최소, p50, p95, 최대, 그리고 **카운트 상위 버킷 다섯 개**를 담는다. 상위 버킷이 분포 모양을 직접 보여준다.
* 요약 뒤 히스토그램을 비워 창마다 독립적으로 읽히게 한다.

#### 기록 예산

`FrameDraws`와 같은 방식으로 제한한다. 기본 120회(약 2분)까지 남기고, complete diagnostics가 켜져 있으면 제한하지 않는다. 초당 한 줄이므로 제품 실행의 로그 증가는 무시할 수 있다.

### 비목표

* 게스트 코드나 원본 실행 파일 수정.
* `Sleep`이나 `timeGetTime` 후킹. 이 작업은 관찰만 한다.
* 프레임률 수정.

### 검증

* 단위 테스트: 히스토그램 누적과 백분위 선택을 공용 코어에서 검증한다.
* `ez2dj3rd`를 `--vsync off`와 기본값으로 실행해 요약 줄을 수집한다.
* `ez2d2m`(`Flip`로 present)도 실행해 present 방식과 무관하게 기록되는지 확인한다.

---

## English

### Background

Task 288's runtime check confirmed that `ez2dj3rd` stays at 56-57 fps even with vsync off (`applied_interval=0`). Since the rate does not move when presents never block, the cap is not at the present boundary: the guest paces itself at roughly 17.6 ms.

The next question is **how that 17.6 ms is produced**, and there is no instrument that answers it.

* The `FrameDraws` summary that records per-frame time lives only inside `SurfaceFlip`. 3rd never calls `Flip` — it presents with `Blt` — so a 3rd run records no frame time at all.
* The title-bar FPS is a one-second average and hides the distribution.
* The launcher's `--api-trace` is software-breakpoint based and perturbs the very timing being measured.

### Goal

Record the **distribution** of intervals between presents, in-process, with no per-frame I/O.

The shape of that distribution separates the causes.

* A tight bimodal split at 17 ms and 18 ms means an integer-millisecond timer target: `timeGetTime` has 1 ms resolution, so a 17 ms target is observed as 17 or 18 depending on phase.
* Clustering on multiples of 16.67 ms would mean something is still bound to a refresh boundary.
* A broad spread means the guest waits on something other than a timer, such as an event or an audio cursor.

### Design

#### Where to measure

`RecordPresentedFrame` is the single point that **all three** present paths pass through — `Blt` to a primary target, `BltFast`, and `Flip`. Instrumenting there records for every product regardless of how it presents, which is also why `FrameDraws` is not duplicated into the `Blt` path.

#### How to accumulate

Nothing is written per frame; that would let the instrument change what it measures. Each present only increments a bucket, and one summary line is emitted when the existing FPS window (about one second) closes.

* Buckets are `0.25 ms` wide over `0`-`40 ms`: 160 of them plus one overflow bucket.
* The summary carries the frame count, mean, minimum, p50, p95, maximum, and the **five highest-count buckets**, which show the distribution's shape directly.
* The histogram resets after each summary so every window reads independently.

#### Record budget

Bounded the same way as `FrameDraws`: 120 summaries (about two minutes) by default, unbounded when complete diagnostics are on. At one line per second the added volume on a product run is negligible.

### Non-goals

* Modifying guest code or the original executables.
* Hooking `Sleep` or `timeGetTime`. This task only observes.
* Changing the frame rate.

### Verification

* Unit tests for histogram accumulation and percentile selection in the shared core.
* Run `ez2dj3rd` with `--vsync off` and with the default, collecting summary lines.
* Run `ez2d2m`, which presents through `Flip`, to confirm the record appears regardless of present method.
