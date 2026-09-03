# Task 155: bounded 진단 idle timeout 설정

## 작업 목표

launcher probe의 bounded 진단 loop가 사용하는 고정 5초 idle 경계를 CLI로 조정할 수 있게 하여, Task 154에서 끊긴 `ez2dj4th` 관찰 구간을 field read anchor까지 확장할 수 있게 합니다. 기본 동작은 바꾸지 않습니다.

## 선행 문서

- [Task 155 설계](../design/20260903-155-diagnostic-idle-timeout.md)
- [Task 154 작업 로그](../work-logs/20260903-154-ez2dj4th-field-writer-execution-trace.md)
- [4th Hardlock 런타임 분석](../analysis/ez2dj4th-hardlock-runtime.md)

## 구현 범위

1. `--diagnostic-idle-timeout <milliseconds>` 옵션과 usage 문자열을 추가합니다.
2. 값을 `1000`–`600000`으로 검증하고, 기본값은 `5000`으로 둡니다.
3. 값을 `WaitForExitProcessBreakpoint`로 전달해 bounded 진단 loop의 `WaitForDebugEvent` 대기에만 적용합니다.
4. `launch` 진단 event에 `diagnostic_idle_timeout_ms`를 기록합니다.
5. 설계와 작업 로그를 작성하고, 새 관찰 결과가 나오면 누적 분석 문서를 갱신합니다.

## 비범위

- 다른 대기 경로의 timeout 변경
- child 무제한 실행
- field 직접 주입 또는 Hardlock 응답 변경
- `ez2dj4th`용 그래픽·사운드 HLE 신규 구현

## 최소 검증

```powershell
cmd /c scripts\build_win32.bat
.\build\windows-x86\bin\Debug\re2dj_unit_tests.exe
```

실제 CHD가 있으면 기본값 실행과 확장 timeout 실행을 각각 수행하고 boundary reason과 hit 수를 비교합니다. 로그에는 원본 자산 경로 내용이나 Hardlock secret 값을 기록하지 않습니다.

---

# Task 155: Bounded Diagnostic Idle-Timeout Configuration

## Objective

Make the fixed five-second idle boundary of the launcher probe's bounded diagnostic loop configurable from the CLI so the `ez2dj4th` observation window cut short in Task 154 can extend to the field-read anchor. Default behavior is unchanged.

## Preceding documents

- [Task 155 design](../design/20260903-155-diagnostic-idle-timeout.md)
- [Task 154 work log](../work-logs/20260903-154-ez2dj4th-field-writer-execution-trace.md)
- [4th Hardlock runtime analysis](../analysis/ez2dj4th-hardlock-runtime.md)

## Implementation scope

1. Add the `--diagnostic-idle-timeout <milliseconds>` option and usage text.
2. Validate the value in `1000`–`600000` and keep `5000` as the default.
3. Pass the value into `WaitForExitProcessBreakpoint` and apply it only to the bounded diagnostic loop's `WaitForDebugEvent` wait.
4. Record `diagnostic_idle_timeout_ms` in the `launch` diagnostic event.
5. Write the design and work log, and update the cumulative analysis document when new observations result.

## Out of scope

- Changing other wait-path timeouts.
- Running the child without bound.
- Direct field injection or Hardlock-response changes.
- New graphics or sound HLE for `ez2dj4th`.

## Minimum verification

```powershell
cmd /c scripts\build_win32.bat
.\build\windows-x86\bin\Debug\re2dj_unit_tests.exe
```

When the real CHD is available, run once with the default and once with an extended timeout, then compare boundary reasons and hit counts. The logs must not contain original asset contents or Hardlock secret values.
