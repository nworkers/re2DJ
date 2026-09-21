# 작업 로그 323: Linux probe persistent-write 기대값 / Work log 323: Linux probe persistent-write expectation

## 결과 / Result

`native_ipc_host_probe`의 protected-page fixture는 `{9, 8, 7, 6}`을 초기화한 뒤 first page에 `{5}`를 성공적으로 씁니다. 기존 코드는 다음 import에서 초기값을 다시 기대했기 때문에 helper transport가 올바르게 persistent mapping을 유지해도 probe가 실패했습니다. 다음 import의 readback 기대값을 `{5, 8, 7, 6}`으로 분리하여 실제 허용된 변경을 검증하도록 수정했습니다.

*The `native_ipc_host_probe` protected-page fixture initializes `{9, 8, 7, 6}` and successfully writes `{5}` to the first page. The previous code expected the initial value again at the next import, so the probe failed even when helper transport correctly retained the persistent mapping. The next-import readback expectation is now separately `{5, 8, 7, 6}`, validating the actual allowed change.*

helper protocol, dynamic-allocation registry, page access rule, product runner에는 변경이 없습니다.

*There is no change to helper protocol, dynamic-allocation registry, page-access rule, or product runner.*

## 검증 / Validation

WSL Ubuntu 24.04에서 `bash scripts/test_linux_native_helper_probe.sh`를 실행했습니다.

*Ran `bash scripts/test_linux_native_helper_probe.sh` on WSL Ubuntu 24.04.*

- Linux x64 CTest 1/1, ELF64 host + ELF32 helper, normal probe `result=51 child=0`: 통과.
- Linux x64 fault (`SIGILL`), terminal pending-import stop, pre-`LoadImage` capability rejection: 통과.
- Linux x86 CTest 1/1, ELF32 host + 같은 ELF32 helper, normal probe `result=51 child=0`: 통과.
- Linux x86 fault, terminal pending-import stop, capability rejection: 통과.

*Linux x64 CTest 1/1, ELF64 host plus ELF32 helper, and the normal `result=51 child=0` probe passed. Linux x64 fault (`SIGILL`), terminal pending-import stop, and pre-`LoadImage` capability rejection passed. Linux x86 CTest 1/1, ELF32 host plus the same ELF32 helper, and the normal `result=51 child=0` probe passed. Linux x86 fault, terminal pending-import stop, and capability rejection also passed.*
