# 작업 290 작업 지시 — 게스트 대기 계정 / Task 290 work order — Guest wait accounting

설계: [20260915-290-guest-wait-accounting.md](../design/20260915-290-guest-wait-accounting.md)

## 한국어

### 변경 목록

| # | 파일 | 변경 |
| --- | --- | --- |
| 1 | `src/platform/windows/guest_wait_accounting.h/.cpp` | 계정기와 요약 구조체, `TakeGuestWaitSummary` |
| 2 | `src/platform/windows/injected_runtime.cpp` | `Re2djWaitSleep`, `Re2djWaitForSingleObject`, `Re2djWaitTimeGetTime` 래퍼 |
| 3 | `src/platform/windows/direct3d3_com_facade.cpp` | 작업 289 요약 옆에 대기 요약 한 줄 |
| 4 | `src/tools/windows_x86_launcher_probe/main.cpp` | `--guest-wait-trace` 파싱과 IAT 패치 |
| 5 | `include/re2dj/target/target_profile.h`, `original_process_backend.cpp` | 옵션 전달 |
| 6 | `src/host/cli/main.cpp` | 제품 `--guest-wait-trace`와 사용법 |
| 7 | `CMakeLists.txt` | 새 소스 등록 |

### 제약

* 래퍼는 원래 함수를 그대로 호출한다. 인자도 반환값도 바꾸지 않는다.
* 호출 경로에서는 카운터·버킷 증가만 한다. 문자열 포맷과 기록은 요약에서만.
* 옵션이 없으면 IAT 슬롯을 건드리지 않는다.
* 계정기는 여러 스레드에서 호출될 수 있으므로 원자적으로 누적한다.
* 요약 주기는 작업 289의 FPS 창을 그대로 쓴다.

### 검증

1. Windows x86 Release 빌드.
2. 단위 테스트와 CTest 실행(기존 실패 1건 확인).
3. `re2dj ez2dj3rd --guest-wait-trace` 실행으로 대기 요약 수집.
4. 옵션 없는 실행에서 대기 요약이 나오지 않음을 확인.

### 범위에서 뺀 것

* 프레임률 수정.
* DirectSound 커서 계정.
* `CreateEventA`·`SetEvent` 계정.

## English

### Change list

| # | File | Change |
| --- | --- | --- |
| 1 | `src/platform/windows/guest_wait_accounting.h/.cpp` | The accumulator, its summary struct, and `TakeGuestWaitSummary` |
| 2 | `src/platform/windows/injected_runtime.cpp` | The `Re2djWaitSleep`, `Re2djWaitForSingleObject` and `Re2djWaitTimeGetTime` wrappers |
| 3 | `src/platform/windows/direct3d3_com_facade.cpp` | One wait-summary line beside task 289's summary |
| 4 | `src/tools/windows_x86_launcher_probe/main.cpp` | `--guest-wait-trace` parsing and the IAT patches |
| 5 | `include/re2dj/target/target_profile.h`, `original_process_backend.cpp` | Option forwarding |
| 6 | `src/host/cli/main.cpp` | The product `--guest-wait-trace` option and its usage text |
| 7 | `CMakeLists.txt` | Register the new source |

### Constraints

* The wrappers forward the original call unchanged, in arguments and return value.
* The call path only increments counters and buckets; formatting and writing happen at summary time.
* Without the option, no IAT slot is touched.
* The accumulator can be called from several threads, so it accumulates atomically.
* The summary cadence reuses task 289's FPS window.

### Verification

1. Windows x86 Release build.
2. Unit tests and CTest, confirming the one known pre-existing failure.
3. Collect wait summaries from `re2dj ez2dj3rd --guest-wait-trace`.
4. Confirm no wait summary appears without the option.

### Excluded from scope

* Changing the frame rate.
* Accounting the DirectSound cursor.
* Accounting `CreateEventA` and `SetEvent`.
