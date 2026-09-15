# 작업 289 작업 지시 — present 간격 히스토그램 / Task 289 work order — Present interval histogram

설계: [20260915-289-present-interval-histogram.md](../design/20260915-289-present-interval-histogram.md)

## 한국어

### 변경 목록

| # | 파일 | 변경 |
| --- | --- | --- |
| 1 | `include/re2dj/graphics/present_interval_histogram.h` | 플랫폼 중립 누적기: `Add(milliseconds)`, `Summarize()`, `Reset()` |
| 2 | `src/graphics/present_interval_histogram.cpp` | 구현 |
| 3 | `src/platform/windows/direct3d3_com_facade.cpp` | `RecordPresentedFrame`에서 간격 누적, FPS 창이 닫힐 때 요약 한 줄 |
| 4 | `tests/unit/present_interval_histogram_test.cpp` | 단위 테스트 |
| 5 | `CMakeLists.txt`, `tests/unit/main.cpp`, `tests/unit/test_support.h` | 등록 |

### 제약

* 누적기는 공용 코어에 둔다. Windows 타입이나 로깅에 의존하지 않는다.
* present 경로에서 파일 I/O나 문자열 포맷을 하지 않는다. 카운터 증가만 한다.
* 요약은 기존 FPS 창 주기를 그대로 쓴다. 새 타이머를 만들지 않는다.
* 기록 예산은 기본 120회, complete diagnostics에서는 무제한.
* 첫 present는 이전 시각이 없으므로 누적하지 않는다.

### 검증

1. Windows x86 Debug/Release 빌드.
2. 단위 테스트 통과, CTest 실행(기존 실패 1건은 그대로 기존 실패로 확인).
3. `ez2dj3rd` 기본값과 `--vsync off` 실행으로 요약 줄 수집.
4. `ez2d2m` 실행으로 `Flip` 경로에서도 기록되는지 확인.

### 범위에서 뺀 것

* 프레임률 수정.
* `Sleep`·`timeGetTime` 후킹.
* 기존 `FrameDraws` 요약의 변경이나 제거.

## English

### Change list

| # | File | Change |
| --- | --- | --- |
| 1 | `include/re2dj/graphics/present_interval_histogram.h` | Platform-neutral accumulator: `Add(milliseconds)`, `Summarize()`, `Reset()` |
| 2 | `src/graphics/present_interval_histogram.cpp` | Implementation |
| 3 | `src/platform/windows/direct3d3_com_facade.cpp` | Accumulate the interval in `RecordPresentedFrame` and emit one summary when the FPS window closes |
| 4 | `tests/unit/present_interval_histogram_test.cpp` | Unit tests |
| 5 | `CMakeLists.txt`, `tests/unit/main.cpp`, `tests/unit/test_support.h` | Registration |

### Constraints

* The accumulator lives in the shared core and depends on no Windows type or logging.
* No file I/O or string formatting on the present path — only a counter increment.
* The summary reuses the existing FPS window; no new timer.
* The record budget is 120 summaries by default and unbounded under complete diagnostics.
* The first present has no previous timestamp and is not accumulated.

### Verification

1. Windows x86 Debug and Release builds.
2. Unit tests pass; CTest runs, with the one known failure confirmed as pre-existing.
3. Collect summary lines from `ez2dj3rd` with the default policy and with `--vsync off`.
4. Run `ez2d2m` to confirm the record also appears on the `Flip` path.

### Excluded from scope

* Changing the frame rate.
* Hooking `Sleep` or `timeGetTime`.
* Changing or removing the existing `FrameDraws` summary.
