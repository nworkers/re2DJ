# 작업 452 작업 지시서 — Windows x64 호환 모드 조사 / Task 452 work order — probing compatibility mode on Windows x64

설계: [20261004-452-windows-x64-compat-mode-probe.md](../design/20261004-452-windows-x64-compat-mode-probe.md)

## 절차 / Steps

1. `src/platform/windows/x64/native_compat_mode_probe.cpp`: 4 GiB 아래 코드·스택·가짜 TEB 할당, 64비트 진입·착지 trampoline과 32비트 게스트 코드(기계어 바이트, 어셈블리 주석), VEH, `rdfsbase`/`wrfsbase` 시험, 부하 스레드. Q1~Q6을 차례로 실행해 결과를 한 줄씩 출력한다.
   *The probe: code, stack and fake TEB below 4 GiB, the 64-bit entry and landing trampoline and 32-bit guest code as machine-code bytes with assembly comments, a VEH, the `rdfsbase`/`wrfsbase` checks and load threads, running Q1 to Q6 in turn with one result line each.*
2. 바이트는 WSL `as --64`/`--32`로 어셈블해 `objdump`로 옮긴다.
   *Assemble the bytes with WSL `as --64`/`--32` and copy them from `objdump`.*
3. CMake: `WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8`일 때만 probe 대상을 만들고 `/CETCOMPAT:NO`로 링크한다. `src/platform/windows/x64/README.md`를 만들고 `src/platform/windows/README.md`를 갱신한다.
   *CMake builds the probe only when `WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8`, linking with `/CETCOMPAT:NO`; add `src/platform/windows/x64/README.md` and update the Windows platform README.*
4. 검증: x64 configure 후 probe 대상만 빌드·실행, x86 Debug build. 결과를 kb `windows-x64-compatibility-mode.md`(색인 포함)와 작업 로그에 남기고 규모를 판단한다.
   *Verification: configure x64, build and run only the probe, and build x86 Debug; record the results in the kb topic `windows-x64-compatibility-mode.md` (with the index) and the work log, and size the work.*

## 완료 조건 / Done when

Q1~Q6 각각이 실행 결과로 확인되거나 확인 불가 이유가 기록되고, 설계의 기준에 따라 Windows x64 backend의 규모 판단이 작업 로그에 있다.

*Each of Q1 to Q6 is settled by a run or has the reason it could not be recorded, and the work log sizes a Windows x64 backend by the design's criteria.*
