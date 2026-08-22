# Windows x86 첫 import handoff 결과

## 결과

`GetCommandLineA` 관찰용 HLE handoff를 구현했습니다. launcher는 원본 PE import table에서 해당 IAT slot을 이름으로 찾고, runtime DLL export RVA를 파일에서 계산합니다. 원래 target은 runtime의 exported variable에 쓰고 IAT는 runtime thunk로 교체합니다.

runtime thunk는 `OUTPUT_DEBUG_STRING_EVENT`용 메시지를 낸 뒤 모든 범용 레지스터·flags를 복원하고 원래 `GetCommandLineA` 주소로 tail-jump합니다. 따라서 관찰 자체는 API 반환값과 호출자 stack cleanup을 바꾸지 않도록 설계됐습니다.

검증:

1. `cmake --preset windows-x86-debug -DRE2DJ_WARNINGS_AS_ERRORS=ON` 성공
2. `cmake --build --preset windows-x86-debug --target re2dj_windows_x86_launcher_probe re2dj_windows_injected_runtime` 성공
3. `ctest --preset windows-x86-debug` 1/1 통과

사용자 승인 뒤 실제 `--probe-handoff`를 실행했습니다. `GetCommandLineA` log-and-forward thunk의 debugger output event를 수신했고, 해당 실행의 runtime module base는 `0x7c140000`이었습니다. event 직후 child를 종료했습니다. 따라서 실제 original import가 runtime을 거쳐 원래 native API로 tail-jump하는 handoff가 확인됐습니다.

## English

An observation HLE handoff for `GetCommandLineA` is implemented. The launcher finds the corresponding IAT slot by name from the original PE import table and computes runtime DLL export RVAs from the DLL file. It writes the original target into an exported runtime variable and replaces the IAT slot with the runtime thunk.

The runtime thunk emits an `OUTPUT_DEBUG_STRING_EVENT` message, restores all general registers and flags, then tail-jumps to the original `GetCommandLineA` address. The observation is therefore designed not to change API results or caller stack cleanup.

Verification:

1. `cmake --preset windows-x86-debug -DRE2DJ_WARNINGS_AS_ERRORS=ON` succeeded.
2. `cmake --build --preset windows-x86-debug --target re2dj_windows_x86_launcher_probe re2dj_windows_injected_runtime` succeeded.
3. `ctest --preset windows-x86-debug` passed 1/1.

After user approval, live `--probe-handoff` was executed. The expected debugger output event from the `GetCommandLineA` log-and-forward thunk was received, and this run's runtime module base was `0x7c140000`. The child was terminated immediately after the event. This confirms a real original import can cross the runtime and tail-jump to the original native API.
