# 작업 357 작업 지시서 — Linux 기본 실행을 in-process 경로로 전환 / Task 357 work order — Make in-process the default Linux run

설계: [20260924-357-linux-default-in-process-run.md](../design/20260924-357-linux-default-in-process-run.md)

## 단계 / Steps

1. `src/host/cli/main.cpp`에서 Linux 실행 경로 선택을 함수 하나로 모은다. 옵션이 없으면 in-process continuation을, `--linux-helper`만 주면 helper를 선택한다. `--linux-helper` 필수 오류를 없앤다.
2. usage 문구와 `README.md`의 `--run`·`--linux-helper` 설명을 고친다.
3. Linux x64·x86 debug를 빌드하고 CTest를 실행한다. 실제 4th CHD로 옵션 없는 `--run`과 `--linux-helper` 경로를 두 폭에서 확인한다.
4. `ARCHITECTURE.md`, `docs/TODO.md`, 작업 로그를 갱신하고 커밋한다.

*Steps: (1) consolidate the Linux run-path selection in `src/host/cli/main.cpp` into one function that chooses the in-process continuation with no option and the helper with `--linux-helper` alone, removing the required-`--linux-helper` error; (2) update the usage text and the `--run`/`--linux-helper` description in `README.md`; (3) build Linux x64/x86 debug, run CTest, and check option-less `--run` and the `--linux-helper` path on the real 4th CHD on both widths; (4) update `ARCHITECTURE.md`, `docs/TODO.md`, and the work log, then commit.*

## 완료 조건 / Completion criteria

* 두 폭에서 `--linux-helper` 없는 `--run`이 in-process로 실행되어 `#0016`에 도달한다.
* `--linux-helper`를 주면 기존 helper 경로가 동작한다.

*Completion: option-less `--run` runs in-process to `#0016` on both widths, and `--linux-helper` still selects the existing helper path.*
