# 작업 451 작업 지시서 — `guest_memory.py`의 Windows in-process 대응 / Task 451 work order — `guest_memory.py` on the Windows in-process runner

설계: [20261004-451-guest-memory-windows-in-process.md](../design/20261004-451-guest-memory-windows-in-process.md)

## 절차 / Steps

1. `guest_memory.py`의 Windows 열거를 `ctypes`(Toolhelp32 스냅숏, `NtQueryInformationProcess` 60, `CommandLineToArgvW`, `GetProcessTimes`)로 바꾸고, 선택 규칙을 Linux와 같게(자기·조상 제외, `--launch` 트리, 최신 생성) 한다. `tasklist` 호출을 지운다.
   *Replace the Windows enumeration with `ctypes` (a Toolhelp32 snapshot, `NtQueryInformationProcess` class 60, `CommandLineToArgvW`, `GetProcessTimes`) and give it Linux's selection rule (self and ancestors skipped, the `--launch` tree, newest creation); remove the `tasklist` call.*
2. `--launch`: Windows는 명령 문자열을 그대로 넘기고, 끝낼 때 `taskkill /T /F /PID`로 트리를 끝낸다.
   *`--launch`: Windows passes the command string as is and ends the tree with `taskkill /T /F /PID`.*
3. 모듈 docstring과 `--process` 도움말을 두 OS 공통 설명으로 고친다.
   *Rewrite the module docstring and the `--process` help for both OSes.*
4. `SKILL.md`: 도구 표, 7단계의 Windows 문단과 예시, 8단계의 `write` 예시를 `re2dj.exe` 명령줄 기준으로 고친다.
   *`SKILL.md`: the tool table, step 7's Windows paragraph and example, and step 8's `write` example, in terms of the `re2dj.exe` command line.*
5. 검증: 설계의 검증 절을 따른다. Windows 빌드 환경(VS 2026 Build Tools, CMake, Python)을 이 PC에 새로 갖춘다(사용자 결정, 2026-10-04). 작업 로그.
   *Verification per the design's section; the Windows build environment (VS 2026 Build Tools, CMake, Python) is set up on this PC first (the user's decision, 2026-10-04); the work log.*

## 완료 조건 / Done when

Windows에서 같은 명령줄의 `re2dj.exe` 둘 중 둘째(게스트 쪽)를 고르고 읽기·폴링·쓰기가 되며, Linux 동작은 그대로다.

*On Windows the script picks the second (guest) of the two same-command-line `re2dj.exe` processes and reads, polls and writes it, and Linux behaves as before.*
