# 작업 로그 333: Linux CLI in-process 첫 import 진단 / Work log 333: Linux CLI in-process first-import diagnostic

## 결과 / Result

Linux x86 `re2dj --run`에 `--linux-in-process-first-import`을 추가했습니다. 이 명시적 진단 옵션은 helper IPC를 시작하지 않고 `NativeInProcessRunner`를 사용해 첫 `GetModuleHandleA("kernel32")` completion 뒤 caller return breakpoint에서 멈춥니다. 기본 `--linux-helper` IPC 경로는 변경하지 않았습니다.

*Added `--linux-in-process-first-import` to Linux x86 `re2dj --run`. This explicit diagnostic option does not start helper IPC; it uses `NativeInProcessRunner` and stops at caller-return breakpoint after first `GetModuleHandleA("kernel32")` completion. The default `--linux-helper` IPC path is unchanged.*

## 검증 / Validation

WSL Linux x86 product build와 실제 CHD 실행을 확인했습니다.

*Verified the WSL Linux x86 product build and real CHD execution.*

```text
re2dj ez2dj4th --run --linux-in-process-first-import
first completion : return 0x00ae028a, SIGTRAP EIP 0x00ae028b
linux-native-in-process-probe: imports=2 exit=51 signal=4
```

원본 CHD는 read-only였고 staging PE는 기존 caller-owned temporary path만 사용했습니다. 실제 module handle 의미와 이후 API는 여전히 미확정입니다.

*The original CHD remained read-only and staging PE used only the existing caller-owned temporary path. Real module-handle meaning and later APIs remain unresolved.*
