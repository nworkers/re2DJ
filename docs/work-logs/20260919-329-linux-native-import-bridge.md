# 작업 로그 329: Linux native import bridge 분리 / Work log 329: Linux native import bridge extraction

## 결과 / Result

`NativeImportGateBridge`와 thread-local handler configuration을 `native_import_bridge.*`로 분리했습니다. bridge는 i386 `stdcall` frame에서 원래 guest return address와 stack start를 계산하고 `NativeImportGateEvent`로 전달합니다. IPC protocol loop는 `HandleIpcImportGate` adapter가 되어 completion packet의 EAX, EDX, stack cleanup 값을 result로 돌려줍니다.

*Extracted `NativeImportGateBridge` and thread-local handler configuration into `native_import_bridge.*`. The bridge computes original guest return address and stack start from the i386 `stdcall` frame and passes them as `NativeImportGateEvent`. The IPC protocol loop is now the `HandleIpcImportGate` adapter, returning completion-packet EAX, EDX, and stack-cleanup values as a result.*

`NativePeSession`은 bridge 주소와 cleanup storage 주소만 받으며 IPC protocol state를 소유하지 않습니다. helper는 session 실행 범위에서만 IPC handler를 등록하고 scope cleanup으로 해제합니다. 따라서 후속 Linux x86 단일 프로세스 backend는 image/session과 같은 thunk ABI를 유지하면서 자체 HLE handler를 등록할 수 있습니다.

*`NativePeSession` receives only bridge and cleanup-storage addresses and owns no IPC protocol state. The helper registers the IPC handler only for the session execution scope and releases it through scope cleanup. A later Linux x86 in-process backend can therefore retain the same thunk ABI while registering its own HLE handler alongside image/session.*

## 검증 / Validation

WSL Ubuntu 24.04에서 i386 helper target을 다시 빌드했습니다. Linux x64 및 x86 host의 unit test가 각각 통과했고, 두 host 모두 shared i386 helper로 다음 결과를 확인했습니다.

*Rebuilt the i386 helper target on WSL Ubuntu 24.04. Unit tests passed for both Linux x64 and x86 hosts, and both hosts verified the following results with the shared i386 helper.*

* normal import: `result=51 child=0`
* SIGILL guest fault 전달
* pending-import terminal stop
* image 전송 전 capability rejection

* normal import: `result=51 child=0`
* SIGILL guest-fault propagation
* pending-import terminal stop
* capability rejection before image transfer

실제 `ez2dj4th`의 `GetModuleHandleA` completion과 단일 프로세스 product backend는 이 작업 범위에 포함하지 않았습니다.

*Actual `ez2dj4th` `GetModuleHandleA` completion and an in-process product backend are outside this task.*
