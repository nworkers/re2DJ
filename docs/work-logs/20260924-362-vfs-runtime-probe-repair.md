# 작업 362 작업 로그 — `re2dj_windows_vfs_runtime_probe` 복구 / Task 362 work log — Repair `re2dj_windows_vfs_runtime_probe`

설계: [20260924-362-vfs-runtime-probe-repair.md](../design/20260924-362-vfs-runtime-probe-repair.md)
작업 지시서: [20260924-362-vfs-runtime-probe-repair.md](../work-orders/20260924-362-vfs-runtime-probe-repair.md)

## 조사 / Investigation

- 작업 360·361 변경을 stash한 HEAD에서도 같은 실패가 났다. 그래서 기존 결함으로 확정했다. 첫 메시지는 `audio trace omitted streaming start`였다. trace에서 32 byte buffer(flags `0x000140c6`)는 `is_streaming=0`으로 만들어졌다.
  *HEAD with Tasks 360–361 stashed failed the same way, confirming a pre-existing defect. The first message was `audio trace omitted streaming start`; in the trace, the 32-byte buffer (flags `0x000140c6`) was created with `is_streaming=0`.*
- `STATIC`을 빼자 다음 실패인 `streaming wrap refresh`가 드러났다. 실제 기록은 `lock-offset=24 first=8 second=8 shadow-offset=0 shadow-bytes=32 ... backend-refresh=1`였다.
  *With `STATIC` removed, the next failure, `streaming wrap refresh`, surfaced; the actual record was `lock-offset=24 first=8 second=8 shadow-offset=0 shadow-bytes=32 ... backend-refresh=1`.*
- 기대값을 고친 뒤에는 검사가 모두 통과(`passed=1`)했지만, 종료 코드가 3이었다. 이것이 사용자가 본 크래시다. 원인은 `remove_all`의 처리되지 않은 예외다.
  - 먼저 오류 32가 나왔다. 개별 삭제로 확인하니 hdd·overlay의 `ABSOLUTE.TXT`가 열린 채였다.
  - 그것을 고치자 실행의 일부에서 "directory is not empty"가 나왔다. 남은 파일은 `audio.log` 하나였다. mixer thread가 검사 뒤에 다시 쓴 것이다.
  *After fixing the expectation, every check passed (`passed=1`) but the exit code was 3. This is the crash the user saw, caused by an unhandled exception from `remove_all`:*
  - *First came error 32; per-file deletion showed `ABSOLUTE.TXT` in hdd and overlay still open.*
  - *After that fix, some runs reported "directory is not empty". The only leftover was `audio.log`, rewritten by the mixer thread after the checks.*

## 변경 / Changes

`src/tools/windows_vfs_runtime_probe/main.cpp`만 바꿨다. 설계의 네 결정을 그대로 적용했다.

*Only `src/tools/windows_vfs_runtime_probe/main.cpp` changed, applying the design's four decisions as written.*

## 검증 / Validation

| 항목 / Item | 결과 / Result |
| --- | --- |
| Windows x86 build | 오류·경고 없음 / no errors or warnings |
| probe 직접 실행 12회 / 12 direct probe runs | 모두 종료 코드 0 / all exit code 0 |
| `ctest --preset windows-x86-debug` 3회 / 3 runs | 매번 6/6 통과 / 6/6 each time |

수정 전에는 같은 방식의 직접 실행 8회 중 6회가 종료 코드 3이었다.

*Before the fix, 6 of 8 direct runs of the same kind exited with code 3.*
