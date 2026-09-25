# 작업 365 설계 — host 실행 출력의 spdlog 전환 / Task 365 design — Host run output through spdlog

선행: [작업 345 설계](20260921-345-spdlog-runtime-logging.md)

## 배경 / Background

`re2dj`는 시작할 때 spdlog logger(`re2dj::logging`)를 만든다. 이 logger는 stderr 색상 sink와 `logs/re2dj-*.log` 파일 sink를 가진다. 그러나 실행 결과는 대부분 이 logger를 거치지 않았다.

*`re2dj` creates an spdlog logger (`re2dj::logging`) at startup, with a colored stderr sink and a `logs/re2dj-*.log` file sink, yet most run output bypassed it:*

- CLI(`src/host/cli/main.cpp`)는 대상·PE 정보, Linux 실행의 호출 기록·Hardlock 통계·정지 경계·fault를 약 110개의 `std::printf`로 stdout에 썼다. 그래서 로그 파일에는 시작 두 줄과 오류만 남았다.
  *The CLI wrote target and PE details and the Linux run's call record, Hardlock counts, stop boundary, and fault to stdout through about 110 `std::printf` calls, leaving the log file with the two start lines and errors.*
- Windows launcher(`src/tools/windows_x86_launcher_probe/main.cpp`)는 JSONL 실행 기록을 `std::ofstream`으로 따로 썼다. 오류 JSON 약 45줄과 성공 요약은 `fprintf(stderr)`/`printf`로 냈다. 일부 오류 문자열은 끝에 줄바꿈 대신 글자 `\n`을 찍고 있었다.
  *The Windows launcher wrote its JSONL run record through its own `std::ofstream`, printed about 45 error JSON lines and the success summary with `fprintf(stderr)`/`printf`, and some error strings printed a literal `\n` instead of ending the line.*

게스트 process 안의 injected runtime trace는 작업 366에서 다룬다.

*The injected runtime's traces inside the guest process are Task 366.*

## 결정 / Decisions

1. **stdout에 남는 것.** `--help`, `--version`, 사용법 출력만 stdout에 남긴다. 명령의 결과물이기 때문이다. 대상 목록과 `--resolve` 결과를 포함한 나머지는 실행 기록으로 보고 logger의 info로 보낸다. 저장소의 script나 test 중 CLI stdout을 읽는 것은 없다.
   ***What stays on stdout:** only `--help`, `--version`, and usage, which are command results. Everything else, including the target list and `--resolve` answers, is run record and goes to the logger at info. No script or test in the repository reads the CLI's stdout.*
2. **CLI.** printf 형식을 그대로 받는 `LogInfo`를 `LogError` 옆에 둔다. 한 줄을 여러 `printf`로 조립하던 곳(호출 기록, fault byte·stack, guest 문자열)은 문자열을 먼저 만들고 한 번에 기록한다. stdout에서 문단을 나누던 앞쪽 빈 줄은 없앤다.
   ***CLI.** `LogInfo`, taking printf formats as they are, sits next to `LogError`. Places that assembled one line from several `printf` calls (the call record, fault bytes and stack, guest strings) build the string first and log it once; leading blank lines that separated stdout paragraphs are dropped.*
3. **Windows launcher.**
   - JSONL 실행 기록은 spdlog logger `launcher-diagnostic`의 파일 sink로 쓴다. pattern은 `%v`라서 파일 형식(한 줄에 JSON 하나)은 그대로다. 파일 경로는 re2dj logger에 info로 남긴다.
     *The JSONL run record is written by the spdlog logger `launcher-diagnostic` through a file sink with pattern `%v`, so the file format (one JSON object per line) is unchanged; its path is logged at info on the re2dj logger.*
   - `--trace`의 console 복사는 re2dj logger의 debug로 보낸다.
     *The `--trace` console copy goes to the re2dj logger at debug.*
   - 오류 JSON은 `LogLauncherError`로 re2dj logger의 error에, 성공 요약은 `LogLauncherInfo`로 info에 보낸다. 글자 `\n` 오타는 없앤다.
     *Error JSON goes through `LogLauncherError` to the re2dj logger at error and the success summary through `LogLauncherInfo` at info, with the literal `\n` typos removed.*
   - launcher probe 자체의 사용법은 stdout에 남긴다.
     *The launcher probe's own usage text stays on stdout.*
   - `re2dj_windows_original_process_backend`는 `re2dj_logging`에 link한다.
     *`re2dj_windows_original_process_backend` links `re2dj_logging`.*
4. **바꾸지 않는 것.** image dump JSON(`process_image_dump.cpp`)은 기록이 아니라 산출물 파일이므로 그대로 둔다. logger pattern과 level도 그대로다.
   ***Unchanged:** the image dump JSON (`process_image_dump.cpp`) is an output artifact, not a record; the logger pattern and levels stay as they are.*
