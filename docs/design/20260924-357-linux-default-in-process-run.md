# 작업 357 설계 — Linux 기본 실행을 in-process 경로로 전환 / Task 357 design — Make in-process the default Linux run

선행: [작업 353 설계](20260924-353-linux-x64-compat-mode-adapter.md), [작업 356 작업 로그](../work-logs/20260924-356-linux-x64-guest-seh-trace.md), [작업 333 CLI 설계](20260919-333-linux-cli-inprocess-first-import.md)

## 문제 / Problem

작업 353에서 "Linux 제품 경로는 같은 프로세스 실행, i386 helper는 진단 fallback"으로 정했다. 작업 356에서 x64 in-process 경로가 x86과 같은 경계에 도달했다. 그런데 CLI의 기본 `--run`은 두 폭 모두 여전히 `--linux-helper <path>`를 요구하고, i386 helper IPC 경로(`RunOriginalUntilBoundary`)를 쓴다. in-process 경로는 `--linux-in-process-*` 진단 옵션을 줄 때만 선택된다. 작업 333 설계에서 기본 경로를 helper로 둔 것이 그대로 남은 것이다. 같은 선택 로직이 CHD 입력과 디렉터리 입력 두 곳에 중복되어 있다.

*Task 353 decided that the Linux product path runs in the same process and that the i386 helper is only a diagnostic fallback, and Task 356 brought the x64 in-process path to the same boundaries as x86. Yet the CLI's default `--run` still requires `--linux-helper <path>` on both widths and takes the i386 helper IPC path (`RunOriginalUntilBoundary`); the in-process path is chosen only with a `--linux-in-process-*` diagnostic option. This is the helper default from the Task 333 design, left in place, and the same selection logic is duplicated for CHD and directory input.*

## 결정 / Decision

| 명령 / Command | 경로 / Path |
| --- | --- |
| `re2dj <profile> --run` (옵션 없음 / no option) | in-process continuation (`RunOriginalInProcessContinuation`). helper 불필요 / no helper needed |
| `--linux-in-process-*` | 해당 in-process 진단 / the named in-process diagnostic |
| `--linux-helper <path>` (in-process 옵션 없음 / no in-process option) | i386 helper IPC 진단 fallback / i386 helper IPC diagnostic fallback |

기본 경로로 continuation을 쓰는 이유는 이것이 현재 가장 멀리 가는 in-process 실행이기 때문이다. facade와 SEH를 거쳐 첫 미해석 lookup, 미처리 import, fault 또는 process exit까지 원본을 실행하고, API 호출 기록을 출력한다. 선택 로직은 CLI의 함수 하나로 모아 두 입력 경로가 같이 쓴다. `--linux-in-process-*` 설명의 "Linux x86" 표기는 두 폭 모두 지원하므로 "Linux"로 고친다.

*Continuation is the default because it is currently the furthest-reaching in-process run: it executes the original through the facades and SEH until the first unresolved lookup, unhandled import, fault, or process exit, and prints the API call record. The selection moves into one CLI function shared by both input paths, and the "Linux x86" wording in the `--linux-in-process-*` help becomes "Linux" now that both widths support them.*

## 검증 / Validation

x64·x86 제품 build에서 `re2dj ez2dj4th --run`을 `--linux-helper` 없이 실행한다. 결과가 `--linux-in-process-continue`와 같이 `#0016`에 도달해야 한다. `--linux-helper <helper>`를 주면 기존 helper 경계가 나와야 한다. 옵션 없이 helper 경로가 선택되지 않는지도 확인한다.

*Run `re2dj ez2dj4th --run` without `--linux-helper` on the x64 and x86 product builds and confirm it reaches `#0016` like `--linux-in-process-continue`; confirm that `--linux-helper <helper>` still produces the existing helper boundary, and that the helper path is never chosen without that option.*
