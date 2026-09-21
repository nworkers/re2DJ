# 작업 로그 330: Linux x86 in-process runner / Work log 330: Linux x86 in-process runner

## 결과 / Result

Linux x86 product library에 `NativeInProcessRunner`를 추가했습니다. runner는 PE bytes와 분석된 PE 정보, caller import handler를 받아 handler를 `NativeImportGateBridge`에 등록하고, `NativePeSession`의 TLS callbacks와 entry를 같은 process에서 실행합니다. 실행 결과는 entry exit code 또는 `NativeGuestFault`로 caller에게 돌아옵니다.

*Added `NativeInProcessRunner` to the Linux x86 product library. It accepts PE bytes, parsed PE information, and a caller import handler; registers that handler with `NativeImportGateBridge`; and runs `NativePeSession` TLS callbacks and entry in the same process. Execution returns either entry exit code or `NativeGuestFault` to the caller.*

CLI selection과 actual Win32 import binding은 바꾸지 않았습니다. Linux x64는 compatibility-mode guest transition이 없으므로 이 source set을 포함하지 않습니다.

*CLI selection and actual Win32 import binding remain unchanged. Linux x64 does not include this source set because it lacks compatibility-mode guest transition.*

## 검증 / Validation

WSL Ubuntu 24.04에서 `linux-x86-debug` preset을 `RE2DJ_WARNINGS_AS_ERRORS=ON`으로 구성했습니다. `NativeInProcessRunner`, native bridge, PE image/session, bootstrap이 Linux x86 product static library에 컴파일·링크됐고, product build와 `re2dj_unit_tests`가 통과했습니다.

*Configured the `linux-x86-debug` preset on WSL Ubuntu 24.04 with `RE2DJ_WARNINGS_AS_ERRORS=ON`. `NativeInProcessRunner`, native bridge, PE image/session, and bootstrap compiled and linked into the Linux x86 product static library; the product build and `re2dj_unit_tests` passed.*

synthetic image의 direct handler invocation과 fault를 runner public contract로 검증하는 probe는 다음 작업으로 남깁니다.

*A probe for direct handler invocation and fault through the runner public contract with a synthetic image remains the next task.*
