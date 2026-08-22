# Windows x86 child runtime DLL 주입 결과

## 결과

기본 `windows-x86-debug` preset에서도 `re2dj_windows_injected_runtime.dll`을 빌드하도록 변경했습니다. `re2dj_windows_x86_launcher_probe --inject-runtime`은 기본적으로 probe executable과 같은 출력 디렉터리의 DLL을 찾습니다.

실제 `roms\ez2dj1stse` 입력에서 entry `INT3` 정지 후 primary thread를 suspend하고 debug event를 계속한 뒤, same-bitness x86 `kernel32!LoadLibraryW` 주소로 remote thread를 만들었습니다. runtime DLL은 `0x7c130000` module base를 반환했습니다. primary thread는 suspend 상태이고 probe는 child를 종료하므로 original entry는 실행하지 않았습니다.

검증:

1. `cmake --preset windows-x86-debug -DRE2DJ_WARNINGS_AS_ERRORS=ON` 성공
2. `cmake --build --preset windows-x86-debug --target re2dj_windows_x86_launcher_probe re2dj_windows_injected_runtime` 성공
3. `re2dj_windows_x86_launcher_probe --hdd .\roms\ez2dj1stse --software-breakpoint --inject-runtime` 성공
4. `ctest --preset windows-x86-debug` 1/1 통과

이 결과는 동일 x86 환경의 injection feasibility만 확인합니다. `LoadLibraryW` 주소가 다른 모든 process 조합에서 유효하다는 일반 결론은 아닙니다. 다음 작업은 runtime이 원본 IAT를 교체하고 첫 HLE import를 관찰하는 handoff입니다.

## English

The default `windows-x86-debug` preset now also builds `re2dj_windows_injected_runtime.dll`. With `--inject-runtime`, `re2dj_windows_x86_launcher_probe` finds the DLL beside the probe executable by default.

With live `roms\ez2dj1stse` input, after the entry `INT3` stop the launcher suspended the primary thread, continued the debug event, and created a remote thread at the same-bitness x86 `kernel32!LoadLibraryW` address. The runtime DLL returned module base `0x7c130000`. The primary thread remained suspended and the probe terminated the child, so original entry did not execute.

Verification:

1. `cmake --preset windows-x86-debug -DRE2DJ_WARNINGS_AS_ERRORS=ON` succeeded.
2. `cmake --build --preset windows-x86-debug --target re2dj_windows_x86_launcher_probe re2dj_windows_injected_runtime` succeeded.
3. `re2dj_windows_x86_launcher_probe --hdd .\roms\ez2dj1stse --software-breakpoint --inject-runtime` succeeded.
4. `ctest --preset windows-x86-debug` passed 1/1.

This result confirms injection feasibility only for the same x86 environment. It does not establish that the `LoadLibraryW` address is valid for every process combination. The next task is runtime IAT replacement and observation of the first HLE import handoff.
