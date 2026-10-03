# 작업 443 작업 지시서 — 자식 run은 실행 중인 그 실행 파일로 / Task 443 work order — a child run from the very executable running

설계: [20261003-443-linux-child-self-exe.md](../design/20261003-443-linux-child-self-exe.md)

## 절차 / Steps

1. 실행 파일 교체로 자식 시작이 실패하는 것을 재현한다.
   *Reproduce the child start failing once the executable is replaced.*
2. `LinuxHostProcessLauncher::Start`가 `/proc/self/exe`를 실행하게 한다.
   *Make `LinuxHostProcessLauncher::Start` run `/proc/self/exe`.*
3. 재현 확인, Linux x64 build와 CTest, 작업 로그.
   *Recheck the reproduction, the Linux x64 build and CTest, and the work log.*

## 완료 조건 / Done when

재현에서 6th 자식이 뜨고, build와 CTest가 통과한다.

*The 6th child starts in the reproduction, and the build and CTest pass.*
