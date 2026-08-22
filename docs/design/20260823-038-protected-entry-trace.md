# Protected entry debugger trace

## 설계

protected `ez2dj.exe`의 raw 실행이 빠르게 종료하고 hardware entry breakpoint는 전달되지 않으므로, Windows x86 launcher probe의 `--trace`는 debugger event 번호만이 아니라 보호 초기화의 관찰 근거를 남겨야 한다. trace는 load/unload DLL의 base와 파일 경로, create/exit thread, process exit code, exception code와 address를 출력한다.

trace는 관찰 전용이다. child memory, IAT, entry breakpoint, 또는 debug-event continue 상태를 바꾸지 않는다. 출력된 DLL과 종료 code는 원인 후보를 좁히는 자료일 뿐, 단일 event로 보호 기법이나 종료 원인을 확정하지 않는다.

software entry breakpoint를 사용하는 경우 각 pre-entry debugger event 뒤 static entry의 첫 byte도 읽어 기록한다. 이 byte가 `0xcc`에서 바뀌면 entry page가 entry 이전에 다시 작성된 사실을 확인할 수 있고, 계속 `0xcc`인데 breakpoint가 없으면 header entry가 아닌 제어 경로를 조사한다.

protected-entry 분석에는 `ExitProcess` 관찰 wrapper도 제공한다. launcher는 software entry stop 뒤 resolved IAT slot 하나만 이 wrapper로 바꿀 수 있다. wrapper는 요청된 exit code와 caller return address를 출력하고 곧바로 native `ExitProcess`를 호출하므로, guest의 종료를 억제하거나 바꾸지 않는다.

관찰 대상 breakpoint가 아닌 guest exception은 `DBG_EXCEPTION_NOT_HANDLED`로 계속한다. debugger가 illegal instruction 같은 예외를 처리된 것으로 넘기면 faulting instruction을 반복 실행해 원래의 SEH 또는 process-exit 경로를 왜곡할 수 있기 때문이다. trace는 이 예외의 주소와 첫 16 byte를 기록한다.

예외 trace는 `VirtualQueryEx`의 memory region 속성과 faulting thread의 ESP 위 return-address word들도 기록한다. 이를 통해 fault address가 image code, executable dynamic code, 또는 data region 중 어디에 속하는지와 guest caller 후보를 구분한다.

## English

Because protected `ez2dj.exe` exits quickly in a raw run and the hardware entry breakpoint is not delivered, the Windows x86 launcher probe's `--trace` must retain evidence from protection initialization rather than only debugger event numbers. It reports loaded and unloaded DLL bases and paths, created and exited threads, process exit codes, and exception codes and addresses.

Tracing is observational only. It does not alter child memory, IAT entries, the entry breakpoint, or debug-event continuation. The observed DLLs and exit code narrow possible causes, but no single event proves the protection method or exit cause.

When a software entry breakpoint is in use, tracing also reads and reports the first byte at the static entry after every pre-entry debugger event. A change from `0xcc` confirms that the entry page was rewritten before entry; a persistent `0xcc` with no breakpoint instead directs investigation toward a control path other than the header entry.

For the protected-entry investigation, the runtime also offers an `ExitProcess` observation wrapper. The launcher can patch only that resolved IAT slot after the software entry stop. The wrapper emits the requested exit code and caller return address, then immediately invokes the native `ExitProcess`; it does not suppress or alter the guest's termination.

Guest exceptions other than the observation breakpoint continue with `DBG_EXCEPTION_NOT_HANDLED`. Reporting an illegal instruction as handled would repeat the faulting instruction and distort its original SEH or process-exit path. The trace records the exception address and its first 16 bytes.

The exception trace also records `VirtualQueryEx` memory-region properties and return-address words above the faulting thread's ESP. This distinguishes whether a fault address belongs to image code, executable dynamic code, or a data region, and identifies guest caller candidates.
