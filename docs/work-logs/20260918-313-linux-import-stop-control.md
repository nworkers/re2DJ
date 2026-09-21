# 작업 로그 313 — Linux import 중지 제어 계약

## 결과

Linux `NativeHelperBackend`는 pending import completion의 `kStop`을 helper에 보내지 않습니다. 대신 pipe를 닫고 i386 helper를 종료한 뒤 내부 상태를 `Stopped`로 바꿉니다. 따라서 helper가 EAX·EDX·stack cleanup을 적용해 guest로 복귀하는 continuation은 `kContinue`에만 남습니다.

Linux `native_ipc_host_probe`에 `--stop` fixture를 추가했습니다. 이 fixture는 합성 PE32가 첫 import gate에 도달한 뒤 `kStop` completion을 요청하고, backend가 즉시 후속 `WaitForEvent()`를 거부하는지 확인합니다. 이는 host observer와 미구현 import 정책이 guest를 계속 실행하지 않게 하는 제어 경계입니다.

`kStop`은 guest `ExitProcess`의 대체가 아닙니다. guest 종료 code를 `kProcessExit` event로 전달하는 HLE handler와 process service는 실제 import 호출 근거에 따라 별도 작업에서 추가합니다.

*Result*

*The Linux `NativeHelperBackend` no longer sends pending-import `kStop` to the helper. Instead it closes pipes, terminates the i386 helper, and changes internal state to `Stopped`. Only `kContinue` remains a continuation through which the helper applies EAX, EDX, and stack cleanup before returning to guest code.*

*The Linux `native_ipc_host_probe` now has a `--stop` fixture. After a synthetic PE32 reaches its first import gate, the fixture requests `kStop` and verifies that the backend immediately rejects a subsequent `WaitForEvent()`. This is the control boundary that prevents a host observer or unimplemented-import policy from continuing guest execution.*

*`kStop` is not a replacement for guest `ExitProcess`. An HLE handler and process service that report a guest exit code through a `kProcessExit` event will be added separately from actual import-call evidence.*

## 검증

- WSL2 Ubuntu 24.04.1에서 `bash scripts/test_linux_native_helper_probe.sh` 실행
- Linux x64 Debug CTest 1/1 통과, continue probe가 `result=51`, `child=0`, fault signal 4를 보고
- 같은 x64 host에서 `linux-native-stop-probe: terminal pending-import stop` 확인
- Linux x86 Debug CTest 1/1 통과, x86 continue probe가 `result=51`, `child=0`, fault signal 4를 보고
- 같은 x86 host에서 `linux-native-stop-probe: terminal pending-import stop` 확인
- i386 helper가 ELF32로 build됨을 확인
- `git diff --check` 통과

*Verification*

*Ran `bash scripts/test_linux_native_helper_probe.sh` on WSL2 Ubuntu 24.04.1. Linux x64 Debug CTest passed 1/1; its continue probe reported `result=51`, `child=0`, and fault signal 4, while its stop probe reported `linux-native-stop-probe: terminal pending-import stop`. Linux x86 Debug CTest also passed 1/1 with the same continue and stop outcomes. The i386 helper built as ELF32, and `git diff --check` passed.*

## 다음 단계

실제 `kernel32` binding은 정적 import 표면과 실행 관찰이 함께 있는 API부터 추가합니다. 그 전에 target별 Win32 version persona, guest process/thread identity, module/handle registry와 guest memory allocation 정책을 설계해야 합니다.

*Next step*

*Actual `kernel32` bindings begin only with APIs supported by both static import-surface and execution observations. Before that, target-specific Win32 version persona, guest process/thread identity, module/handle registry, and guest-memory allocation policy require design.*
