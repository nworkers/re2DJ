# 작업 443 작업 로그 — 자식 run은 실행 중인 그 실행 파일로 / Task 443 work log — a child run from the very executable running

설계: [20261003-443-linux-child-self-exe.md](../design/20261003-443-linux-child-self-exe.md) · 지시서: [20261003-443-linux-child-self-exe.md](../work-orders/20261003-443-linux-child-self-exe.md)

## 2026-10-03

- **사용자 실행**(`20261003-152557-244` launcher, `-152558-094` 6th, `-152646-266` 1st)
  - 6th `ExitProcess(0x100)` → 1st `ExitProcess(0x105)`(작업 438~440의 복귀 경로가 동작함).
  - 그 뒤 launcher의 세 번째 `CreateProcessA`가 `cannot start the child run: No such file or directory`로 멈췄다.
  - `build/linux-x64-debug/bin/re2dj`의 수정 시각이 15:29:05로, 실행 도중이었다. 작업 442의 로컬 빌드가 실행 중인 파일을 교체한 것이다.

  *User run (launcher `20261003-152557-244`, 6th `-152558-094`, 1st `-152646-266`): 6th `ExitProcess(0x100)` → 1st `ExitProcess(0x105)`, the return path of tasks 438 to 440 working; the launcher's third `CreateProcessA` then stopped with `cannot start the child run: No such file or directory`. `build/linux-x64-debug/bin/re2dj` was modified at 15:29:05, mid-run: task 442's local build replaced the file being run.*
- **재현**: 실행 파일 사본으로 launcher를 띄우고 0.15초 뒤 사본을 지웠다. 그러자 launcher가 첫 `CreateProcessA`에서 같은 오류로 멈췄다(두 번 재현).
  *Reproduction: starting a launcher from a copy of the executable and deleting the copy 0.15 s later stopped the launcher at its first `CreateProcessA` with the same error, twice.*
- **수정**: `LinuxHostProcessLauncher::Start`가 `readlink`로 얻은 경로 대신 `/proc/self/exe`를 `posix_spawn`한다. 쓰지 않게 된 `<limits.h>`를 뺐다.
  *Fix: `LinuxHostProcessLauncher::Start` `posix_spawn`s `/proc/self/exe` rather than the path `readlink` gives; the now unused `<limits.h>` went.*
- **검증**
  - 같은 재현에서 6th 자식이 떠서 8초 동안 `Flip`을 반복했다. 끝낼 때 launcher는 `ExitProcess(0)`이었다.
  - `scripts/test_all.sh linux-x64-debug`(경고를 오류로) build 성공, CTest 4개 통과.

  *Verification: in the same reproduction the 6th child started and repeated `Flip` for 8 seconds, the launcher ending with `ExitProcess(0)` when stopped; `scripts/test_all.sh linux-x64-debug` (warnings as errors) builds and passes 4 CTest tests.*
- **사용자 확인**: 수정한 빌드로 Linux에서 6th → Remember 1st → 게임 한 판 → 6th 복귀가 동작하는 것을 사용자가 확인했다.
  *User check: with the fixed build the user confirmed 6th → Remember 1st → one game → back to 6th on Linux.*
- **작업 442의 태그 실행**: `v0.0.60` 태그로 돈 Release workflow(`37127407384`)가 성공했다. publish 단계가 처음 실제로 돌아, release에 Windows x86 zip, Linux x64·x86 tar.gz와 각 `.sha256` 여섯 파일을 올렸다.
  *Task 442's tag run: the Release workflow on the `v0.0.60` tag (`37127407384`) succeeded, its publish step running for real for the first time and putting six files on the release: the Windows x86 zip, the Linux x64 and x86 tar.gz and each `.sha256`.*
