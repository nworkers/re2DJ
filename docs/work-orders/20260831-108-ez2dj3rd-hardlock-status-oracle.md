# ez2dj3rd Hardlock status 경계 진단 작업 지시

관련 설계: [ez2dj3rd Hardlock status 경계 진단](../design/20260831-108-ez2dj3rd-hardlock-status-oracle.md)

*Related design: [ez2dj3rd Hardlock status-boundary diagnostic](../design/20260831-108-ez2dj3rd-hardlock-status-oracle.md).*

## 범위

1. 현재 branch와 worktree, 원본 경로 및 기존 descriptor 관찰 주소를 재확인합니다.
2. 기존 launcher로 `roms/ez2dj3rd` 원본을 bounded child process에서 실행합니다.
3. 실제 process ID의 `0x00a672aa`가 정확히 38일 때만 0으로 한 번 변경하고 read-back을 기록합니다.
4. 같은 실행의 VFS 로그에서 후속 Hardlock request code와 buffer 크기를 수집합니다.
5. status 변경이 분기보다 늦은 경우 동적 resolver 이름을 bounded VFS 진단으로 기록하고 전용 runtime probe로 route 보존과 marker를 검증합니다.
6. 확인된 Terminal Services query를 원래 API로 전달하는 관찰 wrapper로 info class와 bounded scalar 결과를 수집합니다.
7. 명시적 분석 옵션으로 현재 session의 성공한 class 4 결과만 활성 console 상태로 HLE하고 후속 Hardlock 요청을 재측정합니다.
8. 살아 있는 진단 child만 종료하고, 분석 문서와 작업 로그를 확인/추정/미확정으로 구분해 갱신합니다.
9. 영향 대상 Windows x86 probe를 빌드·실행하고 `git diff --check`를 수행합니다.

*Reconfirm the branch, worktree, original path, and observed descriptor address; run the original from `roms/ez2dj3rd` as a bounded child; write zero once only if that process's `0x00a672aa` equals 38 and record the read-back; collect later Hardlock request codes and buffer sizes from the same run; if the write is too late, add a bounded dynamic-resolver name diagnostic and verify route preservation plus its marker in the runtime probe; terminate only a surviving diagnostic child; update cumulative analysis and the work log with confirmed/inferred/unresolved labels; and build and run the affected Windows x86 probe plus `git diff --check`.*

## 완료 조건

- 원본 파일과 overlay는 변경되지 않습니다.
- 대상 PID, 원래 status, 변경값, read-back 및 후속 request 여부가 같은 실행 증거로 연결됩니다.
- status 38의 인과성은 관찰 결과보다 강하게 기술하지 않습니다.
- 실제 dongle response나 seed가 아닌 synthetic 값을 유효값으로 승격하지 않습니다.
- 분석 종료 뒤 대상 child가 남지 않습니다.

*The original files and overlay remain unchanged; target PID, original status, new value, read-back, and later requests are tied to evidence from one run; causality is not stated more strongly than observed; synthetic values are not promoted to dongle responses or seeds; and no target child remains after the analysis.*
