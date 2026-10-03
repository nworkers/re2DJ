# 작업 440 작업 지시서 — Linux run의 종료 순서 / Task 440 work order — the Linux run's teardown order

설계: [20261003-440-linux-exit-teardown.md](../design/20261003-440-linux-exit-teardown.md)

## 절차 / Steps

1. 종료 뒤 SIGSEGV를 재현하고 원인을 좁힌다(오디오 driver, `_exit`, 명시적 해제).
   *Reproduce the SIGSEGV after exit and narrow it down (audio driver, `_exit`, explicit release).*
2. `LinuxHostLifetime`으로 host 서비스를 `main` 안에서 해제한다.
   *Release the host services inside `main` through `LinuxHostLifetime`.*
3. Linux x64 build와 CTest, 재현 실행, 작업 로그.
   *The Linux x64 build and CTest, the reproduction run, and the work log.*

## 완료 조건 / Done when

- build와 CTest가 통과하고, 재현 실행이 0으로 끝난다.
  *The build and CTest pass and the reproduction run ends with 0.*
