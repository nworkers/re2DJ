# Linux native PE session 분리 / Linux native PE session extraction

## 목적 / Purpose

`NativePeImage`, import thunk region, import gate table, guest bootstrap을 하나의 `NativePeSession`으로 묶는다. helper IPC adapter는 pipe event와 completion만 담당하고, PE 준비·TLS·entry 실행 상태는 session이 소유한다.

*Group `NativePeImage`, import thunk region, import gate table, and guest bootstrap into one `NativePeSession`. The helper IPC adapter owns only pipe events and completion, while the session owns PE preparation, TLS, and entry execution state.*

## 계약 / Contract

`Prepare()`는 mapping, thunk binding, bootstrap 초기화를 순서대로 수행하고 어느 단계든 실패하면 session을 빈 상태로 되돌린다. `RunTlsCallbacks()`와 `RunEntry()`는 준비된 session에서만 동작한다. session 해제는 thunk region을 먼저 release하고 image를 unmap한다.

*`Prepare()` maps image, binds thunks, and initializes bootstrap in order, returning the session to empty state when any stage fails. `RunTlsCallbacks()` and `RunEntry()` operate only on prepared session. Session release drops thunk region before unmapping image.*

이 단계는 helper IPC wire format, import gate bridge, guest memory policy, HLE API binding 또는 in-process product execution을 바꾸지 않는다.

*This step does not change helper IPC wire format, import-gate bridge, guest-memory policy, HLE API binding, or in-process product execution.*
