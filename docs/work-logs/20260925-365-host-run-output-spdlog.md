# 작업 365 작업 로그 — host 실행 출력의 spdlog 전환 / Task 365 work log — Host run output through spdlog

설계: [20260925-365-host-run-output-spdlog.md](../design/20260925-365-host-run-output-spdlog.md)
작업 지시서: [20260925-365-host-run-output-spdlog.md](../work-orders/20260925-365-host-run-output-spdlog.md)

## 변경 / Changes

- **CLI** (`src/host/cli/main.cpp`). `LogInfo`와 `HexBytes`를 추가하고, 도움말·버전을 뺀 출력을 모두 logger로 옮겼다. `PrintGuestText`는 문자열을 돌려주는 `QuoteGuestText`가 됐다. 호출 기록 한 줄, fault byte 창, stack word는 문자열로 조립한 뒤 한 번에 기록한다.
  ***CLI.** Added `LogInfo` and `HexBytes` and moved all output except help and version to the logger; `PrintGuestText` became the string-returning `QuoteGuestText`, and each call-record line, fault byte window, and stack-word line is assembled before being logged once.*
- **Windows launcher.** `DiagnosticLog`은 spdlog 파일 sink(pattern `%v`)로 JSONL을 쓴다. `--trace` 복사는 debug, 오류 JSON은 `LogLauncherError`, 성공 요약은 `LogLauncherInfo`로 보낸다. 오류 문자열 끝의 글자 `\n` 오타를 없앴다.
  ***Windows launcher.** `DiagnosticLog` writes the JSONL through an spdlog file sink (pattern `%v`); the `--trace` copy goes to debug, error JSON through `LogLauncherError`, and the success summary through `LogLauncherInfo`, with the literal `\n` typos at the end of error strings removed.*
- **CMake.** `re2dj_windows_original_process_backend`가 `re2dj_logging`에 link한다.
  ***CMake.** `re2dj_windows_original_process_backend` links `re2dj_logging`.*

## 검증 / Validation

| 항목 / Item | 결과 / Result |
| --- | --- |
| Linux x64·x86 build, CTest | 경고·오류 없음, 각각 3/3 / no warnings or errors, 3/3 each |
| Windows x86 build, CTest | 오류·경고 없음, 6/6 / no errors or warnings, 6/6 |
| Linux x86 실제 4th / real 4th | stdout 0줄. 대상 정보, 호출 기록, Hardlock 통계, 정지 경계가 `[시각] [info] [re2dj]` 형식으로 stderr와 `logs/re2dj-*.log`에 남음 / nothing on stdout; target details, call record, Hardlock counts, and the stop boundary appear as `[time] [info] [re2dj]` on stderr and in `logs/re2dj-*.log` |
| Windows 실제 4th, 15초 뒤 종료 / real 4th, stopped after 15 s | stdout 0 byte. JSONL 첫 줄은 `{"event":"launch",...}` 그대로. re2dj 로그에 `launcher diagnostic log: …jsonl`이 남음 / stdout 0 bytes; the JSONL still starts with `{"event":"launch",...}`, and the re2dj log records `launcher diagnostic log: …jsonl` |
