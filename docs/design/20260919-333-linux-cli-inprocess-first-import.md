# Linux CLI in-process 첫 import 진단 / Linux CLI in-process first-import diagnostic

`--linux-in-process-first-import`은 Linux x86 `re2dj --run`에서만 명시적으로 선택하는 진단 backend다. 기본 `--run --linux-helper` IPC 경로는 유지한다. 이 옵션은 첫 `GetModuleHandleA("kernel32")` completion 뒤 caller return breakpoint에서 멈추며 이후 API를 실행하지 않는다.

*`--linux-in-process-first-import` is a diagnostic backend selected explicitly only by Linux x86 `re2dj --run`. The default `--run --linux-helper` IPC path remains. This option stops at caller-return breakpoint after first `GetModuleHandleA("kernel32")` completion and executes no later API.*

CHD 입력은 기존 caller-owned staging PE를, directory 입력은 선택된 PE path를 사용한다. x64 host는 compatibility-mode transition이 아직 없으므로 옵션을 거부한다. 실제 CHD에서 return/EIP를 확인하고 synthetic probe regression도 같은 task에서 실행한다.

*CHD input uses existing caller-owned staging PE; directory input uses selected PE path. The x64 host rejects the option until compatibility-mode transition exists. Verify return/EIP against real CHD and run synthetic-probe regression in the same task.*
