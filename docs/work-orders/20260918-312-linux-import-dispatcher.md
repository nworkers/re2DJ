# 작업 312 — Linux import dispatcher와 x86 ABI marshalling

설계: [Linux import dispatcher와 x86 ABI marshalling](../design/20260918-312-linux-import-dispatcher.md).

*Design: [Linux import dispatcher and x86 ABI marshalling](../design/20260918-312-linux-import-dispatcher.md).*

## 작업

1. 공용 HLE 공개 header/source에 import binding registry와 dispatcher를 추가합니다.
2. `ExecutionBackend` memory API로 `ESP + 4`의 little-endian dword argument를 읽고 `__stdcall`/`__cdecl` completion을 만듭니다.
3. asset-free unit test로 lookup, ABI cleanup과 오류 경계를 검증합니다.
4. Linux x64/x86 native helper probe가 dispatcher를 통해 두 import를 완료하도록 연결합니다.
5. architecture, TODO, Linux runtime analysis와 작업 로그를 갱신하고 x64/x86/helper 검증을 실행합니다.

*Work*

1. Add an import-binding registry and dispatcher in shared HLE public header/source.
2. Read little-endian dword arguments at `ESP + 4` through the `ExecutionBackend` memory API and form `__stdcall`/`__cdecl` completions.
3. Verify lookup, ABI cleanup, and error boundaries with asset-free unit tests.
4. Connect the Linux x64/x86 native-helper probe so it completes both imports through the dispatcher.
5. Update architecture, TODO, Linux runtime analysis, and the work log, then run x64/x86/helper validation.

## 검증

- Linux x64 Debug build 및 CTest
- Linux x86 Debug build 및 CTest
- Linux i386 helper build
- 공용 helper에 대한 x64·x86 native IPC probe
- `git diff --check`

*Verification*

Run Linux x64 Debug build/CTest, Linux x86 Debug build/CTest, the Linux i386 helper build, x64 and x86 native IPC probes against the same helper, and `git diff --check`.
