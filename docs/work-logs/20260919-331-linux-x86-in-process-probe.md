# 작업 로그 331: Linux x86 in-process runner probe / Work log 331: Linux x86 in-process runner probe

## 결과 / Result

`re2dj_linux_native_in_process_probe`를 Linux x86 product build에 추가했습니다. 기존 native IPC fixture의 relocatable PE generator를 같은 translation unit에서 재사용하지만, 실행은 `NativeInProcessRunner`와 동기 handler만 사용하며 pipe나 helper process를 만들지 않습니다.

*Added `re2dj_linux_native_in_process_probe` to the Linux x86 product build. It reuses the relocatable PE generator from the existing native IPC fixture in the same translation unit, but execution uses only `NativeInProcessRunner` and a synchronous handler, creating no pipe or helper process.*

handler는 event stack start 뒤의 첫 argument를 확인합니다. 두 import는 차례로 `41 -> EAX 42`, `42 -> EAX 43·EDX 1`을 반환하고 각각 4 bytes를 정리합니다. TLS callback state와 guest entry 계산을 포함한 exit code는 51입니다. entry 첫 instruction을 `UD2`로 바꾼 fixture는 `NativeGuestFault`의 SIGILL과 expected entry EIP를 보고합니다.

*The handler checks the first argument after event stack start. The two imports return `41 -> EAX 42`, then `42 -> EAX 43·EDX 1`, each cleaning four bytes. Including TLS callback state and guest entry calculation, exit code is 51. A fixture with the entry's first instruction changed to `UD2` reports SIGILL and the expected entry EIP through `NativeGuestFault`.*

## 검증 / Validation

WSL Ubuntu 24.04에서 `linux-x86-debug` preset과 `RE2DJ_WARNINGS_AS_ERRORS=ON`으로 probe를 빌드하고 실행했습니다.

*Built and ran the probe on WSL Ubuntu 24.04 with the `linux-x86-debug` preset and `RE2DJ_WARNINGS_AS_ERRORS=ON`.*

```text
linux-native-in-process-probe: imports=2 exit=51 signal=4
```

이 결과는 Linux x86에서 direct PE session·import bridge·handler·TLS·fault boundary가 한 process에서 동작함을 보입니다. 실제 `GetModuleHandleA` binding과 product CLI 선택은 후속 작업입니다.

*This result shows that direct PE session, import bridge, handler, TLS, and fault boundaries work in one process on Linux x86. Actual `GetModuleHandleA` binding and product CLI selection are later work.*
