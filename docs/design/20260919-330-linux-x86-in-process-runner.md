# Linux x86 in-process runner / Linux x86 in-process runner

## 목적 / Purpose

Linux x86 product build에서 원본 PE32 image, import thunk, guest bootstrap, import handler를 하나의 `re2dj` process 안에 연결한다. 기존 IPC backend는 변경하지 않고 진단 fallback으로 남긴다.

*Connect original PE32 image, import thunk, guest bootstrap, and import handler inside one `re2dj` process in the Linux x86 product build. Keep the existing IPC backend unchanged as a diagnostic fallback.*

## 설계 / Design

`NativeInProcessRunner`는 caller가 제공한 PE bytes와 `PeImageInfo`, requested base, `NativeImportGateHandler`를 받는다. runner는 handler를 thread-local bridge에 등록하고 `NativePeSession`을 준비한 뒤 TLS callbacks와 entry를 실행한다. import thunk가 bridge에 도달하면 handler가 같은 thread에서 즉시 결과를 반환한다. runner는 entry result 또는 `NativeGuestFault`를 caller에게 보고하고 session 및 handler configuration을 해제한다.

*`NativeInProcessRunner` accepts caller-provided PE bytes, `PeImageInfo`, requested base, and `NativeImportGateHandler`. It registers the handler in the thread-local bridge, prepares `NativePeSession`, then runs TLS callbacks and entry. When an import thunk reaches the bridge, the handler returns a result immediately on the same thread. The runner reports entry result or `NativeGuestFault` to its caller and releases session and handler configuration.*

Linux x86 product library에만 native image/session/bridge sources를 포함한다. x64 product는 x86 compatibility-mode transition implementation이 아직 없으므로 이 runner를 빌드하거나 사용하지 않는다.

*Include native image/session/bridge sources only in the Linux x86 product library. The x64 product neither builds nor uses this runner because its x86 compatibility-mode transition implementation does not yet exist.*

## 검증 / Validation

Linux x86 product preset에서 runner source를 warnings-as-errors로 컴파일한다. synthetic PE로 direct handler invocation과 guest fault를 검증하는 probe는 다음 단위에서 runner public contract를 기준으로 추가한다.

*Compile the runner source with warnings-as-errors under the Linux x86 product preset. A probe that verifies direct handler invocation and guest fault with synthetic PE follows in the next unit against the runner public contract.*
