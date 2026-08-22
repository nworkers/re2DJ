# Windows x86 HLE GetCommandLineA 결과

## 결과

사용자 결정대로 `GetCommandLineA`의 첫 HLE 구현은 원본 command line forwarding 대신 target executable basename만 반환합니다. launcher는 `ez2dj1.exe`를 runtime DLL의 exported ANSI buffer에 쓰고, runtime HLE thunk는 해당 buffer 주소를 EAX로 반환한 뒤 `ret`합니다.

실제 `--hle-command-line` 제한 실행에서 HLE debugger output event를 수신했습니다. 이 실행의 runtime base는 `0x7c150000`이었고, event 직후 child를 종료했습니다. 원본 HDD는 변경하지 않았습니다.

검증:

1. `cmake --preset windows-x86-debug -DRE2DJ_WARNINGS_AS_ERRORS=ON` 성공
2. `cmake --build --preset windows-x86-debug --target re2dj_windows_x86_launcher_probe re2dj_windows_injected_runtime` 성공
3. `ctest --preset windows-x86-debug` 1/1 통과
4. `re2dj_windows_x86_launcher_probe --hdd .\roms\ez2dj1stse --hle-command-line` 성공

다음 HLE API는 파일·INI·경로 처리 중 하나가 되어야 합니다. 이 API들은 원본 HDD 읽기와 overlay write 정책을 실제로 요구하므로, 어떤 API를 먼저 교체할지와 guest path policy를 정해야 합니다.

## English

As decided by the user, the first HLE implementation of `GetCommandLineA` returns only the target executable basename rather than forwarding the original command line. The launcher writes `ez2dj1.exe` into an exported ANSI buffer in the runtime DLL; the runtime HLE thunk returns that buffer address in EAX and executes `ret`.

The live limited `--hle-command-line` run received the HLE debugger output event. This run's runtime base was `0x7c150000`, and the child was terminated immediately after the event. The original HDD was not modified.

Verification:

1. `cmake --preset windows-x86-debug -DRE2DJ_WARNINGS_AS_ERRORS=ON` succeeded.
2. `cmake --build --preset windows-x86-debug --target re2dj_windows_x86_launcher_probe re2dj_windows_injected_runtime` succeeded.
3. `ctest --preset windows-x86-debug` passed 1/1.
4. `re2dj_windows_x86_launcher_probe --hdd .\roms\ez2dj1stse --hle-command-line` succeeded.

The next HLE API must be one of file, INI, or path handling. Those APIs require actual original-HDD reads and the overlay-write policy, so the first replacement API and guest-path policy need to be chosen.
