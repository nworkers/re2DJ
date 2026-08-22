# Windows 로더 진단 로그 작업 지시

관련 설계: [Windows 로더 진단 로그](../design/20260823-039-windows-loader-diagnostic-log.md)

## 작업 목표

`re2dj_windows_x86_launcher_probe` 실행의 진단 이벤트와 실패 결과를 JSONL 파일로 남겨, 보호된 `ez2dj.exe`의 재현 실패를 실행 직후 분석할 수 있게 한다.

## 수행 항목

1. launcher 전용 진단 로그 구성요소를 추가하고 실행별 로그 경로를 생성한다.
2. 기존 debug trace와 오류·최종 결과를 파일에도 기록하되 `--trace`의 stderr 동작은 유지한다.
3. 생성 경로를 최종 콘솔 JSON에 표시하고 생성 로그 디렉터리를 Git ignore에 추가한다.
4. Windows x86 Debug 빌드·CTest 및 `ez2dj1stse` 실실행으로 로그 생성과 실패 기록을 검증한다.
5. 설계·아키텍처·작업 로그에 결과와 관찰 한계를 반영한다.

## 완료 기준

실행 가능한 대상이 해석된 뒤 launcher가 실행별 JSONL 파일을 생성하고, 정상·실패 결과 모두에서 사용자가 해당 파일 경로와 최종 상태를 확인할 수 있다.

---

# Windows Loader Diagnostic Log Work Order

Related design: [Windows Loader Diagnostic Log](../design/20260823-039-windows-loader-diagnostic-log.md)

## Goal

Record diagnostic events and failures from `re2dj_windows_x86_launcher_probe` in JSONL so a reproduction failure of the protected `ez2dj.exe` can be analyzed immediately after the run.

## Tasks

1. Add a launcher-specific diagnostic-log component and create a per-run log path.
2. Record existing debug traces, errors, and final results in the file while preserving `--trace` stderr behavior.
3. Show the generated path in final console JSON and Git-ignore the generated directory.
4. Verify log generation and failure recording through a Windows x86 Debug build, CTest, and a live `ez2dj1stse` run.
5. Reflect results and evidence limits in the design, architecture, and work log.

## Completion criteria

Once an executable target is resolved, the launcher creates one JSONL file per run, and users can identify both its path and final state for successful and failed runs.
