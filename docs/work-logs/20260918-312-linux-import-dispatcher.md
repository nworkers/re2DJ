# 작업 로그 312 — Linux import dispatcher와 x86 ABI marshalling

## 결과

공용 HLE 계층에 `ImportDispatcher`를 추가했다. dispatcher는 `{module, name 또는 ordinal, calling convention, argument count, handler}` binding을 등록하고, import gate event의 `ESP + 4`에서 little-endian 32비트 인자를 `ExecutionBackend::ReadMemory()`로 읽는다. handler가 반환한 `EAX`·`EDX`와 `__stdcall` cleanup 또는 `__cdecl` zero cleanup을 `ImportCompletion`으로 helper에 보낸다.

The shared HLE layer now contains `ImportDispatcher`. It registers `{module, name or ordinal, calling convention, argument count, handler}` bindings and reads little-endian 32-bit arguments from `ESP + 4` in an import-gate event through `ExecutionBackend::ReadMemory()`. It sends the handler's `EAX`/`EDX` result and either `__stdcall` cleanup or zero `__cdecl` cleanup to the helper as `ImportCompletion`.

binding은 비어 있지 않은 module, name/ordinal 형식, handler, calling convention과 최대 64개 인자를 검증하며 duplicate key를 거부한다. module 이름만 ASCII 대소문자를 구분하지 않고 name import는 정확히 비교한다. 등록되지 않은 import, stack-address overflow, guest memory read 실패와 handler 실패에는 completion을 보내지 않는다.

Bindings validate a nonempty module, name/ordinal form, handler, calling convention, and a maximum of 64 arguments, and reject duplicate keys. Only module names compare ASCII case-insensitively; name imports compare exactly. Unregistered imports, stack-address overflow, guest-memory read failure, and handler failure send no completion.

Linux native IPC probe의 `probe.dll!ProbeGate`와 `probe.dll!#7`은 이제 수동 stack read/write 및 completion 대신 dispatcher binding으로 처리한다. 첫 import는 41에서 EAX 42를, 두 번째 import는 42에서 EAX 43·EDX 1을 반환하며 원래 합성 process status 51까지 실행한다.

The Linux native IPC probe's `probe.dll!ProbeGate` and `probe.dll!#7` now use dispatcher bindings instead of manual stack reads/writes and completion. The first returns EAX 42 from 41, and the second returns EAX 43/EDX 1 from 42, then execution reaches the existing synthetic process status 51.

## 검증

- WSL2 Ubuntu 24.04.1에서 `bash scripts/test_linux_native_helper_probe.sh` 실행
- Linux x64 Debug build 및 CTest 1/1 통과
- Linux i386 helper build 통과
- Linux x86 Debug build 및 CTest 1/1 통과
- x64·x86 host probe가 동일 i386 helper를 통해 dispatcher completion 후 `result=51`, `child=0`을 보고
- fault probe가 signal 4를 보고
- `git diff --check` 통과

The validation ran `bash scripts/test_linux_native_helper_probe.sh` under WSL2 Ubuntu 24.04.1. Linux x64 Debug and x86 Debug CTest each passed 1/1, the Linux i386 helper built, and both host probes used the same helper and reported `result=51`, `child=0` after dispatcher completion. The fault probe reported signal 4, and `git diff --check` passed.

Windows x86 primary build도 공용 dispatcher와 unit test를 포함해 완료됐다. Windows CTest 여섯 항목 중 `re2dj_windows_vfs_runtime_probe`를 제외한 다섯 항목이 통과했다. 전체 suite는 해당 기존 probe가 45초 넘게 출력을 내지 않아 중단했으며, 이 hang은 이번 작업이 아닌 기존 `docs/TODO.md` 추적 항목이다.

Windows x86 primary build also completed with the shared dispatcher and unit test. Of six Windows CTest entries, the five tests other than `re2dj_windows_vfs_runtime_probe` passed. The full suite was interrupted while that existing probe produced no output for more than 45 seconds; its hang is already tracked in `docs/TODO.md` and is not introduced by this task.

## 범위와 다음 단계

실제 `kernel32`, USER32, DirectX, VFS, allocator, handle registry, callback, thread와 원본 HDD 실행은 추가하지 않았다. 다음 단계는 원본 import 표면과 실제 실행 증거를 기준으로 최소 Win32 API binding을 dispatcher에 등록하는 일이다. 등록되지 않은 import를 성공으로 처리하지 않는다.

No actual `kernel32`, USER32, DirectX, VFS, allocator, handle registry, callbacks, threads, or original-HDD execution was added. The next step registers the minimum Win32 API bindings in the dispatcher from original import-surface and execution evidence. Unregistered imports are never treated as successful.
