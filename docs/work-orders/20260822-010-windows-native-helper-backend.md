# 작업 지시: Windows native helper backend adapter

## 목표

Windows IPC prototype의 host transport를 `ExecutionBackend` 구현으로 추출하고 synthetic integration probe가 해당 구현을 통해서만 helper를 제어하게 합니다.

## Goal

Extract the Windows IPC prototype's host transport into an `ExecutionBackend` implementation and make the synthetic integration probe control the helper only through that implementation.

## 작업 항목

1. Windows header를 숨기는 `NativeHelperBackend` header/source pair를 추가합니다.
2. helper process와 pipe의 RAII 정리, protocol packet 송수신과 오류 변환을 구현합니다.
3. backend lifecycle 및 import-pending 상태에서만 허용되는 memory/completion 호출을 검증합니다.
4. host probe에서 직접 IPC와 process 관리 코드를 제거하고 공용 backend API를 사용합니다.
5. CMake, 아키텍처와 관련 README/포팅 계획을 갱신합니다.
6. x64 warnings-as-errors build와 unit test, 기존 x86 probe, adapter integration script를 실행합니다.
7. 결과를 대응 작업 로그에 기록하고 커밋합니다.

## Work items

1. Add a `NativeHelperBackend` header/source pair that hides Windows headers.
2. Implement RAII cleanup for the helper process and pipes, protocol packet transport, and error conversion.
3. Validate backend lifecycle and restrict memory/completion calls to an import-pending state.
4. Remove direct IPC/process management from the host probe and use the shared backend API.
5. Update CMake, architecture, related READMEs, and the porting plan.
6. Run the x64 warnings-as-errors build and unit tests, existing x86 probe, and adapter integration script.
7. Record results in the matching work log and commit the task.

## 완료 조건

integration 출력이 기존 `load=0x10000000`, `entry=0x10001000`, `argument=41`, `result=42`, `child=0`을 유지하고 host probe에 Win32 pipe/process API가 남지 않으면 완료입니다.

## Completion criteria

The task is complete when the integration output retains `load=0x10000000`, `entry=0x10001000`, `argument=41`, `result=42`, and `child=0`, and the host probe no longer contains Win32 pipe/process APIs.
