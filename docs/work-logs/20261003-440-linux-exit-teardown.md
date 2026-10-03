# 작업 440 작업 로그 — Linux run의 종료 순서 / Task 440 work log — the Linux run's teardown order

설계: [20261003-440-linux-exit-teardown.md](../design/20261003-440-linux-exit-teardown.md) · 지시서: [20261003-440-linux-exit-teardown.md](../work-orders/20261003-440-linux-exit-teardown.md)

## 2026-10-03

- **재현**: 작업 439의 1st 종료 경로 재현(직접 실행, 그 실행의 메모리에만 쓰기)에서, 게스트는 `ExitProcess(0x105)`로 끝나고 요약이 로그에 남은 뒤 host가 -11(SIGSEGV)로 끝났다. 세 번 반복해 같았다.
  *Reproduction: in task 439's reproduction of 1st's exit (a direct run, writing only into its memory) the guest ended with `ExitProcess(0x105)` and the summary was logged, then the host ended with -11 (SIGSEGV), the same in three runs.*
- **좁히기**
  - `SDL_AUDIODRIVER=dummy`: 여전히 -11.
  - gdb 아래 실행: 게스트 런타임의 SIGTRAP과 충돌해 시작하지 못했다. core dump: apport가 남기지 않았다(`core_pattern`이 apport, 패키지 밖 프로그램).
  - 임시 실험 코드(되돌림): `RunChdTarget`이 돌아온 직후 `_exit` → 0. 전역 host 서비스를 launcher, 오디오, 창 순서로 `main` 안에서 해제 → 0.

  *Narrowing: `SDL_AUDIODRIVER=dummy` still gave -11. Under gdb the run clashed with the guest runtime's SIGTRAP and did not start; apport left no core dump (`core_pattern` is apport, the program is not packaged). Temporary experiment code (reverted): `_exit` right after `RunChdTarget` returned gave 0, and releasing the global host services inside `main` in the order launcher, audio, window gave 0.*
- **구현**: `LinuxWindowHold`를 `LinuxHostLifetime`으로 바꿔, `--hold-window` 대기 뒤 host 서비스를 launcher, 오디오, 창 순서로 해제한다.
  *Implementation: `LinuxWindowHold` became `LinuxHostLifetime`, which after the `--hold-window` wait releases the host services in the order launcher, audio, window.*
- **검증**: `scripts/test_all.sh linux-x64-debug`(경고를 오류로) build 성공, CTest 4개 통과. 재현 실행의 host 종료 상태 0. 어느 정적 객체가 충돌했는지는 미확정으로 남긴다.
  *Verification: `scripts/test_all.sh linux-x64-debug` (warnings as errors) builds and passes 4 CTest tests; the reproduction run's host exit status is 0. Which static object crashed stays unresolved.*
