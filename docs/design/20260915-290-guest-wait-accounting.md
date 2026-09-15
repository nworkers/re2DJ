# 작업 290 설계 — 게스트 대기 계정 / Task 290 design — Guest wait accounting

선행: [작업 289 present 간격 히스토그램](20260915-289-present-interval-histogram.md)

## 한국어

### 배경

작업 289에서 `ez2dj3rd`의 프레임 상한이 vsync도 그래픽 HLE도 아님을 확정했다.

| 측정 | 값 |
| --- | --- |
| 프레임 간격 mean | 17.7~17.9 ms |
| backend `Present` 비용 mean | 0.22 ms (프레임의 1.3%) |
| 게스트 프로세스 CPU | 한 코어의 약 16% |

CPU 16%는 17.5 ms 중 약 2.8 ms만 계산이고 나머지 약 14.7 ms는 **대기**라는 뜻이다. 무엇을 기다리는지는 아직 보이지 않는다. 3rd의 원본 `.idata`에는 `Sleep`, `WaitForSingleObject`, `CreateEventA`, `timeGetTime`이 모두 있으나 HLE가 하나도 감싸지 않는다.

### 목표

프레임 창마다 게스트가 **어느 대기 원시 함수에서 얼마나** 머물렀는지 기록한다.

* `Sleep` 호출 수, 실제 소요 시간 합, 요청값 분포
* `WaitForSingleObject` 호출 수와 실제 소요 시간 합
* `timeGetTime` 호출 수

프레임 간격 17.8 ms 중 `Sleep`이 14 ms를 차지하면 원인은 게스트의 sleep 기반 리미터다. 어느 쪽도 채우지 못하면 대기는 이 세 함수 밖에 있고, 다음 후보는 DirectSound 재생 커서다.

### 설계

#### 계정기

호출 경로에서는 카운터와 히스토그램 버킷만 올린다. 문자열 포맷과 기록은 요약 시점에만 한다. 작업 289와 같은 원칙이며, 이유도 같다. `Sleep`은 프레임마다 여러 번 불릴 수 있어 호출당 I/O는 측정 대상을 바꾼다.

소요 시간 분포는 작업 289의 `PresentIntervalHistogram`을 재사용한다. 같은 성질의 값이고 버킷 폭도 맞다.

#### 래퍼

기존 VFS 래퍼와 같은 방식이다. 주입 런타임이 `__stdcall` 내보내기를 제공하고, 런처가 게스트 IAT 슬롯을 그 주소로 바꾼다.

| 게스트 import | 래퍼 |
| --- | --- |
| `KERNEL32!Sleep` | `Re2djWaitSleep` |
| `KERNEL32!WaitForSingleObject` | `Re2djWaitForSingleObject` |
| `WINMM!timeGetTime` | `Re2djWaitTimeGetTime` |

래퍼는 `QueryPerformanceCounter` 한 쌍으로 실제 소요를 재고 원래 함수를 그대로 호출한다. 동작은 바꾸지 않는다. **관찰만 한다.**

#### 보고

작업 289의 요약이 나가는 자리에서 한 줄 더 낸다. 같은 FPS 창을 쓰므로 프레임 간격과 대기 시간이 같은 구간에 대응한다. 요약을 낸 뒤 계정기를 비운다.

```
re2dj:hle:guest-wait:sleep_calls=..:sleep_ms=..:sleep_p50=..:wait_calls=..:wait_ms=..:time_calls=..
```

#### 스위치

제품 경로는 건드리지 않는다. 기존 `--audio-volume-trace`와 같은 성격의 진단이므로 같은 방식으로 제품 옵션 `--guest-wait-trace`를 두고, 그것이 런처 옵션 `--guest-wait-trace`로 전달되어 IAT 패치를 켠다. 옵션이 없으면 슬롯을 건드리지 않으므로 오버헤드가 0이다.

### 비목표

* 대기 동작 변경. 래퍼는 원래 함수를 그대로 호출한다.
* 프레임률 수정.
* DirectSound 커서 계정. 위 셋으로 설명되지 않을 때의 다음 후보이며 이 작업에는 넣지 않는다.

### 미확정

* 3rd는 보호된 빌드다. 게임 본체의 `Sleep` 호출이 정적 IAT를 지나는지, 보호 계층이 `GetProcAddress`로 따로 해석하는지는 확인되지 않았다. 호출 수가 0으로 나오면 그 자체가 답이며, 동적 해석 경로를 봐야 한다는 뜻이다.

---

## English

### Background

Task 289 established that what caps `ez2dj3rd` is neither vsync nor the graphics HLE.

| Measurement | Value |
| --- | --- |
| Frame interval mean | 17.7-17.9 ms |
| Backend `Present` cost mean | 0.22 ms (1.3% of a frame) |
| Guest process CPU | about 16% of one core |

At 16% CPU only about 2.8 ms of the 17.5 ms is computation; the remaining ~14.7 ms is **waiting**. What it waits on is still invisible: 3rd's original `.idata` carries `Sleep`, `WaitForSingleObject`, `CreateEventA` and `timeGetTime`, and the HLE wraps none of them.

### Goal

Record, per frame window, **which wait primitive the guest sits in and for how long**.

* `Sleep` call count, total measured duration, and the distribution of requested values
* `WaitForSingleObject` call count and total measured duration
* `timeGetTime` call count

If `Sleep` accounts for 14 ms of a 17.8 ms interval, the cause is the guest's sleep-based limiter. If neither accounts for it, the wait is outside these three and the next candidate is the DirectSound play cursor.

### Design

#### Accumulator

The call path only increments counters and histogram buckets; formatting and writing happen at summary time. Same principle as task 289, for the same reason: `Sleep` can be called several times per frame, so per-call I/O would change what is measured.

Duration distributions reuse task 289's `PresentIntervalHistogram` — the same kind of value at a matching bucket width.

#### Wrappers

The same mechanism as the existing VFS wrappers: the injected runtime exports a `__stdcall` entry and the launcher rewrites the guest's IAT slot to it.

| Guest import | Wrapper |
| --- | --- |
| `KERNEL32!Sleep` | `Re2djWaitSleep` |
| `KERNEL32!WaitForSingleObject` | `Re2djWaitForSingleObject` |
| `WINMM!timeGetTime` | `Re2djWaitTimeGetTime` |

Each wrapper brackets the original call with one `QueryPerformanceCounter` pair and forwards it unchanged. Behavior does not change; this **observes only**.

#### Reporting

One extra line where task 289's summary is emitted, so the same FPS window covers both the frame intervals and the waits inside them. The accumulator resets after each summary.

```
re2dj:hle:guest-wait:sleep_calls=..:sleep_ms=..:sleep_p50=..:wait_calls=..:wait_ms=..:time_calls=..
```

#### Switch

The product path stays untouched. This is the same kind of diagnostic as the existing `--audio-volume-trace`, so it follows the same route: a product `--guest-wait-trace` option forwards to a launcher `--guest-wait-trace` that enables the IAT patches. Without the option no slot is touched and the overhead is zero.

### Non-goals

* Changing wait behavior. The wrappers forward the original call.
* Changing the frame rate.
* Accounting the DirectSound cursor. That is the next candidate if these three do not explain the time, and it is not in this task.

### Unresolved

* 3rd is a protected build. Whether the game body's `Sleep` calls pass through the static IAT, or the protection resolves them separately through `GetProcAddress`, is not established. A call count of zero is itself an answer, meaning the dynamic resolution path is what must be examined.
