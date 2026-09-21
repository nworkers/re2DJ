# 작업 지시 330: Linux x86 in-process runner / Work order 330: Linux x86 in-process runner

Linux x86 product library에 `NativeInProcessRunner`와 필요한 PE session components를 추가합니다. runner는 caller handler를 bridge에 등록하고 TLS·entry를 실행한 뒤 result 또는 fault를 돌려줍니다. IPC backend와 Linux x64 product 구성은 변경하지 않습니다.

*Add `NativeInProcessRunner` and required PE-session components to the Linux x86 product library. The runner registers a caller handler with the bridge, runs TLS and entry, then returns a result or fault. Do not change the IPC backend or Linux x64 product configuration.*
