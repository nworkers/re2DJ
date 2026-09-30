# 작업 431 작업 로그 — Linux 6th와 자식 프로세스 / Task 431 work log — Linux 6th and child processes

설계: [20260930-431-linux-6th-child-process.md](../design/20260930-431-linux-6th-child-process.md) · 지시서: [20260930-431-linux-6th-child-process.md](../work-orders/20260930-431-linux-6th-child-process.md)

## 2026-09-30

- **처음 상태**(Linux x64)
  - launcher가 78번째 호출인 `user32!GetKeyState`에서 멈췄다.
  - 새 `--guest-executable EZ2DJ/EZ2DJ6th.EXE`로 자식을 직접 실행하자, 151번째 호출에서 "This program only allow to run from Launcher."를 띄우고 `ExitProcess(-1)`로 끝났다.

  *Starting point (Linux x64):*
  - *The launcher stopped at call 78, `user32!GetKeyState`.*
  - *Run directly with the new `--guest-executable EZ2DJ/EZ2DJ6th.EXE`, the child showed "This program only allow to run from Launcher." at call 151 and ended with `ExitProcess(-1)`.*
- **원본 분석**
  - launcher의 반복(`0x401070`)과 문자열(`.\EZ2DJ6TH.EXE`, `.\EZ2DJ1ST\EZ2DJ.EXE`)을 확인했다.
  - `STARTUPINFO` 구성(`cbReserved2` 0x115c, `lpReserved2`+4에 "261")을 확인했다.
  - 자식의 확인 코드(`0x401510`)도 확인했다.

  *The original:*
  - *The launcher's loop (`0x401070`) and its strings (`.\EZ2DJ6TH.EXE`, `.\EZ2DJ1ST\EZ2DJ.EXE`).*
  - *How it builds `STARTUPINFO` (`cbReserved2` 0x115c, "261" at `lpReserved2`+4).*
  - *The child's check (`0x401510`).*
- **측정**(Windows 11): 결과는 설계와 같다.
  - `cp431.exe`: `CreateProcessA`와 자식 쪽
  - `ks431.exe`: `GetKeyState`
  - `fp431.exe`: `GetFullPathNameA`
  - `dib32.exe`: 32비트 DIB

  첫 `cp431` 실행은 자식 인자가 없는 경우에 probe 자신이 다시 실행되어 재귀했다. 만들어진 폴더와 프로세스를 정리하고, 필요한 측정(launcher와 같은 호출)만 결과로 썼다.

  *Measurements (Windows 11), with results as in the design:*
  - *`cp431.exe`: `CreateProcessA` and the child side;*
  - *`ks431.exe`: `GetKeyState`;*
  - *`fp431.exe`: `GetFullPathNameA`;*
  - *`dib32.exe`: 32-bit DIBs.*

  *The first `cp431` run recursed: in the cases without the child argument the probe ran itself again. The folders and processes it made were cleaned up, and only the needed measurement (the call as the launcher makes it) was used.*
- **차례로 만난 경계**(Linux x64)

  | 순서 / Order | 멈춘 곳 / Stop | 처리 / Fix |
  | --- | --- | --- |
  | 1 | launcher의 `GetKeyState` | `GetKeyState` |
  | 2 | launcher의 `CreateProcessA`("needs a host that starts processes") | 기록용 서비스 wrapper(`RecordingImportCallServices`)가 `ProcessLauncher`를 넘기지 않았음 → 전달 |
  | 3 | 자식 11,332번째 호출 `GetFullPathNameA` | `GetFullPathNameA` |
  | 4 | 자식 1,162,013번째 호출 `StretchDIBits`(32비트 `BI_RGB` DIB) | 32비트 DIB. 멈춘 호출의 사유가 API 로그 한도 밖이라 보이지 않아, 멈춘 호출은 한도 밖에서도 기록하게 함 |

  *Boundaries met in turn (Linux x64):*
  1. *the launcher's `GetKeyState`: implemented;*
  2. *the launcher's `CreateProcessA` ("needs a host that starts processes"): the recording services wrapper (`RecordingImportCallServices`) did not pass `ProcessLauncher` on, and now does;*
  3. *the child's call 11,332, `GetFullPathNameA`: implemented;*
  4. *the child's call 1,162,013, `StretchDIBits` (a 32-bit `BI_RGB` DIB): 32-bit DIBs added. The stop's reason lay past the API log's limit and could not be seen, so the call a run stops on is now recorded past the limit.*
- **결과**
  - Linux x64: 150초 시간 제한까지 멈추지 않았다(자식 2,990,880호출). 타이틀("6th TRAX SELF EVOLUTION")이 나온다. 코인 4번 뒤 1P 시작으로 모드 선택에 들어가며, 모드별 3D 그림(작업 430)이 나온다. Light를 고르면 Ruby Mix 소개 화면으로 넘어간다. 창을 닫으면 자식이 0을 보내고 launcher가 `ExitProcess(0)`으로 끝난다.
  - Linux x86: 100초 시간 제한까지 멈추지 않았다(자식 1,015,271호출). 모드 선택 화면이 나온다.
  - 첫 코인 키는 창에 초점이 옮겨지는 동안 빠지는 것으로 보인다(3번 중 2번만 들어감). 한 번 더 넣으면 된다.
  - 성능: x64 Debug는 타이틀에서 약 40 FPS, x86 Debug는 모드 선택 전환 중 4.5 FPS까지 떨어진다(TODO).

  *Results:*
  - *Linux x64: no stop before the 150-second timeout (2,990,880 child calls). The title ("6th TRAX SELF EVOLUTION") shows. After four coins, 1P start enters mode select with the per-mode 3D pictures (task 430), and choosing Light goes on to the Ruby Mix introduction. Closing the window makes the child report 0, and the launcher ends with `ExitProcess(0)`.*
  - *Linux x86: no stop before the 100-second timeout (1,015,271 child calls), and mode select shows.*
  - *The first coin key seems to be lost while focus moves to the window (only 2 of 3 went in); one more coin makes up for it.*
  - *Performance: about 40 FPS on the title with x64 Debug; x86 Debug drops to 4.5 FPS during the mode-select transition (TODO).*
- **테스트 결과**(실패 0)
  - Linux x64·x86: CTest 4개 통과, 단위 5,770 checks.
  - Windows x86: CTest 6개 통과, 단위 5,773 checks.
  - 새 검사: `CheckChildProcesses`, `CheckLaunchedStartup`, `CheckFullPathName`(`guest_files_test.cpp`), `GetKeyState`(`user32_module_test.cpp`), 32비트 DIB(`ddraw_module_test.cpp`).

  *Test results (no failures):*
  - *Linux x64 and x86: all 4 CTest tests pass, 5,770 unit checks.*
  - *Windows x86: all 6 CTest tests pass, 5,773 unit checks.*
  - *New checks: `CheckChildProcesses`, `CheckLaunchedStartup`, and `CheckFullPathName` (`guest_files_test.cpp`), `GetKeyState` (`user32_module_test.cpp`), and 32-bit DIBs (`ddraw_module_test.cpp`).*
