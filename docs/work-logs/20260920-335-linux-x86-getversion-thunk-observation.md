# 작업 로그 335: Linux x86 GetVersion thunk 연속 실행 진단 / Work log 335: Linux x86 GetVersion thunk continuation diagnostic

## 결과 / Result

Linux x86 전용 `NativeDynamicThunk`를 추가했습니다. 이 thunk는 기존 import thunk와 같은 bridge ABI를 사용해 dynamic gate를 전달합니다. `--linux-in-process-getversion-call`은 `GetProcAddress(kernel32, "GetVersion")`에 이 thunk를 반환한 뒤, thunk 도달과 호출 전 중단을 별도 `OriginalRunBoundary` 값으로 보고합니다.

*Added Linux x86-only `NativeDynamicThunk`. It uses the same bridge ABI as existing import thunks and forwards a dynamic gate. After returning this thunk for `GetProcAddress(kernel32, "GetVersion")`, `--linux-in-process-getversion-call` reports thunk arrival and a pre-call stop as separate `OriginalRunBoundary` values.*

실제 4th CHD에서 resolver는 통과했지만 dynamic gate에는 도달하지 않았습니다. 원본 실행은 thunk 호출 전에 signal 11 (`SIGSEGV`)과 EIP `0x00af0c22`에서 멈췄습니다. 그러므로 이 실행에서 `GetVersion` API 호출은 확인되지 않았습니다. 이는 원본이 이후 다른 경로에서 호출하지 않는다는 뜻은 아닙니다.

*On the real 4th CHD, the resolver completed but did not reach the dynamic gate. Original execution stopped before the thunk call with signal 11 (`SIGSEGV`) at EIP `0x00af0c22`. `GetVersion` API invocation is therefore not confirmed for this run. This does not mean the original can never call it on a later path.*

## 검증 / Validation

WSL Linux x86에서 다음을 실행했습니다.

*Ran the following in WSL Linux x86:*

```text
cmake --build --preset linux-x86-debug --target re2dj re2dj_linux_native_in_process_probe
build/linux-x86-debug/bin/re2dj ez2dj4th --run --linux-in-process-getversion-call
build/linux-x86-debug/bin/re2dj_linux_native_in_process_probe
```

```text
GetVersion thunk not reached: signal 11, EIP 0x00af0c22
linux-native-in-process-probe: imports=2 exit=51 signal=4
```
Linux x64 product build도 통과했습니다. 같은 CLI option은 i386 host가 필요하다는 오류로 종료해, x64에서 실행 가능한 x86 thunk를 제공한다는 인상을 주지 않습니다.

*The Linux x64 product build also passed. The same CLI option exits with an error that it requires an i386 host, so it does not imply that x64 provides an executable x86 thunk.*
Synthetic probe는 dynamic thunk를 직접 호출해 bridge의 고유 gate 도달도 확인했습니다. `dynamic=1`은 이 직접 호출이 handler까지 도달했음을 뜻합니다.

*The synthetic probe directly calls the dynamic thunk and confirms arrival at its unique bridge gate. `dynamic=1` means that this direct call reached the handler.*