# 작업 로그: Windows native helper backend adapter

## 결과

Windows x64 host의 helper process 및 protocol v1 처리를 `NativeHelperBackend`로 추출했습니다. 공개 header는 PImpl을 사용해 Win32 type을 노출하지 않으며, 구현이 process/pipe handle, packet 검증, backend 상태, memory transfer, import completion과 child 종료를 소유합니다.

기존 host probe에서는 `CreateProcessW`, anonymous pipe와 protocol packet 직접 처리를 제거했습니다. probe는 synthetic PE32를 만든 뒤 `ExecutionBackend` API만 호출하여 load, start, import event, guest memory read/write, completion과 process exit을 검증합니다.

## Result

Extracted the Windows x64 host's helper-process and protocol-v1 handling into `NativeHelperBackend`. Its public header uses PImpl and exposes no Win32 types; the implementation owns process/pipe handles, packet validation, backend state, memory transfers, import completion, and child shutdown.

Removed direct `CreateProcessW`, anonymous-pipe, and protocol-packet handling from the host probe. The probe now builds synthetic PE32 and uses only the `ExecutionBackend` API to verify load, start, import event, guest-memory read/write, completion, and process exit.

## 상태와 제한

adapter는 잘못된 호출 순서를 pipe 송신 전에 거부하고 import event가 pending인 동안에만 guest memory와 completion을 허용합니다. process-exit event는 child exit code 0까지 확인한 후 성공합니다. helper의 import 처리는 아직 `probe.dll!ProbeGate` 하나로 제한되며 `LoadedPeImage.imports`는 비어 있습니다. 다음 Windows 작업은 import별 native thunk와 gate metadata입니다.

## State and limitations

The adapter rejects invalid call order before writing to the pipe and permits guest-memory operations and completion only while an import event is pending. A process-exit event succeeds only after confirming child exit code zero. Helper import handling remains limited to `probe.dll!ProbeGate`, and `LoadedPeImage.imports` is empty. The next Windows task is per-import native thunks and gate metadata.

## 검증

* Windows x64 warnings-as-errors 전체 build — 성공
* Windows x64 unit CTest — 1/1 통과
* 기존 Win32 native gate probe — 1/1 통과
* adapter 기반 `scripts/test_windows_native_ipc_probe.ps1` — 성공

통합 출력:

```text
native-ipc-host-probe: load=0x10000000 entry=0x10001000 argument=41 result=42 child=0
```

첫 x64 build에서는 backend 객체의 괄호 초기화가 함수 선언으로 해석되는 C2228 오류가 발생했습니다. 중괄호 초기화로 수정한 뒤 경고-오류 build와 전체 검증이 통과했습니다.

## Verification

The complete Windows x64 warnings-as-errors build passed, the x64 unit CTest passed 1/1, the existing Win32 native-gate probe passed 1/1, and the adapter-based integration script succeeded with the output above.

The first x64 build hit C2228 because parenthesized backend initialization was parsed as a function declaration. Switching to brace initialization fixed it, after which the warnings-as-errors build and all verification paths passed.
