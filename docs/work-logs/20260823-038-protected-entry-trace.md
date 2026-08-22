# Protected entry debugger trace 결과

## 결과

`--trace`에 DLL load/unload base와 path, thread/process exit code, exception code와 address를 추가했습니다. 이 진단은 관찰만 하며 child memory 또는 IAT를 바꾸지 않습니다.

hardware breakpoint trace는 `umpdc.dll`, `wsock32.dll`, `ws2_32.dll` 적재, Winsock DLL 두 개의 해제, 세 thread의 code 0 종료, process exit `0x001affcc` 순서를 보였습니다. debugger 없는 직접 실행도 같은 code로 즉시 종료했습니다. 그러나 software breakpoint는 static entry VA `0x01ed23cf`에 도달하므로, 이 trace는 pre-entry 종료가 아니라 hardware breakpoint가 전달되지 않은 상태에서 entry 이후 실행을 놓친 기록입니다. debugger 감지 단독 원인은 배제됐지만, 보호 검증 실패와 현대 Windows 호환성 실패 중 어느 것인지는 아직 확인되지 않았습니다.

runtime 없이 static entry 뒤 native `ExitProcess`에 software breakpoint를 두고, observation breakpoint가 아닌 exception을 guest에 넘겼습니다. Winsock DLL 해제 다음 `MEM_PRIVATE | PAGE_READWRITE` page의 실행마다 달라지는 주소(예: `0x00257004`)에서 `0xc000001d` illegal instruction이 발생했습니다. 첫 16 byte가 `ff ff ff ff 00 00 40 00 ...`인 data page였고, 예외 stack에서는 guest caller를 식별하지 못했습니다. `ExitProcess` breakpoint는 발생하지 않았으며, 세 thread와 process는 `0xc000001d`로 종료했습니다.

## English

`--trace` now reports DLL load/unload bases and paths, thread/process exit codes, and exception codes and addresses. The diagnostic is observational and does not alter child memory or IAT entries.

The hardware-breakpoint trace showed `umpdc.dll`, `wsock32.dll`, and `ws2_32.dll` loading; the two Winsock DLLs unloading; three threads exiting with code zero; and process exit `0x001affcc`. A direct run without a debugger also exited immediately with the same code. However, a software breakpoint reaches static-entry VA `0x01ed23cf`, so this trace missed post-entry execution while the hardware breakpoint was not delivered rather than proving a pre-entry exit. This excludes debugger detection as the sole cause, but does not yet distinguish a protection-check failure from a modern-Windows compatibility failure.

Without runtime injection, a software breakpoint was placed on native `ExitProcess` after static entry, while exceptions other than the observation breakpoint were passed to the guest. After Winsock DLL unload, an illegal instruction `0xc000001d` occurred at a run-varying address (for example, `0x00257004`) on a `MEM_PRIVATE | PAGE_READWRITE` page. Its first 16 bytes were `ff ff ff ff 00 00 40 00 ...`, a data page, and the exception stack did not identify a guest caller. The `ExitProcess` breakpoint never fired; three threads and the process exited with `0xc000001d`.
