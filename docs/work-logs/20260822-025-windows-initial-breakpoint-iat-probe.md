# Windows Initial-Breakpoint IAT Probe Result

## 한국어

`DEBUG_ONLY_THIS_PROCESS`로 원본 `ez2dj1.exe`를 시작해 create/load-DLL event를 계속 처리하고 첫 `EXCEPTION_BREAKPOINT`에서 멈췄습니다. 그러나 IAT 첫 slot은 `CREATE_SUSPENDED` 결과와 동일하게 아직 결합되지 않았습니다.

```text
KERNEL32.dll, IAT RVA 0x01aba354, value 0x01aba6ea
```

따라서 첫 debugger breakpoint는 이 환경에서 import binding 이후 entry 이전이라는 가정에 맞지 않습니다. guest entry는 continue하지 않았고, IAT·메모리는 읽기만 했으며 child를 종료했습니다.

다음 후보는 첫 breakpoint에서 primary WOW64 thread에 원본 entry address의 hardware execution breakpoint를 설정하는 방식입니다. loader를 계속 실행하되 entry instruction이 실행되기 직전 `EXCEPTION_SINGLE_STEP`으로 정지해 IAT를 다시 검증합니다.

## English

The original `ez2dj1.exe` was started with `DEBUG_ONLY_THIS_PROCESS`; create/load-DLL events were continued and the first `EXCEPTION_BREAKPOINT` was held. Its first IAT slot, however, remained unbound just as it did with `CREATE_SUSPENDED`.

```text
KERNEL32.dll, IAT RVA 0x01aba354, value 0x01aba6ea
```

The first debugger breakpoint therefore does not meet the assumption of being after import binding and before entry in this environment. Guest entry was never continued; only IAT/memory reads occurred and the child was terminated.

The next candidate sets a hardware execution breakpoint at the original entry address on the primary WOW64 thread at the first breakpoint. It continues loader execution, then stops on `EXCEPTION_SINGLE_STEP` immediately before the entry instruction and verifies the IAT again.
