# Windows x86 원본 프로세스 entry launcher 결과

## 결과

Win32 x86 기본 preset에 `re2dj_windows_x86_launcher_probe`를 추가했습니다. 이 도구는 원본 `ez2dj1.exe`를 Windows loader의 child main image로 생성하고, entry 직전에서 IAT 상태를 읽기 전용으로 검사합니다.

실제 `roms\ez2dj1stse` 입력에서 x86 DR0 hardware breakpoint는 context read-back에는 보존됐지만 entry single-step을 전달하지 않았고, child는 loader event 뒤 exit code `-1`로 종료했습니다. 따라서 진단용 `--software-breakpoint` fallback을 추가했습니다. fallback은 child memory의 `0x0043a640` 첫 바이트만 `INT3`로 바꾸고 정지 즉시 원래 바이트를 복원합니다. 원본 파일과 HDD는 변경하지 않습니다.

fallback 검증 출력은 다음 결과를 확인했습니다.

* image base와 child main module base: `0x00400000`
* entry: `0x0043a640`
* loader-resolved IAT: 7 DLL, 144 slot
* original entry 실행: 하지 않음

검증:

1. `cmake --preset windows-x86-debug -DRE2DJ_WARNINGS_AS_ERRORS=ON` 성공
2. `cmake --build --preset windows-x86-debug --target re2dj_windows_x86_launcher_probe` 성공
3. `re2dj_windows_x86_launcher_probe --hdd .\roms\ez2dj1stse --software-breakpoint` 성공
4. `ctest --preset windows-x86-debug` 1/1 통과

다음 작업은 동일 x86 child에 runtime DLL을 주입하고, entry 정지 상태에서 IAT handoff를 검증하는 것입니다. DR0 hardware breakpoint의 실패 원인은 runtime handoff와 분리해 후속 조사합니다.

## English

The primary Win32 x86 preset now includes `re2dj_windows_x86_launcher_probe`. It creates original `ez2dj1.exe` as a Windows-loader-owned child main image and inspects the IAT read-only immediately before entry.

With live `roms\ez2dj1stse` input, the x86 DR0 hardware breakpoint was retained by context read-back but did not deliver an entry single-step; the child exited with code `-1` after loader events. A diagnostic `--software-breakpoint` fallback was therefore added. It replaces only the first byte at child-memory `0x0043a640` with `INT3` and restores the original byte immediately on stop. It does not modify the original file or HDD.

The fallback confirmed:

* image base and child main-module base: `0x00400000`
* entry: `0x0043a640`
* loader-resolved IAT: seven DLLs and 144 slots
* original entry execution: not performed

Verification:

1. `cmake --preset windows-x86-debug -DRE2DJ_WARNINGS_AS_ERRORS=ON` succeeded.
2. `cmake --build --preset windows-x86-debug --target re2dj_windows_x86_launcher_probe` succeeded.
3. `re2dj_windows_x86_launcher_probe --hdd .\roms\ez2dj1stse --software-breakpoint` succeeded.
4. `ctest --preset windows-x86-debug` passed 1/1.

The next task injects the runtime DLL into the same x86 child and verifies IAT handoff while stopped at entry. The DR0 hardware-breakpoint issue is isolated for later investigation.
