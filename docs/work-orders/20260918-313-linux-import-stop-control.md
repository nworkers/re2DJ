# 작업 313 — Linux import 중지 제어 계약

설계: [Linux import 중지 제어 계약](../design/20260918-313-linux-import-stop-control.md).

*Design: [Linux import stop-control contract](../design/20260918-313-linux-import-stop-control.md).*

## 작업

1. native helper backend의 pending `kStop`을 helper continuation packet이 아닌 host-side terminal stop으로 처리합니다.
2. Linux IPC probe에 stop fixture를 추가해 pending import 뒤의 terminal state와 후속 wait 거부를 검증합니다.
3. architecture, Linux runtime analysis, TODO, 플랫폼 README와 작업 로그를 갱신합니다.

*Work*

1. Treat pending `kStop` in native-helper backends as a host-side terminal stop instead of a helper continuation packet.
2. Add a Linux IPC stop fixture that verifies terminal state and rejected later wait after a pending import.
3. Update architecture, Linux runtime analysis, TODO, platform READMEs, and the work log.

## 검증

- WSL Linux x64/x86 Debug build 및 CTest
- i386 helper build와 기존 continue probe
- 새 Linux native stop probe
- `git diff --check`

*Verification*

*Run WSL Linux x64/x86 Debug build and CTest, the i386 helper build with the existing continue probe, the new Linux native stop probe, and `git diff --check`.*
