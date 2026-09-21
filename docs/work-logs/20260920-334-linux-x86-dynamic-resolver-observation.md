# 작업 로그 334: Linux x86 동적 resolver 관측 / Work log 334: Linux x86 dynamic resolver observation

## 결과 / Result

Linux x86 in-process diagnostic은 `GetModuleHandleA("kernel32")`에 stable pseudo module identity를 반환하고, 이어지는 `GetProcAddress(identity, "GetVersion")`를 확인합니다. resolver return에는 8-byte stdcall cleanup을 적용하고 caller return address에 process-local breakpoint를 둡니다.

*Linux x86 in-process diagnostic returns stable pseudo-module identity for `GetModuleHandleA("kernel32")`, then verifies following `GetProcAddress(identity, "GetVersion")`. It applies eight-byte stdcall cleanup to resolver return and places process-local breakpoint at caller return address.*

## 검증 / Validation

WSL Linux x86에서 실제 CHD와 synthetic regression을 실행했습니다.

*Ran real CHD and synthetic regression on WSL Linux x86.*

```text
first resolver completion: GetVersion, return 0x00af0b99, SIGTRAP EIP 0x00af0b9a
linux-native-in-process-probe: imports=2 exit=51 signal=4
```

`GetVersion` resolver 결과는 실행 가능한 API thunk가 아닌 진단 값이며, `CreateFileA` 및 이후 API는 실행하지 않았습니다.

*The `GetVersion` resolver result is a diagnostic value rather than executable API thunk; `CreateFileA` and later APIs were not executed.*
