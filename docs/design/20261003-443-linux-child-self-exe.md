# 작업 443 설계 — 자식 run은 실행 중인 그 실행 파일로 / Task 443 design — a child run from the very executable running

선행: [작업 431 설계](20260930-431-linux-6th-child-process.md)

## 배경 / Background

사용자가 Linux에서 6th → Remember 1st를 한 판 마쳤다. 1st는 `ExitProcess(0x105)`로 끝났지만, launcher는 6th를 다시 띄우는 세 번째 `CreateProcessA`에서 멈췄다: `cannot start the child run: No such file or directory`(실행 `20261003-152557-244`, `-152558-094`, `-152646-266`).

- 그 실행 중(15:29:05)에 `build/linux-x64-debug/bin/re2dj`가 다시 빌드되어 교체됐다. 그 빌드는 같은 시각 작업 442의 로컬 빌드였다.
- `LinuxHostProcessLauncher::Start`는 `readlink("/proc/self/exe")`로 얻은 경로를 `posix_spawn`한다. 실행 파일이 교체되거나 지워지면 그 경로는 `... (deleted)`가 되어 실행할 수 없다.
- 사본 실행 파일로 launcher를 띄운 뒤 자식이 뜨기 전에 사본을 지우면 같은 멈춤이 재현된다.

*The user finished a game of Remember 1st from 6th on Linux. 1st ended with `ExitProcess(0x105)`, but the launcher stopped at the third `CreateProcessA`, starting 6th again: `cannot start the child run: No such file or directory` (runs `20261003-152557-244`, `-152558-094`, `-152646-266`). During that run (15:29:05) `build/linux-x64-debug/bin/re2dj` was rebuilt and replaced, by task 442's local build at the time. `LinuxHostProcessLauncher::Start` `posix_spawn`s the path `readlink("/proc/self/exe")` gives, which becomes `... (deleted)` once the executable is replaced or removed and cannot run. Starting a launcher from a copy of the executable and deleting the copy before the child starts reproduces the stop.*

## 결정 / Decisions

- 자식은 `/proc/self/exe` 자체를 `posix_spawn`한다. 이 링크는 경로가 아니라 지금 실행 중인 실행 파일을 가리키므로([proc_pid_exe(5)](https://man7.org/linux/man-pages/man5/proc_pid_exe.5.html)), 파일이 교체되거나 지워져도 실행된다. 부모와 자식이 항상 같은 빌드가 되어, 둘이 주고받는 자식 옵션의 형식도 어긋나지 않는다.
  *A child `posix_spawn`s `/proc/self/exe` itself. The link points at the executable running rather than at a path ([proc_pid_exe(5)](https://man7.org/linux/man-pages/man5/proc_pid_exe.5.html)), so it runs even after the file is replaced or removed, and parent and child are always the same build, so the child options they exchange always agree.*
- `argv[0]`은 지금처럼 부모의 것을 넘긴다.
  *`argv[0]` stays the parent's, as before.*

## 검증 / Verification

- 위 재현(사본으로 launcher를 띄우고 자식 전에 사본을 지움)에서, 수정 전에는 `CreateProcessA`에서 멈추고 수정 뒤에는 6th 자식이 뜬다.
  *In the reproduction above (a launcher from a copy, the copy deleted before the child), the launcher stops at `CreateProcessA` before the fix and the 6th child starts after it.*
- Linux x64 build와 CTest(경고를 오류로).
  *The Linux x64 build and CTest with warnings as errors.*
