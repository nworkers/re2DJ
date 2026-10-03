# 작업 440 설계 — Linux run의 종료 순서 / Task 440 design — the Linux run's teardown order

선행: [작업 439 설계](20261003-439-remember-1st-quit-message.md)

## 배경 / Background

Remember 1st의 정상 종료를 재현하자(작업 439), 게스트는 `ExitProcess(0x105)`로 끝나고 요약까지 로그에 남았다. 그런데 host 프로세스가 그 뒤 SIGSEGV로 끝났다.

- `SDL_AUDIODRIVER=dummy`로도 같았다.
- 실행이 돌아온 직후 `_exit`하면 0으로 끝났다.
- 전역 host 서비스(`g_linux_process_launcher`, `g_linux_audio`, `g_linux_presentation`)를 `main` 안에서 이 순서로 해제하면 0으로 끝났다.

이 서비스들은 지금까지 정적 소멸 단계에서 해제됐다. 그때는 다른 번역 단위의 정적 상태가 먼저 사라졌을 수 있는데, 서비스가 해제 중 그 상태를 건드린 것으로 **추정**한다. 1st에는 프로세스가 끝나도 막힌 채 남는 게스트 사운드 스레드가 있다. 어느 정적 객체인지는 **미확정**이다. gdb 아래에서는 게스트 런타임의 SIGTRAP과 충돌해 실행되지 않았고, core dump는 apport가 남기지 않았다.

*Reproducing Remember 1st's normal exit (task 439), the guest ended with `ExitProcess(0x105)` and the summary was logged, but the host process then ended with SIGSEGV. `SDL_AUDIODRIVER=dummy` changed nothing; `_exit` right after the run returned ended with 0; releasing the global host services (`g_linux_process_launcher`, `g_linux_audio`, `g_linux_presentation`) in that order inside `main` ended with 0. Until now they were released during static destruction, when static state of other translation units may already be gone, and they are **inferred** to touch it while releasing; 1st leaves a guest sound thread blocked when the process ends. Which static object it is stays **unresolved**: under gdb the run clashes with the guest runtime's SIGTRAP, and apport left no core dump.*

## 결정 / Decisions

- `main`의 `LinuxWindowHold`를 `LinuxHostLifetime`으로 바꾼다. 소멸자는 다음 순서로 일한다.
  1. `--hold-window`면 창을 닫을 때까지 둔다.
  2. launcher, 오디오, 창 순서로 host 서비스를 해제한다.
- 이것은 `main`이 어떻게 돌아오든 정적 소멸보다 먼저 일어난다. Windows 경로는 바뀌지 않는다.

*`main`'s `LinuxWindowHold` becomes `LinuxHostLifetime`, whose destructor keeps the window until closed under `--hold-window`, then releases the host services in the order launcher, audio, window, ahead of static destruction however `main` returns. The Windows path is unchanged.*

## 검증 / Verification

- Linux x64 build와 CTest(경고를 오류로).
  *The Linux x64 build and CTest with warnings as errors.*
- 작업 439의 종료 경로 재현에서 host 종료 상태가 SIGSEGV(-11)에서 0이 되는지.
  *In task 439's exit-path reproduction, the host exit status goes from SIGSEGV (-11) to 0.*
