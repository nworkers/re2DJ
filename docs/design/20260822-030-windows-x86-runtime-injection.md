# Windows x86 child runtime DLL 주입

## 설계

entry 직전 software breakpoint에서 primary thread를 suspend한 뒤 debug event를 계속하여 loader thread를 생성한다. 같은 x86 bitness인 launcher의 `kernel32!LoadLibraryW` 주소를 remote thread start address로 사용하고, DLL path를 child에 쓴다. loader thread의 exit code가 0이 아니면 x86 runtime DLL의 module base로 기록한다.

```mermaid
sequenceDiagram
    participant L as x86 launcher
    participant G as stopped original child
    participant R as remote loader thread
    L->>G: suspend primary thread
    L->>G: continue entry breakpoint
    L->>G: write absolute runtime DLL path
    L->>R: CreateRemoteThread(LoadLibraryW)
    R->>G: load re2dj runtime DLL
    R-->>L: module base exit code
    L->>G: terminate; primary entry remains unexecuted
```

이 단계는 runtime DLL의 `DllMain`이 정상 반환하고 module base를 얻는지만 검증한다. IAT를 변경하지 않고 원본 entry도 재개하지 않는다. `LoadLibraryW`의 cross-process 주소 사용은 같은 x86 bitness Windows 환경에서 이번 실험으로만 확인하며, 일반적인 module-address policy로 확정하지 않는다.

## English

At the pre-entry software breakpoint, the launcher suspends the primary thread, continues the debug event, writes the DLL path into the child, and creates a loader thread. It uses the same-bitness x86 launcher's `kernel32!LoadLibraryW` address as the remote thread start address. A nonzero loader-thread exit code is recorded as the runtime DLL module base.

```mermaid
sequenceDiagram
    participant L as x86 launcher
    participant G as stopped original child
    participant R as remote loader thread
    L->>G: suspend primary thread
    L->>G: continue entry breakpoint
    L->>G: write absolute runtime DLL path
    L->>R: CreateRemoteThread(LoadLibraryW)
    R->>G: load re2dj runtime DLL
    R-->>L: module base exit code
    L->>G: terminate; primary entry remains unexecuted
```

This task verifies only that the runtime DLL's `DllMain` returns successfully and yields a module base. It does not alter the IAT or resume original entry. Cross-process use of the `LoadLibraryW` address is tested only for this same-bitness x86 environment; it is not established as a general module-address policy.
