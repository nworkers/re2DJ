# ez2dj3rd Hardlock 실행 작업 로그

## 결과 요약

`re2dj ez2dj3rd` 명령으로 저장소 root의 `roms/ez2dj3rd/ez2dj/EZ2DJ.EXE`를 실제 실행하는 경로를 확인했습니다. 내장 프로파일이 HDD 경로, `ez2dj` 작업 디렉터리, VFS, DirectSound, detached 실행을 구성하고 원본 프로세스를 응답 상태로 유지했습니다.

실행은 게임 화면 전에 원본 프로세스가 만든 `Hardlock` 대화상자와 `Error 1009 : Cannot open Hardlock driver.`에서 멈췄습니다. 따라서 이번 작업의 실행 결과는 Hardlock 실패 경계까지이며, 보호 응답을 추측해 게임 화면을 성공으로 기록하지 않았습니다.

*The command `re2dj ez2dj3rd` was verified against the original `roms/ez2dj3rd/ez2dj/EZ2DJ.EXE` under the repository root. The built-in profile configured the HDD path, `ez2dj` working directory, VFS, DirectSound, and detached execution, and the original process remained responsive.*

*Before a game screen appeared, the original process created a `Hardlock` dialog containing `Error 1009 : Cannot open Hardlock driver.` This task therefore completes at the Hardlock failure boundary and does not claim game-screen success by guessing a protection response.*

## 실행 증거

- branch: `task-097-music-select-z-order`
- product command: `build/windows-x86/bin/Debug/re2dj.exe ez2dj3rd`
- verification log: `logs/windows_x86_launcher_probe/ez2dj3rd/20260830-211620-710.jsonl`
- log evidence: target `ez2dj3rd`, executable `roms/ez2dj3rd/ez2dj/EZ2DJ.EXE`, `hle_vfs:true`, `hle_directsound:true`, `run_detached:true`, `runtime_detached`
- window evidence: owned original process window class `#32770`, title `Hardlock`; dialog text `Error 1009 : Cannot open Hardlock driver.`

*Evidence: branch `task-097-music-select-z-order`; product command `build/windows-x86/bin/Debug/re2dj.exe ez2dj3rd`; verification log `logs/windows_x86_launcher_probe/ez2dj3rd/20260830-211620-710.jsonl`; the log records target `ez2dj3rd`, the original executable path, VFS and DirectSound enabled, detached execution, and `runtime_detached`; the owned original process created a `#32770` window titled `Hardlock` with the stated error text.*

## 구현 내용

- `injected_runtime.cpp`에 `\\.\\` Win32 장치 경로를 위한 별도 bounded trace를 추가했습니다. API, 요청 경로, 성공 여부, 오류 코드를 최대 128건 기록합니다.
- VFS runtime probe가 설정되지 않은 `\\.\\Hardlock` 요청의 `ERROR_INVALID_NAME` 결과와 trace 기록을 검사하도록 추가했습니다.
- 3rd 프로파일에는 1st SE LPTDI mock, raw I/O, target-state 응답을 연결하지 않았습니다. 3rd 정적 import에는 `DeviceIoControl`, `LPTDI`, `TDSD.VXD`가 확인되지 않았고, 실제 Hardlock 요청이 현재 VFS thunk를 통과하는 증거도 없습니다.
- 원본 HDD, 원본 EXE, 게임 데이터는 수정하거나 저장소에 추가하지 않았습니다.

*Implementation: `injected_runtime.cpp` now has a separate bounded trace for `\\.\\` Win32 device paths, recording API, requested path, success, and error code for at most 128 events. The VFS runtime probe checks the rejected `\\.\\Hardlock` result and trace. The 3rd profile does not receive the 1st SE LPTDI mock, raw-I/O policy, or target-state response. Static 3rd imports did not confirm `DeviceIoControl`, `LPTDI`, or `TDSD.VXD`, and the product run did not prove that the real Hardlock request passes through the current VFS thunk. No original HDD, executable, or game data was modified or added to the repository.*

## 검증

- Windows x86 Debug build: 통과

  `cmake --build --preset windows-x86-debug --config Debug --target re2dj re2dj_windows_x86_launcher_probe re2dj_windows_vfs_runtime_probe re2dj_unit_tests`

- VFS runtime probe: 통과
- unit test: `checks: 981, failures: 0`
- 제품 shortcut 실행: 프로파일 선택, 원본 EXE 실행, runtime 주입, VFS mount, DirectSound hook, detached 상태와 Hardlock 경계를 확인
- 실행 후 이번 작업에서 시작한 원본 `EZ2DJ.EXE`와 launcher 프로세스는 종료했습니다.

*Verification: the Windows x86 Debug build passed with the command shown above; the VFS runtime probe passed; unit tests reported `checks: 981, failures: 0`; the product shortcut confirmed profile selection, original-EXE execution, runtime injection, VFS mount, DirectSound hook, detached state, and the Hardlock boundary; the original and launcher processes started for this run were terminated afterward.*

## 관련 문서

- 설계: `docs/design/20260830-101-ez2dj3rd-hardlock-execution.md`
- 작업 지시: `docs/work-orders/20260830-101-ez2dj3rd-hardlock-execution.md`
- 누적 분석: `docs/analysis/ez2dj-exe-structures.md`

*Related documents: design `docs/design/20260830-101-ez2dj3rd-hardlock-execution.md`; work order `docs/work-orders/20260830-101-ez2dj3rd-hardlock-execution.md`; cumulative analysis `docs/analysis/ez2dj-exe-structures.md`.*
