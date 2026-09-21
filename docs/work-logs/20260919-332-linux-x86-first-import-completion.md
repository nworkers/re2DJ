# 작업 로그 332: Linux x86 첫 import completion 관측 / Work log 332: Linux x86 first-import completion observation

## 결과 / Result

in-process probe가 optional PE file을 읽어 첫 `GetModuleHandleA("kernel32")` argument를 확인하고 EAX=1 및 4-byte cleanup을 반환하게 했습니다. handler는 caller return address에 process-local `INT3`를 두며, SIGTRAP EIP가 return address 다음 byte인지 확인합니다.

*The in-process probe now reads an optional PE file, checks first `GetModuleHandleA("kernel32")` argument, and returns EAX=1 with four-byte cleanup. Its handler places process-local `INT3` at caller return address and verifies that SIGTRAP EIP is the byte after that return address.*

## 검증 / Validation

WSL Linux x86에서 `4thTrax.chd`의 canonical PE만 `/tmp`에 materialize해 probe를 실행하고 즉시 제거했습니다. 원본 CHD와 repository는 변경하지 않았습니다.

*On WSL Linux x86, materialized only the canonical PE from `4thTrax.chd` to `/tmp`, ran the probe, then removed it immediately. The original CHD and repository were not modified.*

```text
linux-first-import-probe: return=0x00ae028a eip=0x00ae028b
```

이는 첫 import completion과 caller 복귀의 확인이며, real kernel32 module handle 또는 후속 API compatibility는 미확정입니다.

*This confirms first-import completion and caller return. Real kernel32 module handle and later API compatibility remain unresolved.*
