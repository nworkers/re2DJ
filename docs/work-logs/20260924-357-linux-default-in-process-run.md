# 작업 357 작업 로그 — Linux 기본 실행을 in-process 경로로 전환 / Task 357 work log — Make in-process the default Linux run

설계: [20260924-357-linux-default-in-process-run.md](../design/20260924-357-linux-default-in-process-run.md)
작업 지시서: [20260924-357-linux-default-in-process-run.md](../work-orders/20260924-357-linux-default-in-process-run.md)

## 배경 / Background

사용자가 "x64 `re2dj`가 여전히 helper를 요구한다"고 보고했다. 확인해 보니 CLI의 기본 `--run`(진단 옵션 없음)이 두 폭 모두 `--linux-helper <path>`를 요구하고, i386 helper IPC 경로로 가고 있었다. in-process 경로는 `--linux-in-process-*` 옵션을 줄 때만 쓰였다. 작업 353에서 helper를 진단 fallback으로 정했지만, 그 결정이 CLI 기본값에는 반영되지 않았던 것이다.

*The user reported that the x64 `re2dj` still requires the helper. The CLI's default `--run` (no diagnostic option) required `--linux-helper <path>` on both widths and took the i386 helper IPC path; the in-process path was used only with a `--linux-in-process-*` option. Task 353 made the helper a diagnostic fallback, but that decision had not reached the CLI default.*

## 변경 / Changes

- `src/host/cli/main.cpp`: Linux 실행 경로 선택을 `RunLinuxOriginal`과 `IsLinuxContinuationRun`으로 모았다. 옵션이 없으면 in-process continuation을, `--linux-helper`만 주면 helper를 선택한다. CHD 입력과 디렉터리 입력의 중복 삼항 연산 블록과 `--linux-helper` 필수 오류를 없앴다. 옵션 없이 실행해도 API 호출 기록을 출력한다.
  *`src/host/cli/main.cpp`: consolidated the Linux run-path selection into `RunLinuxOriginal` and `IsLinuxContinuationRun`, choosing the in-process continuation with no option and the helper with `--linux-helper` alone; removed the duplicated ternary blocks for CHD and directory input and the required-`--linux-helper` error. The API call record is printed for an option-less run too.*
- usage: `--linux-helper`를 진단 fallback으로 설명하고, `--linux-in-process-*`의 "Linux x86"을 "Linux"로 고쳤다.
  *Usage: describes `--linux-helper` as a diagnostic fallback and changes "Linux x86" to "Linux" for `--linux-in-process-*`.*
- `README.md`: 실행 방식 설명과 `--linux-helper` 옵션 설명을 현재 동작에 맞췄다.
  *`README.md`: aligned the execution description and the `--linux-helper` option text with current behavior.*

## 검증 / Validation

WSL2 Ubuntu 24.04에서 실행했다. 실제 4th CHD(`roms/ez2dj4th/4thTrax.chd`)는 읽기 전용으로 사용했다.

*Run under WSL2 Ubuntu 24.04 with the real 4th CHD (`roms/ez2dj4th/4thTrax.chd`) read-only.*

| 명령 / Command | x64 | x86 |
| --- | --- | --- |
| `re2dj ez2dj4th` (positional, `--run` 자동 / implied) | in-process. SEH 1회, API 16개, `#0016 ExitProcess`에서 정지 / SEH once, 16 APIs, stop at `#0016` | 같음 / same |
| `re2dj ez2dj4th --run` | 같음 / same | 같음 / same |
| `re2dj ez2dj4th --linux-helper <helper>` | helper 경로, `first boundary: kernel32.dll!GetModuleHandleA`, return `0x00ae028a` / helper path | 같음 / same |
| `--linux-in-process-*` 다섯 개 / five options | 작업 356과 같은 경계 / same boundaries as Task 356 | — |

- Linux x64·x86 debug와 x86 helper build: 경고·오류 없음. CTest는 각각 3/3 통과.
  *Linux x64/x86 debug and the x86 helper build have no warnings or errors, and CTest passes 3/3 on each.*
- Windows x86 빌드는 실행하지 않았다. 바뀐 CLI 코드는 모두 `#if defined(__linux__)` 안에 있다.
  *The Windows x86 build was not run; every CLI change is inside `#if defined(__linux__)`.*
