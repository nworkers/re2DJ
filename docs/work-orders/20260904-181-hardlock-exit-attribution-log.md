# Task 181: Hardlock 종료 귀속 로그 추가와 검토 종료

## 작업 목표

나중에 Hardlock 때문에 종료가 일어날 경우 즉시 가릴 수 있는 로그를 남기고, Hardlock 방향의 조사를 닫습니다.

## 선행 문서

- [Task 181 설계](../design/20260904-181-hardlock-exit-attribution-log.md)
- [Task 180 작업 로그](../work-logs/20260904-180-ez2dj4th-exit-process-source.md)
- [4th Hardlock 런타임 분석](../analysis/ez2dj4th-hardlock-runtime.md)

## 구현 범위

1. **누적 상태.** `src/platform/windows/injected_runtime.cpp`의 `CompleteHardlockRequest`가 요청마다 종류별 횟수, 거절 횟수, 마지막 요청의 종류·결과·바이트 수·시각(`GetTickCount64`)을 갱신합니다.
2. **종료 기록 확장.** `Re2djHleExitProcess`가 `re2dj:vfs:exit-process-hardlock` 한 줄을 추가로 남깁니다. 항목은 설계 3절의 표를 따릅니다.
3. **문서 정정.** Task 180 작업 로그와 `docs/analysis/ez2dj4th-hardlock-runtime.md`에서 `answered=0`을 "응답되지 않음"으로 읽은 부분을 바로잡습니다. 그 필드는 handshake 전용입니다.
4. **진단 실행과 확인.** 진입 추적을 실행해 새 줄이 남고 값이 요청별 줄과 맞는지 확인합니다.
5. **작업 로그.** 결과와 Hardlock 검토 종료 근거를 남깁니다.

## 비범위

- Hardlock 응답 material 변경.
- 종료를 막거나 우회.
- 새 CLI 옵션 추가.
- 보호 스텁 내부 분기 해석.

## 최소 검증

```powershell
cmd /c scripts\build_win32.bat
.\build\windows-x86\bin\Debug\re2dj_unit_tests.exe
.\build\windows-x86\bin\Debug\re2dj_windows_product_loader_probe.exe
```

실제 CHD가 있으면 진입 추적을 실행합니다.

```powershell
$common = '--hdd', "$env:TEMP\re2dj\chd\ez2dj4th", '--target', 'ez2dj4th',
  '--chd', 'roms\ez2dj4th\4thTrax.chd',
  '--target-executable', 'EZ2DJ/EZ2DJ.EXE',
  '--hle-vfs', '--hle-dynamic-vfs', '--hle-d3d3', '--hle-io-ports',
  '--device-mock-lptdi', '--device-mock-lptdi-path-prefix', '\\.\FEnteDev',
  '--device-mock-wts-console-session', '--diagnostic-idle-timeout', '60000'
.\build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe @common --null-context-entry-trace
```

## 자기 검증 기준

- `exit-process-hardlock`의 `total`이 `.vfs.log`의 `hardlock-device` 줄 수와 같아야 합니다.
- 종류별 횟수의 합이 `total`과 같아야 합니다.
- `last_kind`가 마지막 `hardlock-device` 줄의 종류와 같아야 합니다.
- 로그에는 요청 종류와 개수, 바이트 수만 남기고 Hardlock secret 값은 남기지 않습니다.

---

# Task 181: Hardlock Exit Attribution Log and Closing The Investigation

## Goal

Leave the evidence needed to attribute a future exit to Hardlock immediately, and close the Hardlock line of investigation.

## Scope

1. Track per-kind counts, the rejected count, and the last request's kind, outcome, byte count, and tick in `CompleteHardlockRequest`.
2. Have `Re2djHleExitProcess` write an additional `re2dj:vfs:exit-process-hardlock` record following the design's table.
3. Correct the Task 180 work log and the analysis topic where `answered=0` was read as "unanswered"; that field is handshake-specific.
4. Run the entry trace and confirm the new record agrees with the per-request lines.
5. Write the work log, including why the Hardlock direction is being closed.

## Out of Scope

Hardlock material changes, blocking the exit, new CLI options, and reading the stub's internal branches.

## Minimum Verification

Build, unit tests, and the product loader probe, then the entry trace against the real CHD.

## Self-Check

The record's `total` must equal the number of `hardlock-device` lines, the per-kind counts must sum to it, and `last_kind` must match the final request line. Logs record request kinds, counts, and byte sizes only — never Hardlock secrets.

---

## 범위 확장 기록 (Scope Extension Record)

검증 실행에서 종료 관측 wrapper가 모든 종료를 덮지 못한다는 것이 드러나, 한 항목을 추가했습니다.

- 한 실행은 종료 코드 `1`로 끝났는데 `exit-process` 기록이 남지 않았습니다. 이 경로는 동적으로 해석된 `ExitProcess`를 지나지 않습니다. 종료 코드 `0xffffffff`로 끝난 실행에서는 wrapper가 정상 동작했으므로, 서로 다른 두 종료 경로가 존재합니다.
- 따라서 `DLL_PROCESS_DETACH`에서도 귀속 기록을 남깁니다. detach는 모든 종료가 반드시 지나는 지점입니다. wrapper가 이미 기록했으면 중복해서 남기지 않고, 남기는 것은 `exit-detach` 한 줄과 Hardlock 요약 한 줄뿐입니다. 이 시점은 loader lock 아래이므로 그 두 줄로 제한합니다.

Verification showed the wrapper does not cover every exit: one run ended with code `1` and wrote no `exit-process` record, so that path never passes the dynamically resolved `ExitProcess`, while the `0xffffffff` run did reach the wrapper. The attribution records are therefore also written from `DLL_PROCESS_DETACH`, the one point every exit crosses, limited to two lines because it runs under the loader lock.

추가 검증에서 detach도 그 경로를 덮지 못한다는 것이 드러나, 한 항목을 더 넣었습니다.

- 종료 코드 `1` 실행은 `exit-process`도 `exit-detach`도 남기지 않았습니다. `DLL_PROCESS_DETACH`까지 건너뛴다는 것은 `ExitProcess`가 아니라 자기 프로세스를 강제 종료한다는 뜻입니다. 같은 실행의 동적 resolver 기록에 `TerminateProcess`가 있습니다.
- 따라서 `TerminateProcess`에도 같은 관측 wrapper를 답니다. 대상이 자기 프로세스일 때만 기록하고, 동작은 그대로 넘깁니다. 기록에는 `route` 필드를 추가해 `exit_process`와 `terminate_process`를 구분합니다.

A further check showed detach does not cover that path either: the code-`1` run wrote neither record, so it skips `DLL_PROCESS_DETACH` and is therefore a self-termination rather than an `ExitProcess`, and the same run's resolver log contains `TerminateProcess`. The same observation wrapper is therefore attached to `TerminateProcess`, recording only when the target is this process and passing the call through unchanged, with a `route` field added to separate the two.
