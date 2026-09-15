# 작업 290 작업 로그 — 게스트 대기 계정과 3rd 프레임 예산 / Task 290 work log — Guest wait accounting and the 3rd frame budget

설계: [20260915-290-guest-wait-accounting.md](../design/20260915-290-guest-wait-accounting.md)
작업 지시: [20260915-290-guest-wait-accounting.md](../work-orders/20260915-290-guest-wait-accounting.md)
선행: [작업 289](20260915-289-present-interval-histogram.md)

## 한국어

### 구현

| 계층 | 변경 |
| --- | --- |
| Windows | `guest_wait_accounting.h/.cpp` — SRW lock 아래 카운터와 소요 히스토그램, `TakeGuestWaitSummary` |
| 주입 런타임 | `Re2djWaitSleep`, `Re2djWaitForSingleObject`, `Re2djWaitTimeGetTime` 래퍼 |
| 주입 런타임 | `g_re2dj_guest_wait_trace`와 동적 resolver의 세 이름 추가 |
| facade | 작업 289 요약 옆에 `guest-wait` 한 줄 |
| launcher | `--guest-wait-trace` 파싱, IAT 패치, 전역 기록, 패치 결과 진단 |
| profile·인자·제품 CLI | `guest_wait_trace` 전달과 `--guest-wait-trace` |

### 정적 IAT 패치만으로는 닿지 않았다 — 확인됨

첫 실행에서 세 슬롯 모두 `present=true, prepared=true`로 패치되었는데 **대기 요약이 한 줄도 나오지 않았다.** 즉 게스트의 호출이 그 슬롯을 지나지 않는다. 3rd는 보호된 빌드이고 프로파일이 이미 `hle_dynamic_vfs`를 켜는 이유와 같은 현상이다.

설계의 "미확정" 항목이 여기서 답을 얻었다. 동적 resolver에 세 이름을 추가하자 즉시 계정이 잡혔다. 따라서 **3rd의 이 호출들은 `GetProcAddress`로 해석된다.**

### 측정 결과 — `ez2dj3rd` 타이틀 화면

약 1초 창(56~57 프레임) 기준이다.

| 항목 | 값 | 프레임당 |
| --- | --- | --- |
| `Sleep` 호출 수 | 56~57 | **정확히 1회** |
| `Sleep` 실제 소요 합 | 941~961 ms | **16.8 ms** |
| `Sleep` 요청값 합 | 910~931 ms | **16.3 ms** |
| `Sleep` 소요 p50 | 16.50~17.00 ms | |
| `WaitForSingleObject` 호출 수 | 112~114 | 2회 |
| `WaitForSingleObject` 소요 합 | 0.11~0.14 ms | **무시 가능** |
| `timeGetTime` 호출 수 | 225~229 | 4회 |
| 프레임 간격 mean | 17.77~17.95 ms | |
| backend `Present` 비용 mean | 0.26~0.28 ms | |

### 프레임 예산 — 확정

17.8 ms가 어디로 가는지 전부 설명된다.

| 구간 | 프레임당 | 비중 |
| --- | --- | --- |
| 게스트 `Sleep` | 16.8 ms | **94%** |
| backend `Present` | 0.26 ms | 1.5% |
| 나머지(게스트 계산 + 우리 Blt 작업) | 약 0.7 ms | 4% |
| 합계 | 약 17.8 ms | 56.2 fps |

게스트의 프레임 루프는 **작업 → `Sleep` → present**이고, 프레임 시간의 94%가 그 `Sleep` 안이다. `WaitForSingleObject`는 프레임당 두 번 불리지만 합쳐 0.12 ms로 즉시 반환한다. 대기 대상은 이벤트도 오디오 커서도 아닌 **`Sleep`** 이다.

### 60 fps에 못 미치는 이유

두 몫이 겹친다.

1. **`Sleep` 초과.** 게스트는 평균 16.3 ms를 요청하는데 실제로는 16.8 ms 잔다. 프레임당 약 **0.5 ms 초과**다. Windows `Sleep`은 요청값을 타이머 틱 위로 올림하므로, 1 ms 해상도에서도 요청값보다 길게 잔다.
2. **잠들지 않은 시간이 게스트의 예상보다 길다.** 요청 16.3 ms에 present 0.26 ms와 나머지 0.7 ms를 더하면 약 17.3 ms다. 게스트가 16.67 ms를 의도했다면 잠들지 않은 시간을 약 0.37 ms로 보고 있다는 뜻인데 실제로는 약 0.96 ms다.

요청값이 16.3 ms라는 것은 게스트의 목표 주기가 60 Hz 근처라는 **추정** 근거다. 다만 게스트가 `timeGetTime`을 프레임당 4번 부르고 그 해상도가 1 ms이므로, 자기 경과 시간 측정 자체가 양자화되어 있다. 원본 캐비닛에서 이 루프가 실제로 몇 fps였는지는 **미확정**이며, 이 측정으로 단정하지 않는다.

### 검증

* Windows x86 Release 전체 빌드: 오류 0건.
* 단위 테스트: `checks: 1752, failures: 0`.
* CTest 6개 중 5개 통과. 실패한 `re2dj_windows_vfs_runtime_probe`는 작업 288에서 baseline 대조로 확인한 **기존 실패**다.
* 옵션 없는 직전 세 실행의 `.ddraw.log`에서 `guest-wait` 줄이 0개이고 `present-interval`은 99~120개임을 확인했다. 스위치가 실제로 게이트 역할을 한다.
* 옵션을 준 실행에서 세 래퍼의 패치 결과가 진단 로그에 남는다.

### 다음 경계

프레임률을 60으로 올리려면 게스트의 `Sleep` 주기에 개입해야 한다. 이는 원본 타이밍에 손대는 일이므로 별도 설계가 필요하다. 최소한 두 가지를 먼저 정해야 한다.

* 원본 의도가 정말 60 fps인지. 요청값 16.3 ms는 근거이지 확정이 아니다.
* 개입 방식. `Sleep` 래퍼가 초과분만큼 덜 자게 만드는 방법은 이미 래퍼가 있으므로 작지만, 원본 게임 로직의 시간 진행을 바꾸는 행위다. AGENTS.md의 "원본 게임 로직 보존" 원칙과 대조해 판단해야 한다.

이 작업은 관찰만 했고 대기 동작을 바꾸지 않았다.

### 범위에서 뺀 것

* 프레임률 수정.
* DirectSound 커서 계정. `Sleep`이 94%를 설명하므로 필요하지 않았다.
* `CreateEventA`·`SetEvent` 계정.

## English

### Implementation

| Layer | Change |
| --- | --- |
| Windows | `guest_wait_accounting.h/.cpp` — counters and duration histograms under an SRW lock, plus `TakeGuestWaitSummary` |
| Injected runtime | The `Re2djWaitSleep`, `Re2djWaitForSingleObject` and `Re2djWaitTimeGetTime` wrappers |
| Injected runtime | `g_re2dj_guest_wait_trace` and the three names in the dynamic resolver |
| Facade | One `guest-wait` line beside task 289's summary |
| Launcher | `--guest-wait-trace` parsing, the IAT patches, the global, and a patch-result diagnostic |
| Profile, arguments, product CLI | `guest_wait_trace` forwarding and `--guest-wait-trace` |

### The static IAT patch alone did not reach the calls — confirmed

On the first run all three slots reported `present=true, prepared=true`, yet **not one wait summary appeared**: the guest's calls do not pass through those slots. 3rd is a protected build, and this is the same phenomenon that already makes its profile enable `hle_dynamic_vfs`.

That answered the design's open question. Adding the three names to the dynamic resolver produced accounting immediately, so **3rd resolves these calls through `GetProcAddress`.**

### Measurements — `ez2dj3rd` title screen

Per one-second window of 56-57 frames.

| Item | Value | Per frame |
| --- | --- | --- |
| `Sleep` calls | 56-57 | **exactly one** |
| `Sleep` measured total | 941-961 ms | **16.8 ms** |
| `Sleep` requested total | 910-931 ms | **16.3 ms** |
| `Sleep` duration p50 | 16.50-17.00 ms | |
| `WaitForSingleObject` calls | 112-114 | 2 |
| `WaitForSingleObject` total | 0.11-0.14 ms | **negligible** |
| `timeGetTime` calls | 225-229 | 4 |
| Frame interval mean | 17.77-17.95 ms | |
| Backend `Present` cost mean | 0.26-0.28 ms | |

### The frame budget — established

All 17.8 ms is now accounted for.

| Segment | Per frame | Share |
| --- | --- | --- |
| Guest `Sleep` | 16.8 ms | **94%** |
| Backend `Present` | 0.26 ms | 1.5% |
| Remainder (guest computation plus our Blt work) | about 0.7 ms | 4% |
| Total | about 17.8 ms | 56.2 fps |

The guest's frame loop is **work, `Sleep`, present**, and 94% of frame time is inside that sleep. `WaitForSingleObject` is called twice per frame but returns immediately, totalling 0.12 ms. What the guest waits on is neither an event nor an audio cursor: it is **`Sleep`**.

### Why it falls short of 60 fps

Two contributions overlap.

1. **`Sleep` overshoot.** The guest asks for 16.3 ms on average and sleeps 16.8 ms, about **0.5 ms over per frame**. Windows rounds a sleep up to a timer tick, so it runs long even at 1 ms resolution.
2. **The non-sleeping time is longer than the guest assumes.** The 16.3 ms request plus 0.26 ms of present and 0.7 ms of remainder is about 17.3 ms. For the guest to have intended 16.67 ms it must believe its non-sleeping time is about 0.37 ms, while it is about 0.96 ms.

A 16.3 ms request is **inferred** evidence that the guest's target period is near 60 Hz. It calls `timeGetTime` four times per frame at 1 ms resolution, though, so its own measurement of elapsed time is itself quantized. What frame rate this loop actually ran at on the original cabinet is **unresolved**, and these measurements are not treated as settling it.

### Verification

* Windows x86 Release full build: no errors.
* Unit tests: `checks: 1752, failures: 0`.
* CTest: 5 of 6 pass. The failing `re2dj_windows_vfs_runtime_probe` is the **pre-existing failure** confirmed against the baseline in task 288.
* The three most recent runs without the option recorded zero `guest-wait` lines against 99-120 `present-interval` lines, so the switch does gate it.
* A run with the option records each wrapper's patch result in the diagnostic log.

### Next boundary

Raising the frame rate to 60 means intervening in the guest's sleep period, which touches original timing and needs its own design. At least two things must be settled first.

* Whether the original intent really is 60 fps. The 16.3 ms request is evidence, not proof.
* How to intervene. Making the `Sleep` wrapper sleep short by the overshoot is small now that the wrapper exists, but it changes how time advances for the original game logic and has to be weighed against the AGENTS.md principle of preserving original game logic.

This task only observed; it changed no wait behavior.

### Excluded from scope

* Changing the frame rate.
* Accounting the DirectSound cursor, which was unnecessary once `Sleep` explained 94%.
* Accounting `CreateEventA` and `SetEvent`.
