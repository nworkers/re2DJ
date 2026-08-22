# 작업 로그: Windows native helper synthetic PE32와 IPC

## 결과

Windows x64 host와 Win32 x86 helper 사이의 native execution prototype을 구현했습니다. host가 실행 시 synthetic PE32 바이트를 생성해 anonymous pipe로 보내고, helper는 Stage 2 PE header reader로 검증한 뒤 선호 주소 `0x10000000`에 mapping합니다. synthetic image의 `.text`는 `probe.dll!ProbeGate` IAT를 호출하며 helper가 해당 IAT를 in-process `__stdcall` gate로 바인딩합니다.

gate는 backend event ID, thread ID, guest return address, ESP와 gate 주소를 x64 host에 보냅니다. host는 `ESP + 4`를 IPC로 읽어 첫 인자 `41`을 확인하고 같은 값을 write protocol로 되쓴 뒤 EAX `42`, stack cleanup 4를 응답합니다. guest entry는 42를 반환하고 helper도 exit code 0으로 종료합니다.

## Result

Implemented a native-execution prototype between a Windows x64 host and Win32 x86 helper. The host builds synthetic PE32 bytes at runtime and sends them over anonymous pipes. The helper validates them with the Stage 2 PE reader and maps the image at preferred base `0x10000000`. Its `.text` calls the `probe.dll!ProbeGate` IAT, which the helper binds to an in-process `__stdcall` gate.

The gate sends backend event ID, thread ID, guest return address, ESP, and gate address to the x64 host. The host reads the first argument `41` at `ESP + 4`, writes it back through the memory-write protocol, and replies with EAX `42` and four bytes of stack cleanup. The guest entry returns 42 and the helper exits with code zero.

## Protocol v1 범위

protocol은 고정 폭 little-endian packet이며 magic, version, type, payload size를 검증합니다. `LoadImage`, `LoadResult`, `Start`, `ExecutionEvent`, `ReadMemory`, `MemoryData`, `WriteMemory`, `WriteResult`, `CompleteImport`, `Error`를 구현했습니다. 현재 gate 하나를 직렬 처리하므로 멀티스레드 병렬 event queue와 실제 backend adapter는 후속 작업입니다.

## Protocol v1 scope

The protocol uses fixed-width little-endian packets and validates magic, version, type, and payload size. It implements load, start, execution-event, memory read/write, import completion, and error messages. It currently serializes one gate; a parallel multithreaded event queue and the production backend adapter are follow-up work.

## 검증

* Windows x64 경고-오류 build — 성공
* Windows x64 unit CTest — 1/1 통과
* Win32 x86 helper 경고-오류 build — 성공
* 기존 WOW64 gate probe — 1/1 통과
* `scripts/test_windows_native_ipc_probe.ps1` — 성공

통합 출력:

```text
native-ipc-host-probe: load=0x10000000 entry=0x10001000 argument=41 result=42 child=0
```

## Verification

The Windows x64 warnings-as-errors build and unit suite passed, the Win32 helper built with warnings as errors, the original WOW64 gate probe remained green, and the x64/x86 integration script succeeded with the output shown above.
