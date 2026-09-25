# 작업 370 작업 로그 — 게스트 API 호출 기록 / Task 370 work log — Guest API call log

설계: [20260925-370-guest-api-call-log.md](../design/20260925-370-guest-api-call-log.md)
작업 지시서: [20260925-370-guest-api-call-log.md](../work-orders/20260925-370-guest-api-call-log.md)

## 변경 / Changes

- **`api_call_record.h/.cpp`**(새 파일): 서비스 장식자, 호출 기록, 기록 형식.
  ***`api_call_record.h/.cpp`** (new): the services decorator, the call record, and its formatting.*
- **logging**: API logger(`*.api.log`, console 없음, warn 이상에서 flush, Shutdown에서 flush). CLI는 시작할 때 `API log : …` 경로를 남긴다.
  ***logging:** the API logger (`*.api.log`, no console, flushing at warn and on Shutdown); the CLI logs the `API log : …` path at start.*
- **Linux**: `NativeKernel32Diagnostic::Dispatch`가 장식자로 dispatch하고, `last_record()`와 `dispatch_error()`를 준다. continuation의 `LogApiCall`이 호출 줄, `arg` 줄, 기록 줄, 결과 줄을 쓴다. `DeviceIoControl`은 `[withheld]`로 적는다.
  ***Linux:** `NativeKernel32Diagnostic::Dispatch` dispatches through the decorator and exposes `last_record()` and `dispatch_error()`; continuation's `LogApiCall` writes the call line, `arg` lines, record lines, and the outcome, with `DeviceIoControl` as `[withheld]`.*
- **단위 테스트**(`api_call_record_test.cpp`): 전달, 문자열·byte 읽기, 실패한 읽기, 쓰기, ASCII 문자열 표시, 64 byte 절단, last error, 숨김.
  ***Unit tests** (`api_call_record_test.cpp`): forwarding, string and byte reads, a failed read, writes, ASCII string display, the 64-byte cut, the last error, and withholding.*

첫 출력에서 경로가 `D:\x5cez2dj`로 나와 읽기 어려웠다. 그래서 `\`와 `"`는 backslash로 escape하도록 바꿨다. `GetEnvironmentVariableA`처럼 handler가 이름을 읽지 않는 호출은 입력 문자열이 빠졌다. 그래서 continuation이 읽는 문자열 인자를 `arg` 줄로 더했다.

*The first output showed paths as `D:\x5cez2dj`, hard to read, so `\` and `"` are now backslash-escaped; calls whose handler never reads its name, such as `GetEnvironmentVariableA`, lacked the input string, so the string arguments continuation reads were added as `arg` lines.*

## 검증 / Validation

| 항목 / Item | 결과 / Result |
| --- | --- |
| Linux x64·x86 build, CTest | 경고·오류 없음, 각각 3/3 / no warnings or errors, 3/3 each |
| Linux helper, probe, 기존 진단 네 개 / diagnostics | 이전과 같음 / as before |
| Windows x86 build, CTest | 오류·경고 없음, 6/6 / no errors or warnings, 6/6 |
| 실제 4th, 두 폭 / real 4th, both widths | 호출 1,700번, 정지 지점(`timeBeginPeriod`)과 기록이 이전과 같음. x86 `*.api.log`는 약 1만 줄 / 1,700 calls with the stop (`timeBeginPeriod`) and record unchanged; the x86 `*.api.log` is about 10,000 lines |
