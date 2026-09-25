# 작업 365 작업 지시서 — host 실행 출력의 spdlog 전환 / Task 365 work order — Host run output through spdlog

설계: [20260925-365-host-run-output-spdlog.md](../design/20260925-365-host-run-output-spdlog.md)

## 절차 / Steps

1. CLI에 `LogInfo`와 `HexBytes`를 두고, 도움말·버전·사용법을 뺀 `printf`를 모두 logger로 옮긴다. 조립식 출력은 문자열로 바꾼다.
   *Add `LogInfo` and `HexBytes` to the CLI and move every `printf` except help, version, and usage to the logger, turning assembled output into strings.*
2. launcher의 `DiagnosticLog`를 spdlog 파일 sink로 바꾼다. 오류·요약 출력은 `LogLauncherError`/`LogLauncherInfo`로 옮긴다.
   *Switch the launcher's `DiagnosticLog` to an spdlog file sink, and route error and summary output through `LogLauncherError`/`LogLauncherInfo`.*
3. `re2dj_windows_original_process_backend`를 `re2dj_logging`에 link한다.
   *Link `re2dj_windows_original_process_backend` to `re2dj_logging`.*

## 완료 조건 / Done when

- Linux x64·x86, Windows x86 build에 경고가 없고, 각 CTest가 통과한다.
  *Linux x64/x86 and Windows x86 build without warnings and pass CTest.*
- Linux 실제 4th 실행은 stdout을 쓰지 않는다. 호출 기록과 경계는 stderr와 `logs/re2dj-*.log`에 같이 남는다.
  *A Linux real-4th run writes nothing to stdout; the call record and boundary appear on stderr and in `logs/re2dj-*.log`.*
- Windows 실제 4th 실행에서 JSONL은 접두사 없는 JSON 줄로 남는다. re2dj 로그에는 그 경로가 남는다.
  *In a Windows real-4th run the JSONL keeps prefix-free JSON lines and the re2dj log records its path.*
