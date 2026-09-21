# Linux x86 동적 resolver 관측 / Linux x86 dynamic resolver observation

## 근거 / Evidence

`ez2dj4th`의 기존 runtime 분석은 entry 이후 `GetProcAddress(kernel32, "GetVersion")`, 이어서 `GetProcAddress(kernel32, "CreateFileA")` 순서를 확인했습니다. Linux x86 in-process 경로는 이제 첫 `GetModuleHandleA("kernel32")` caller 복귀까지 확인됐지만 module handle은 일회성 값이며 dynamic resolver는 아직 실행하지 않습니다.

*Existing runtime analysis of `ez2dj4th` confirms the sequence `GetProcAddress(kernel32, "GetVersion")`, then `GetProcAddress(kernel32, "CreateFileA")` after entry. The Linux x86 in-process path now reaches caller return from first `GetModuleHandleA("kernel32")`, but its module handle is one-time and dynamic resolver does not yet execute.*

## 설계 / Design

Linux platform component가 `kernel32` pseudo module identity와 name-only resolver observation을 소유합니다. `GetModuleHandleA("kernel32")`는 identity를 반환하고 `GetProcAddress`는 그 identity와 ANSI name을 검사합니다. 첫 확인된 `GetVersion` request에는 caller-return breakpoint를 설치해 request와 ABI만 관측한다. resolver 결과를 실행 가능한 API thunk로 만들거나 `CreateFileA`까지 진행하는 일은 이 단위에 포함하지 않는다.

*A Linux platform component owns `kernel32` pseudo-module identity and name-only resolver observation. `GetModuleHandleA("kernel32")` returns the identity and `GetProcAddress` checks that identity plus ANSI name. The first confirmed `GetVersion` request installs caller-return breakpoint so only request and ABI are observed. Producing executable API thunks from resolver results or progressing to `CreateFileA` is outside this unit.*

## 검증 / Validation

실제 4th CHD에서 first resolver request name, caller return, SIGTRAP EIP를 확인한다. synthetic in-process probe도 유지한다.

*Verify first resolver request name, caller return, and SIGTRAP EIP against real 4th CHD. Retain synthetic in-process probe.*
